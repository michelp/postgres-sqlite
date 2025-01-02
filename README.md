# Sqlite PostgreSQL Native Type

`sqlite` is a PostgreSQL extension that adds sqlite databases as a
first class in memory data type for sqlite databases.

Using a combination of the Postgres "exapnded datum" API and the
sqlite serialize/deserialize API, sqlite can be used to create and
store small (up to 1GB) sqlite databases inline with Postgres data.

# Creating SQLite Databases

The extension can be installed into a Postgres in the normal way:

```
CREATE EXTENSION sqlite;
```

There are only three objects in the extension, the `sqlite` type and
the `sqlite_exec()` and `sqlite_query()` functions.  New databases can
be created by casting an initialization string to the `sqlite` type:

```
  SELECT 'CREATE TABLE foo (bar text)'::sqlite;
```

An empty database can be created with an empty string like
`''::sqlite`.

# Modifying SQLite Objects

This new sqlite instance can now be inserted into a table and
manipulated with the `sqlite()` function, for example:

```
CREATE TABLE customer (
    id bigint PRIMARY KEY,
    name text NOT NULL,
    data sqlite DEFAULT 'CREATE TABLE foo (bar text);'
    );
    
INSERT INTO customer_data (name) VALUES ('bob');
UPDATE customer_data SET data = sqlite_exec(data, $$INSERT INTO foo (bar) VALUES ('bing')$$);
```

Notice how the `DEFAULT` value for the sqlite column in the new table
will initialize a new sqlite database into that column.  All new rows
in `customer` will contain a `foo` table if no default overriding
value is provided on insert as shown above.

The `sqlite_exec(db, query)` function takes a sqlite database and a
query as an argument, executes that query and returns the same
database.

# Querying SQLite Objects

The `sqlite_query(db, query)` function is a Set Returning Function
(SRF) that returns a record for each sqlite result row from the given
query:

```
SELECT * from sqlite_query(
    (SELECT data FROM customer), 
    'SELECT * from foo');
```

Because `sqlite_query()` returns a record per row, that record can be
casted to named and typed columns using the standard SQL `AS` syntax:

```
SELECT the_bar FROM sqlite_query(
    (SELECT data FROM customer), 
    'SELECT bar from foo') 
AS (the_bar text);
```

# How it Works

Most Postgres data types, like numbers and text, are "flat" and have
identical in-memory and on-disk representations.  Postgres simply
loads those from disk into memory as is, and copies those datum around
during the course of a SQL query.

Some data types however, like a sqlite database, are more efficiently
represented in memory differently than their on-disk representation.
The act of copying flat datum around also can cost a lot of cycles and
memory for large, flat variable length objects.

Postgres has an API called the [Expanded Datum
API](https://www.postgresql.org/docs/current/xtypes.html#XTYPES-TOAST)
that will "expand" a flat value into a more efficient in-memory
representation (in this case, a live in-memory sqlite db) and
"flatten" the expanded object when it's time to write it to the
database. Another upside of this is by "expanding" a serialized "flat"
sqlite database into an in-memory object, this datum copying just
copies a single pointer to the database around during a sql command
instead of the entire flattened object.

# Is this related to pglite-fusion?

`pglite-fusion` i an extension that acomplishes similar usability
goals.  It is IMO a great idea, obviously since I have implemented the
same idea here, but the implementation has some issues:

  - It does not use the expanded datum API or the sqlite3
    serialize/deserialize API but rather reads/write a temp file on the
    server filesystem for every database operation.  This is not only
    inefficient but also a big no-no for cloud hosting providers.
  
  - Because it does not read/write sqlite data into the Postgres
    database directly, but rather into server filesystem tempfiles,
    the data is not WAL logged and disconnected from the same data
    visibility rules as Postgres.
    
  - Querying it does not return a set of record rows, but rather a
    JSON formatted row.
    
  - It provides several functions for accessing type specific column
    values from a query, where this extension uses a Set Returning
    Function that adapts each returned record tuple to be the exact
    schema of the sqlite query.  This automatically type casts result
    values to their correct Postgres type.

  - pglite-fusion is implemented in Rust, but the expanded datum api
    and the sqlite serialization API are straight, simple C functions
    passing serialization pointers to each other.  Maybe the same
    solution can be done in Rust, but I had no need of that
    hypothesis.
