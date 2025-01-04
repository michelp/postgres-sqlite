#include "sqlite.h"

PG_FUNCTION_INFO_V1(sqlite_deserialize);

Datum
sqlite_deserialize(PG_FUNCTION_ARGS)
{
	sqlite_Sqlite *sqlite;
	sqlite3 *db;
	bytea *input;
	char *data;
	size_t size;

	LOGF();

	input = PG_GETARG_BYTEA_P(0);
    data = VARDATA(input);
    size = VARSIZE(input) - VARHDRSZ;

    if (sqlite3_open(":memory:", &db) != SQLITE_OK)
	{
        ereport(ERROR, (errmsg("Failed to create SQLite in-memory database: %s",
							   sqlite3_errmsg(db))));
    }

    if (sqlite3_deserialize(
			db,
			"main",
			(unsigned char *)data,
			size,
			size,
			SQLITE_DESERIALIZE_RESIZEABLE
			) != SQLITE_OK)
	{
        ereport(ERROR,
				errmsg("could not deserialize SQLite database: %s", sqlite3_errmsg(db)));
	}

 	sqlite = new_expanded_sqlite(NULL, CurrentMemoryContext, db);
	SQLITE_RETURN(sqlite);
}

/* Local Variables: */
/* mode: c */
/* c-file-style: "postgresql" */
/* End: */
