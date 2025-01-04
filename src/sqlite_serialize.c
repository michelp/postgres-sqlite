#include "sqlite.h"

PG_FUNCTION_INFO_V1(sqlite_serialize);

Datum
sqlite_serialize(PG_FUNCTION_ARGS)
{
	sqlite_Sqlite *sqlite;
	void *data;
	sqlite3_int64 size;
	bytea *result;

	sqlite = SQLITE_GETARG(0);

	data = sqlite3_serialize(sqlite->db, "main", &size, 0);
	if (data == NULL)
	{
		ereport(ERROR, (errmsg("Failed to serialize sqlite db %s", sqlite3_errmsg(sqlite->db))));
	}
	result = (bytea *)palloc(size + VARHDRSZ);
    SET_VARSIZE(result, size + VARHDRSZ);
    memcpy(VARDATA(result), data, size);
    sqlite3_free(data);
    PG_RETURN_BYTEA_P(result);

}

/* Local Variables: */
/* mode: c */
/* c-file-style: "postgresql" */
/* End: */
