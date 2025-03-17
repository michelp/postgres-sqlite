#include "sqlite.h"

PG_MODULE_MAGIC;

/* Callback function for freeing sqlite arrays. */
static void
sqlite_free_context_callback(void*);

/* Expanded Object Header "methods" for flattening for storage */
static Size
sqlite_get_flat_size(ExpandedObjectHeader *eohptr);

static void
sqlite_flatten_into(ExpandedObjectHeader *eohptr,
				   void *result, Size allocated_size);

static const ExpandedObjectMethods sqlite_methods = {
	sqlite_get_flat_size,
	sqlite_flatten_into
};

/* Compute flattened size of storage needed for a sqlite */
static Size
sqlite_get_flat_size(ExpandedObjectHeader *eohptr) {
	sqlite_Sqlite *db = (sqlite_Sqlite*) eohptr;
	sqlite3_int64 flat_size;

	LOGF();

	/* This is a sanity check that the object is initialized */
	Assert(db->em_magic == sqlite_MAGIC);

	/* Use cached value if already computed */
	if (db->flat_size)
	{
		return db->flat_size;
	}

	db->flat_data = sqlite3_serialize(db->db, "main", &flat_size, 0);
	if (db->flat_data == NULL)
	{
		ereport(ERROR, (errmsg("Failed to serialize sqlite db %s", sqlite3_errmsg(db->db))));
	}

	flat_size += SQLITE_OVERHEAD();

	/* Cache this value in the expanded object */
	db->flat_size = flat_size;
	return flat_size;
}

/* Flatten sqlite into a pre-allocated result buffer that is
   allocated_size in bytes.  */
static void
sqlite_flatten_into(ExpandedObjectHeader *eohptr,
				   void *result, Size allocated_size)  {
	void *data;

	/* Cast EOH pointer to expanded object, and result pointer to flat
	   object */
	sqlite_Sqlite *db = (sqlite_Sqlite *) eohptr;
	sqlite_FlatSqlite *flat = (sqlite_FlatSqlite *) result;

	LOGF();

	/* Sanity check the object is valid */
	Assert(db->em_magic == sqlite_MAGIC);
	Assert(allocated_size == db->flat_size);

	/* Zero out the whole allocated buffer */
	memset(flat, 0, allocated_size);

	/* Get the pointer to the start of the flattened data and copy the
	   expanded value into it */
	data = SQLITE_DATA(flat);
	memcpy(data, db->flat_data, db->flat_size - SQLITE_OVERHEAD());

	/* Set the size of the varlena object */
	SET_VARSIZE(flat, allocated_size);
}

/* Expand a flat sqlite in to an Expanded one, return as Postgres Datum. */
sqlite_Sqlite *
new_expanded_sqlite(sqlite_FlatSqlite *flat, MemoryContext parentcontext, sqlite3 *existing_db) {
	sqlite_Sqlite *db;
	sqlite3 *innerdb;
	size_t flat_size;
	unsigned char *flat_data;
	MemoryContext objcxt, oldcxt;
	MemoryContextCallback *ctxcb;

	LOGF();

	/* Create a new context that will hold the expanded object. */
	objcxt = AllocSetContextCreate(parentcontext,
								   "expanded sqlite",
								   ALLOCSET_DEFAULT_SIZES);

	/* Allocate a new expanded sqlite */
	db = (sqlite_Sqlite*)MemoryContextAlloc(objcxt,
											sizeof(sqlite_Sqlite));

	/* Initialize the ExpandedObjectHeader member with flattening
	 * methods and the new object context */
	EOH_init_header(&db->hdr, &sqlite_methods, objcxt);

	/* Used for debugging checks */
	db->em_magic = sqlite_MAGIC;

	/* Switch to new object context */
	oldcxt = MemoryContextSwitchTo(objcxt);

	/* Setting flat size to zero tells us the object has been written. */
	db->flat_size = 0;
	db->flat_data = NULL;

	if (existing_db != NULL)
	{
		innerdb = existing_db;
	}
	else if (sqlite3_open(":memory:", &innerdb) != SQLITE_OK)
	{
        ereport(ERROR, (errmsg("Failed to create SQLite in-memory database: %s",
							   sqlite3_errmsg(innerdb))));
	}

	if (flat != NULL)
	{
		flat_size = VARSIZE(flat) - SQLITE_OVERHEAD();
		flat_data = SQLITE_DATA(flat);
		sqlite3_deserialize(innerdb, "main", flat_data, flat_size, flat_size,
							SQLITE_DESERIALIZE_RESIZEABLE);
	}
	db->db = innerdb;

	/* Create a context callback to free sqlite when context is cleared */
	ctxcb = MemoryContextAlloc(objcxt, sizeof(MemoryContextCallback));

	ctxcb->func = sqlite_free_context_callback;
	ctxcb->arg = db;
	MemoryContextRegisterResetCallback(objcxt, ctxcb);

	/* Switch back to old context */
	MemoryContextSwitchTo(oldcxt);
	return db;
}

static void
sqlite_free_context_callback(void* ptr) {
	sqlite_Sqlite *db = (sqlite_Sqlite *) ptr;
	LOGF();
	sqlite3_close(db->db);
}

sqlite_Sqlite *
DatumGetSqlite(Datum d) {
	sqlite_Sqlite *db;
	sqlite_FlatSqlite *flat;
	LOGF();
	if (VARATT_IS_EXTERNAL_EXPANDED(DatumGetPointer(d))) {
		db = SqliteGetEOHP(d);
		Assert(db->em_magic == sqlite_MAGIC);
		return db;
	}
	flat = (sqlite_FlatSqlite*)PG_DETOAST_DATUM(d);
	db = new_expanded_sqlite(flat, CurrentMemoryContext, NULL);
	return db;
}

PG_FUNCTION_INFO_V1(sqlite_in);
Datum
sqlite_in(PG_FUNCTION_ARGS) {
    char *query = PG_GETARG_CSTRING(0);
	sqlite_Sqlite *sqlite;
    sqlite3 *db;
    char *msg = NULL;

	LOGF();

    // Initialize SQLite in-memory database
    if (sqlite3_open(":memory:", &db) != SQLITE_OK)
	{
        ereport(ERROR, (errmsg("Failed to create SQLite in-memory database: %s",
							   sqlite3_errmsg(db))));
    }

    // Execute the query
    if (sqlite3_exec(db, query, NULL, NULL, &msg) != SQLITE_OK)
	{
        ereport(ERROR, (errmsg("Failed to execute query: %s", msg)));
    }

 	sqlite = new_expanded_sqlite(NULL, CurrentMemoryContext, db);
    SQLITE_RETURN(sqlite);
}

int asi_callback(const char *str, void *sinfo);
int asi_callback(const char *str, void *sinfo)
{
	appendStringInfo(*(StringInfo*)sinfo, "%s", str);
	return 1;
}

PG_FUNCTION_INFO_V1(sqlite_out);
Datum
sqlite_out(PG_FUNCTION_ARGS)
{
	sqlite_Sqlite *db;
	StringInfo dump;

	LOGF();

	db = SQLITE_GETARG(0);
	dump = makeStringInfo();
	if (sqlite3_db_dump(db->db, "main", NULL, asi_callback, (void*)&dump) != SQLITE_OK)
	{
        ereport(ERROR, (errmsg("Failed to dump sqlite: %s",
							   sqlite3_errmsg(db->db))));
	}

    PG_RETURN_CSTRING(dump->data);
}

void
_PG_init(void)
{
	LOGF();
}

/* Local Variables: */
/* mode: c */
/* c-file-style: "postgresql" */
/* End: */
