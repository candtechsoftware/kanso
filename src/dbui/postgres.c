#include "postgres.h"
#include <unistd.h>

DB_Handle conn_to_handle(PGconn *conn) {
    DB_Handle handle = {0};
    handle.ptr = (void *)conn;
    return handle;
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
