#ifndef SQLITE_H
#define SQLITE_H

#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "utils/expandeddatum.h"
#include "utils/lsyscache.h"
#include "utils/builtins.h"
#include "lib/stringinfo.h"

#include <sqlite3.h>

/* ID for debugging crosschecks */
#define sqlite_MAGIC 889276513

#define LOGF() elog(DEBUG1, __func__)

/* Flattened representation of sqlite, used to store to disk.

   The first 32 bits must the length of the data.  Actual flattened data
   is appended after this struct and cannot exceed 1GB.
*/
typedef struct sqlite_FlatSqlite {
	int32 vl_len_;
} sqlite_FlatSqlite;

/* Expanded representation of sqlite.

   When loaded from storage, the flattened representation is used to
   build the sqlite.  In this case, it's just a pointer to an integer.
*/
typedef struct sqlite_Sqlite  {
	ExpandedObjectHeader hdr;
	int em_magic;
	sqlite3 *db;
	Size flat_size;
	unsigned char *flat_data;
} sqlite_Sqlite;

/* Create a new sqlite datum. */
sqlite_Sqlite *
new_expanded_sqlite(sqlite_FlatSqlite* flat, MemoryContext parentcontext, sqlite3 *existing_db);

int sqlite3_db_dump(
	sqlite3 *db,
	const char *zSchema,
	const char *zTable,
	int (*xCallback)(const char*,void*),
	void *pArg
	);

/* Helper function that either detoasts or expands. */
sqlite_Sqlite *DatumGetSqlite(Datum d);

/* Helper macro to detoast and expand sqlites arguments */
#define SQLITE_GETARG(n)  DatumGetSqlite(PG_GETARG_DATUM(n))

/* Helper macro to return Expanded Object Header Pointer from sqlite. */
#define SQLITE_RETURN(A) return EOHPGetRWDatum(&(A)->hdr)

/* Helper macro to compute flat sqlite header size */
#define SQLITE_OVERHEAD() MAXALIGN(sizeof(sqlite_FlatSqlite))

/* Helper macro to get pointer to beginning of sqlite data. */
#define SQLITE_DATA(a) (((unsigned char *) (a)) + SQLITE_OVERHEAD())

/* Help macro to cast generic Datum header pointer to expanded Sqlite */
#define SqliteGetEOHP(d) (sqlite_Sqlite *) DatumGetEOHP(d);

/* Public API functions */

void
_PG_init(void);

#endif /* SQLITE_H */

/* Local Variables: */
/* mode: c */
/* c-file-style: "postgresql" */
/* End: */
