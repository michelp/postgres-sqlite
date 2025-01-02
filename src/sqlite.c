#include "sqlite.h"
PG_MODULE_MAGIC;

/* Compute flattened size of storage needed for a sqlite */
static Size
sqlite_get_flat_size(ExpandedObjectHeader *eohptr) {
	sqlite_Sqlite *A = (sqlite_Sqlite*) eohptr;
	Size nbytes;

	LOGF();

	/* This is a sanity check that the object is initialized */
	Assert(A->em_magic == sqlite_MAGIC);

	/* Use cached value if already computed */
	if (A->flat_size) {
		return A->flat_size;
	}

	// Add the overhead of the flat header to the size of the data
	// payload
	nbytes = SQLITE_OVERHEAD();
	nbytes += sizeof(uint64_t);

	/* Cache this value in the expanded object */
	A->flat_size = nbytes;
	return nbytes;
}

/* Flatten sqlite into a pre-allocated result buffer that is
   allocated_size in bytes.  */
static void
sqlite_flatten_into(ExpandedObjectHeader *eohptr,
				   void *result, Size allocated_size)  {
	void *data;

	/* Cast EOH pointer to expanded object, and result pointer to flat
	   object */
	sqlite_Sqlite *A = (sqlite_Sqlite *) eohptr;
	sqlite_FlatSqlite *flat = (sqlite_FlatSqlite *) result;

	LOGF();

	/* Sanity check the object is valid */
	Assert(A->em_magic == sqlite_MAGIC);
	Assert(allocated_size == A->flat_size);

	/* Zero out the whole allocated buffer */
	memset(flat, 0, allocated_size);

	/* Get the pointer to the start of the flattened data and copy the
	   expanded value into it */
	data = SQLITE_DATA(flat);

	/* Set the size of the varlena object */
	SET_VARSIZE(flat, allocated_size);
}

/* Expand a flat sqlite in to an Expanded one, return as Postgres Datum. */
sqlite_Sqlite *
new_expanded_sqlite(int64_t value, MemoryContext parentcontext) {
	sqlite_Sqlite *db;

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

	/* Create a context callback to free sqlite when context is cleared */
	ctxcb = MemoryContextAlloc(objcxt, sizeof(MemoryContextCallback));

	ctxcb->func = context_callback_sqlite_free;
	ctxcb->arg = db;
	MemoryContextRegisterResetCallback(objcxt, ctxcb);

	/* Switch back to old context */
	MemoryContextSwitchTo(oldcxt);
	return db;
}

/* MemoryContextCallback function to free sqlite data when their
   context goes out of scope. */
static void
context_callback_sqlite_free(void* ptr) {
	sqlite_Sqlite *db = (sqlite_Sqlite *) ptr;
	LOGF();
}

/* Helper function to always expanded datum

   This is used by PG_GETARG_SQLITE */
sqlite_Sqlite *
DatumGetSqlite(Datum d) {
	sqlite_Sqlite *db;
	sqlite_FlatSqlite *flat;
	LOGF();
	if (VARATT_IS_EXTERNAL_EXPANDED(DatumGetPointer(d))) {
		A = SqliteGetEOHP(d);
		Assert(db->em_magic == sqlite_MAGIC);
		return db;
	}
	flat = (sqlite_FlatSqlite*)PG_DETOAST_DATUM(d);
	db = new_expanded_sqlite(flat, CurrentMemoryContext);
	return A;
}

Datum
sqlite_in(PG_FUNCTION_ARGS) {
	char *input;
	sqlite_Sqlite *result;
	LOGF();
	input = PG_GETARG_CSTRING(0);
	result = new_expanded_sqlite(NULL, CurrentMemoryContext);
	SQLITE_RETURN_SQLITE(result);
}


sqlite_in(PG_FUNCTION_ARGS) {
    char *query = PG_GETARG_CSTRING(0);
	sqlite_Sqlite *sqlite;
    sqlite3 *db;
    char *errmsg = NULL;

    // Initialize SQLite in-memory database
    if (sqlite3_open(":memory:", &db) != SQLITE_OK) {
        ereport(ERROR, (errmsg("Failed to create SQLite in-memory database: %s", sqlite3_errmsg(db))));
    }

    // Execute the query
    if (sqlite3_exec(db, query, NULL, NULL, &errmsg) != SQLITE_OK) {
        sqlite3_close(db);
        ereport(ERROR, (errmsg("Failed to execute query: %s", errmsg)));
    }
 	sqlite = new_expanded_sqlite(NULL, CurrentMemoryContext, db);
    SQLITE_RETURN(sqlite);
}

Datum
sqlite_out(PG_FUNCTION_ARGS)
{
	char *result = "42";
	sqlite_Sqlite *A = SQLITE_GETARG_SQLITE(0);
	LOGF();
	PG_RETURN_CSTRING(result);
}

Datum
sqlite_info(PG_FUNCTION_ARGS) {
	sqlite_Sqlite *A;
	LOGF();
	A = SQLITE_GETARG_SQLITE(0);
	return Int64GetDatum(*A->value);
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
