#include "sqlite.h"

PG_FUNCTION_INFO_V1(sqlite_query);

typedef struct {
    sqlite3 *db;
    sqlite3_stmt *stmt;
} SqliteQueryState;

Datum
sqlite_query(PG_FUNCTION_ARGS) {
    FuncCallContext *funcctx;
    SqliteQueryState *query_state;
    const char *query;
    TupleDesc tupdesc;
    HeapTuple tuple;
    int column_count;
    int column_type;

    LOGF();

    if (SRF_IS_FIRSTCALL()) {
        MemoryContext oldcontext;
        funcctx = SRF_FIRSTCALL_INIT();

        // Switch memory context
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

        // Initialize SQLite connection
        query_state = (SqliteQueryState *) palloc(sizeof(SqliteQueryState));

        // Prepare the SQLite query
        query_state->db = SQLITE_GETARG(0)->db;
        query = text_to_cstring(PG_GETARG_TEXT_PP(1));

        if (sqlite3_prepare_v2(query_state->db, query, -1, &query_state->stmt, NULL) != SQLITE_OK)
            ereport(ERROR, (errmsg("Failed to prepare SQLite query: %s", sqlite3_errmsg(query_state->db))));

        funcctx->user_fctx = query_state;

        column_count = sqlite3_column_count(query_state->stmt);

		if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("function returning record called in context "
							"that cannot accept type record")));

        BlessTupleDesc(tupdesc);
        funcctx->tuple_desc = tupdesc;
        MemoryContextSwitchTo(oldcontext);
    }

    funcctx = SRF_PERCALL_SETUP();
    query_state = (SqliteQueryState *) funcctx->user_fctx;
    column_count = sqlite3_column_count(query_state->stmt);

    // Fetch the next SQLite row
    if (sqlite3_step(query_state->stmt) == SQLITE_ROW) {
        Datum *values = palloc0(column_count * sizeof(Datum));
        bool *nulls = palloc0(column_count * sizeof(bool));

        for (int i = 0; i < column_count; i++) {
            column_type = sqlite3_column_type(query_state->stmt, i);
            switch (column_type) {
                case SQLITE_INTEGER:
                    values[i] = Int32GetDatum(sqlite3_column_int(query_state->stmt, i));
                    break;
                case SQLITE_FLOAT:
                    values[i] = Float8GetDatum(sqlite3_column_double(query_state->stmt, i));
                    break;
                case SQLITE_TEXT:
                    values[i] = CStringGetTextDatum((const char *) sqlite3_column_text(query_state->stmt, i));
                    break;
                default:
                    nulls[i] = true;
                    break;
            }
        }
        tupdesc = funcctx->tuple_desc;
        tuple = heap_form_tuple(tupdesc, values, nulls);
        SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
    } else {
        // Clean up SQLite resources
        sqlite3_finalize(query_state->stmt);
        SRF_RETURN_DONE(funcctx);
    }
}

/* Local Variables: */
/* mode: c */
/* c-file-style: "postgresql" */
/* End: */
