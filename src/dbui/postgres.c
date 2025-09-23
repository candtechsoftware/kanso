#include "postgres.h"
#include <unistd.h>
#include <stdio.h>

DB_Handle conn_to_handle(PGconn *conn) {
    DB_Handle handle = {0};
    handle.ptr = (void *)conn;
    return handle;
}

static inline PGconn *handle_to_conn(DB_Handle handle) {
    PGconn *conn = (PGconn *)handle.ptr;
    return conn;
}

internal DB_Conn *
pg_connect(DB_Config config) {
    Arena   *arena = arena_alloc();
    DB_Conn *db_conn = push_struct(arena, DB_Conn);
    db_conn->arena = arena;

    PGconn *conn = PQconnectdb(str_to_cstring(arena, config.connection_string));
    if (PQstatus(conn) == CONNECTION_BAD) {
        log_error("Could not connect to postgres db");
        return 0;
    }

    PostgresPollingStatusType res = PQconnectPoll(conn);

    u64 timeout_secs = 5;
    u64 poll_count = 0;
    u64 max_poll = timeout_secs * 100;

    while (res != PGRES_POLLING_OK && res != PGRES_POLLING_FAILED) {
        poll_count += 1;
        if (poll_count > max_poll) {
            log_error("Connection timeout after {d} seconds", timeout_secs);
            return 0;
        }
        sleep(10);
        res = PQconnectPoll(conn);
    }

    if (res == PGRES_POLLING_FAILED) {
        log_error("Connection failed after {d} seconds", timeout_secs);
        return 0;
    }
    db_conn->handle = conn_to_handle(conn);
    return db_conn;
}

internal void
pg_disconnet(DB_Conn *conn) {
    if (conn) {
        PQfinish((PGconn *)conn->handle.ptr);
        arena_release(conn->arena);
    }
}

internal DB_Schema_List pg_get_all_schemas(DB_Conn *conn) {
    PROF_FUNCTION;
    PGconn        *c = handle_to_conn(conn->handle);
    DB_Schema_List list = {0};
    const char    *query =
        " SELECT 'table'::text AS type, schemaname::text AS schema, tablename::text AS name\n"
        " FROM pg_tables\n"
        " WHERE schemaname NOT IN ('pg_catalog', 'information_schema')\n"
        " UNION ALL\n"
        " SELECT 'function'::text AS type, n.nspname::text AS schema, p.proname::text AS name\n"
        " FROM pg_proc p\n"
        " JOIN pg_namespace n ON n.oid = p.pronamespace\n"
        " WHERE n.nspname NOT IN ('pg_catalog', 'information_schema')\n"
        " ORDER BY type, schema, name;\n";

    PGresult *res = PQexecParams(
        c,
        query,
        0,
        NULL,
        NULL,
        NULL,
        NULL,
        0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        log_error("Query Failed {s}\n", query);
        PQclear(res);
        PQfinish(c);
        Prof_End();
        return list;
    }

    s64 n_rows = PQntuples(res);
    s64 n_fields = PQnfields(res);

    for (s32 r = 0; r < n_rows; r++) {
        DB_Schema_Node *node = push_array(conn->arena, DB_Schema_Node, 1);

        char *kind_str = PQgetvalue(res, r, 0);
        char *schema_str = PQgetvalue(res, r, 1);
        char *name_str = PQgetvalue(res, r, 2);

        String kind_temp = cstr_to_string(kind_str, strlen(kind_str));
        String schema_temp = cstr_to_string(schema_str, strlen(schema_str));
        String name_temp = cstr_to_string(name_str, strlen(name_str));

        node->v.kind = str_push_copy(conn->arena, kind_temp);
        node->v.schema = str_push_copy(conn->arena, schema_temp);
        node->v.name = str_push_copy(conn->arena, name_temp);

        DLLPushBack_NPZ(0, list.first, list.last, node, next, prev);
        list.count++;
    }
    Prof_End();
    return list;
}

