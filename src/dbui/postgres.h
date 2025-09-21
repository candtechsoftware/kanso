#pragma once

#include <libpq-fe.h>
#include "dbui.h"


internal DB_Conn       *pg_connect(DB_Config config);
internal DB_Schema_List pg_get_all_schemas(DB_Conn *conn);
internal DB_Table      *pg_get_schema_info(DB_Conn *conn, DB_Schema schema);
internal DB_Table      *pg_get_data_from_schema(DB_Conn *conn, DB_Schema schema, u32 limit);
