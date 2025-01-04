#include "sqlite.h"

PG_FUNCTION_INFO_V1(sqlite_exec);

Datum
sqlite_exec(PG_FUNCTION_ARGS)
{
	sqlite_Sqlite *sqlite;
    text *query;
    char *msg = NULL;
	LOGF();
	sqlite = SQLITE_GETARG(0);
	query = PG_GETARG_TEXT_PP(1);

    // Execute the query
    if (sqlite3_exec(sqlite->db, text_to_cstring(query), NULL, NULL, &msg) != SQLITE_OK)
	{
        ereport(ERROR, (errmsg("Failed to execute query: %s", msg)));
    }
    SQLITE_RETURN(sqlite);

}

/* Local Variables: */
/* mode: c */
/* c-file-style: "postgresql" */
/* End: */