internal DB_Table *pg_get_schema_info(DB_Conn *conn, DB_Schema schema) {
    PROF_FUNCTION;
    PGconn *c = handle_to_conn(conn->handle);

    Arena    *arena = arena_alloc();
    DB_Table *table = push_struct_zero(arena, DB_Table);
    table->arena = arena;
    table->schema = schema;

    table->columns = (Dyn_Array){0};
    table->rows = (Dyn_Array){0};

    char        query_buffer[4096];
    const char *schema_name_cstr = str_to_cstring(arena, schema.schema);
    const char *table_name_cstr = str_to_cstring(arena, schema.name);

    snprintf(query_buffer, sizeof(query_buffer),
             "SELECT "
             "    c.column_name, "
             "    c.data_type, "
             "    c.is_nullable, "
             "    c.column_default, "
             "    CASE WHEN fk.column_name IS NOT NULL THEN 'YES' ELSE 'NO' END AS is_foreign_key, "
             "    COALESCE(fk.foreign_table_name, '') AS foreign_table_name, "
             "    COALESCE(fk.foreign_column_name, '') AS foreign_column_name "
             "FROM information_schema.columns c "
             "LEFT JOIN ( "
             "    SELECT "
             "        kcu.column_name, "
             "        ccu.table_name AS foreign_table_name, "
             "        ccu.column_name AS foreign_column_name "
             "    FROM information_schema.table_constraints tc "
             "    JOIN information_schema.key_column_usage kcu "
             "        ON tc.constraint_name = kcu.constraint_name "
             "        AND tc.table_schema = kcu.table_schema "
             "    JOIN information_schema.constraint_column_usage ccu "
             "        ON ccu.constraint_name = tc.constraint_name "
             "        AND ccu.table_schema = tc.table_schema "
             "    WHERE tc.constraint_type = 'FOREIGN KEY' "
             "        AND tc.table_name = '%s' "
             ") fk ON c.column_name = fk.column_name "
             "WHERE c.table_name = '%s' "
             "ORDER BY c.ordinal_position",
             table_name_cstr, table_name_cstr);

    PGresult *res = PQexec(c, query_buffer);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        const char *error_msg = PQerrorMessage(c);
        log_error("Failed to get schema info: {s}", error_msg);
        PQclear(res);
        arena_release(arena);
        Prof_End();
        return 0;
    }

    s32 n_rows = PQntuples(res);
    table->column_count = n_rows;

    for (s32 r = 0; r < n_rows; r++) {
        DB_Column_Info *col = dyn_array_push(arena, &table->columns, DB_Column_Info);
        if (!col) {
            log_error("Failed to allocate column info");
            continue;
        }

        char *col_name = PQgetvalue(res, r, 0);
        char *data_type = PQgetvalue(res, r, 1);
        char *nullable = PQgetvalue(res, r, 2);
        char *default_val = PQgetvalue(res, r, 3);
        char *is_fk = PQgetvalue(res, r, 4);
        char *fk_table = PQgetvalue(res, r, 5);
        char *fk_column = PQgetvalue(res, r, 6);

        *col = (DB_Column_Info){0};

        col->column_name = str_push_copy(arena, cstr_to_string(col_name, strlen(col_name)));
        col->data_type = str_push_copy(arena, cstr_to_string(data_type, strlen(data_type)));
        col->is_nullable = str_push_copy(arena, cstr_to_string(nullable, strlen(nullable)));
        col->column_default = str_push_copy(arena, cstr_to_string(default_val, strlen(default_val)));
        col->is_foreign_key = str_push_copy(arena, cstr_to_string(is_fk, strlen(is_fk)));
        col->foreign_table_name = str_push_copy(arena, cstr_to_string(fk_table, strlen(fk_table)));
        col->foreign_column_name = str_push_copy(arena, cstr_to_string(fk_column, strlen(fk_column)));

        // Cache display strings for fast rendering
        char display_buf[512];
        snprintf(display_buf, sizeof(display_buf), "%s: %s", col_name, data_type);
        u64 display_len = strlen(display_buf);
        col->display_text = push_array(arena, char, display_len + 1);
        MemoryCopy(col->display_text, display_buf, display_len + 1);

        // Cache foreign key check
        col->is_fk = (strcmp(is_fk, "YES") == 0);

        if (col->is_fk) {
            char fk_buf[256];
            snprintf(fk_buf, sizeof(fk_buf), "  → %s", fk_table);
            u64 fk_len = strlen(fk_buf);
            col->fk_display = push_array(arena, char, fk_len + 1);
            MemoryCopy(col->fk_display, fk_buf, fk_len + 1);
        } else {
            col->fk_display = NULL;
        }
    }

    PQclear(res);
    Prof_End();
    return table;
}

