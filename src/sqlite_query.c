#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "utils/lsyscache.h"
#include "executor/spi.h"
#include <sqlite3.h>

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(sqlite_to_postgres);

typedef struct {
    sqlite3 *db;
    sqlite3_stmt *stmt;
} SqliteQueryState;

Datum
sqlite_to_postgres(PG_FUNCTION_ARGS) {
    FuncCallContext *funcctx;
    SqliteQueryState *query_state;

    if (SRF_IS_FIRSTCALL()) {
        MemoryContext oldcontext;
        funcctx = SRF_FIRSTCALL_INIT();

        // Switch memory context
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

        // Initialize SQLite connection
        query_state = (SqliteQueryState *) palloc(sizeof(SqliteQueryState));
        if (sqlite3_open("your_sqlite.db", &query_state->db) != SQLITE_OK)
            ereport(ERROR, (errmsg("Failed to open SQLite database")));

        // Prepare the SQLite query
        const char *query = text_to_cstring(PG_GETARG_TEXT_PP(0));
        if (sqlite3_prepare_v2(query_state->db, query, -1, &query_state->stmt, NULL) != SQLITE_OK)
            ereport(ERROR, (errmsg("Failed to prepare SQLite query: %s", sqlite3_errmsg(query_state->db))));

        funcctx->user_fctx = query_state;

        // Define the tuple descriptor for PostgreSQL
        TupleDesc tupdesc = CreateTemplateTupleDesc(sqlite3_column_count(query_state->stmt), false);
        for (int i = 0; i < sqlite3_column_count(query_state->stmt); i++) {
            const char *colname = sqlite3_column_name(query_state->stmt, i);
            Oid coltype = TEXTOID; // Default to TEXT
            switch (sqlite3_column_type(query_state->stmt, i)) {
                case SQLITE_INTEGER: coltype = INT4OID; break;
                case SQLITE_FLOAT: coltype = FLOAT8OID; break;
                case SQLITE_BLOB: coltype = BYTEAOID; break;
                case SQLITE_TEXT: coltype = TEXTOID; break;
                default: coltype = TEXTOID; break;
            }
            TupleDescInitEntry(tupdesc, i + 1, colname, coltype, -1, 0);
        }
        BlessTupleDesc(tupdesc);
        funcctx->tuple_desc = tupdesc;

        MemoryContextSwitchTo(oldcontext);
    }

    funcctx = SRF_PERCALL_SETUP();
    query_state = (SqliteQueryState *) funcctx->user_fctx;

    // Fetch the next SQLite row
    if (sqlite3_step(query_state->stmt) == SQLITE_ROW) {
        Datum *values = palloc0(sqlite3_column_count(query_state->stmt) * sizeof(Datum));
        bool *nulls = palloc0(sqlite3_column_count(query_state->stmt) * sizeof(bool));

        for (int i = 0; i < sqlite3_column_count(query_state->stmt); i++) {
            if (sqlite3_column_type(query_state->stmt, i) == SQLITE_NULL) {
                nulls[i] = true;
                values[i] = (Datum) 0;
            } else {
                switch (sqlite3_column_type(query_state->stmt, i)) {
                    case SQLITE_INTEGER:
                        values[i] = Int32GetDatum(sqlite3_column_int(query_state->stmt, i));
                        break;
                    case SQLITE_FLOAT:
                        values[i] = Float8GetDatum(sqlite3_column_double(query_state->stmt, i));
                        break;
                    case SQLITE_TEXT:
                        values[i] = CStringGetTextDatum((const char *) sqlite3_column_text(query_state->stmt, i));
                        break;
                    case SQLITE_BLOB:
                        values[i] = PointerGetDatum(PG_DETOAST_DATUM_COPY(sqlite3_column_blob(query_state->stmt, i)));
                        break;
                    default:
                        nulls[i] = true;
                        break;
                }
            }
        }

        TupleDesc tupdesc = funcctx->tuple_desc;
        HeapTuple tuple = heap_form_tuple(tupdesc, values, nulls);
        SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
    } else {
        // Clean up SQLite resources
        sqlite3_finalize(query_state->stmt);
        sqlite3_close(query_state->db);
        SRF_RETURN_DONE(funcctx);
    }
}