internal DB_Table *pg_get_data_from_schema(DB_Conn *conn, DB_Schema schema, u32 limit) {
    PROF_FUNCTION;
    PGconn *c = handle_to_conn(conn->handle);

    Arena    *arena = arena_alloc();
    DB_Table *table = push_struct_zero(arena, DB_Table);
    table->arena = arena;
    table->schema = schema;

    table->columns = (Dyn_Array){0};
    table->rows = (Dyn_Array){0};

    char        table_name[512];
    const char *schema_cstr = str_to_cstring(arena, schema.schema);
    const char *name_cstr = str_to_cstring(arena, schema.name);

    if (schema.schema.size > 0 && !str_match(schema.schema, str_lit("public"))) {
        snprintf(table_name, sizeof(table_name), "%s.%s", schema_cstr, name_cstr);
    } else {
        snprintf(table_name, sizeof(table_name), "%s", name_cstr);
    }

    char query_buffer[1024];
    snprintf(query_buffer, sizeof(query_buffer), "SELECT * FROM %s LIMIT %u", table_name, limit);

    PGresult *res = PQexec(c, query_buffer);

    if (!res) {
        const char *error_msg = PQerrorMessage(c);
        if (error_msg) {
            log_error("PostgreSQL error: {S}", cstr_to_string(error_msg, strlen(error_msg)));
        }
        arena_release(arena);
        return 0;
    }

    ExecStatusType status = PQresultStatus(res);
    if (status != PGRES_TUPLES_OK) {
        const char *error_msg = PQresultErrorMessage(res);
        if (error_msg) {
            log_error("Query error: {S}", cstr_to_string(error_msg, strlen(error_msg)));
        }
        PQclear(res);
        arena_release(arena);
        Prof_End();
        return 0;
    }

    table->row_count = PQntuples(res);
    table->column_count = PQnfields(res);

    for (u64 c = 0; c < table->column_count; c++) {
        DB_Column_Info *col = dyn_array_push(arena, &table->columns, DB_Column_Info);
        const char     *col_name = PQfname(res, (int)c);
        col->column_name = str_push_copy(arena, cstr_to_string(col_name, strlen(col_name)));

        // For data display, we don't have type info, but we can still cache the name
        col->display_text = push_array(arena, char, strlen(col_name) + 1);
        MemoryCopy(col->display_text, col_name, strlen(col_name) + 1);
        col->is_fk = false;
        col->fk_display = NULL;
    }

    for (u64 r = 0; r < table->row_count; r++) {
        DB_Row *row = dyn_array_push(arena, &table->rows, DB_Row);

        for (u64 c = 0; c < table->column_count; c++) {
            char   *value_str = PQgetvalue(res, (int)r, (int)c);
            String  value = cstr_to_string(value_str, strlen(value_str));
            String *stored_value = dyn_array_push(arena, &row->values, String);
            *stored_value = str_push_copy(arena, value);
        }
    }

    PQclear(res);
    Prof_End();
    return table;
}
