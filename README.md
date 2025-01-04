# postgres-sqlite

`postgres-sqlite` is a PostgreSQL extension that introduces SQLite
databases as a first-class data type within PostgreSQL.

By leveraging the PostgreSQL "expanded datum" API and SQLite's
`sqlite3_serialize`/`sqlite3_deserialize` APIs, `postgres-sqlite`
enables the efficient creation and storage of small SQLite databases
(up to 1GB) directly within PostgreSQL tables. This efficiency is
achieved through PostgreSQL's compressed TOAST storage in conjunction
with SQLite's native serialization API. Importantly, `postgres-sqlite`
does not require filesystem access—SQLite databases are fully managed
within PostgreSQL tables.

### Key Advantages

#### Multitenancy
`postgres-sqlite` simplifies multitenancy by embedding isolated SQLite
databases directly into PostgreSQL. Each SQLite database is entirely
independent from its surrounding PostgreSQL environment, eliminating
the need for complex RLS policies, permissions, or other traditional
multitenancy mechanisms. This allows customer data to be securely
stored in separate rows without additional configuration.

#### Client-Server Synchronization
Another powerful use case is pairing `postgres-sqlite` with the
official SQLite Wasm (WebAssembly) build to synchronize client-side
and server-side data. You can seamlessly exchange data between client
and server using either SQLite’s native SQL text format or the
standard binary format via `sqlite_serialize()` and
`sqlite_deserialize()`.

## Creating SQLite Databases

The extension can be installed into a Postgres in the normal way:

```
CREATE EXTENSION sqlite;
```

There are three main objects in the extension, the `sqlite` type and
the `sqlite_exec()` and `sqlite_query()` functions.  New databases can
be created by casting an initialization string to the `sqlite` type:

```
SELECT 'CREATE TABLE user_config (key text, value text)'::sqlite;
┌──────────────────────────────────────────────────┐
│                      sqlite                      │
├──────────────────────────────────────────────────┤
│ PRAGMA foreign_keys=OFF;                        ↵│
│ BEGIN TRANSACTION;                              ↵│
│ CREATE TABLE user_config (key text, value text);↵│
│ COMMIT;                                         ↵│
│                                                  │
└──────────────────────────────────────────────────┘
(1 row)
```

Note that while you are *seeing* the SQL text representation of the
sqlite object, it is stored internally in Postgres as a compact binary
representation of an in-memory sqlite database.  When the sqlite
database is accessed, it is automatically "expanded" from the on-disk
binary representation to a live sqlite database using
`sqlite3_deserialize()`.  Conversely if and when it is written to a
table, it is serialized and "TOASTed" into a binary flat
representation with `sqlite3_serialize()`.

## Modifying SQLite Objects

This new sqlite instance can now be inserted into a table and
manipulated with the `sqlite_exec()` function, for example:

```
CREATE TABLE customer (
    id bigserial PRIMARY KEY,
    name text NOT NULL,
    data sqlite DEFAULT 'CREATE TABLE user_config (key text, value text);'
    );

INSERT INTO customer (name) VALUES ('bob') RETURNING *;
┌────┬──────┬──────────────────────────────────────────────────┐
│ id │ name │                       data                       │
├────┼──────┼──────────────────────────────────────────────────┤
│  1 │ bob  │ PRAGMA foreign_keys=OFF;                        ↵│
│    │      │ BEGIN TRANSACTION;                              ↵│
│    │      │ CREATE TABLE user_config (key text, value text);↵│
│    │      │ COMMIT;                                         ↵│
│    │      │                                                  │
└────┴──────┴──────────────────────────────────────────────────┘
(1 row)

UPDATE customer SET data = sqlite_exec(data, $$INSERT INTO user_config VALUES ('color', 'blue')$$) RETURNING *;
┌────┬──────┬──────────────────────────────────────────────────────────────────────┐
│ id │ name │                                 data                                 │
├────┼──────┼──────────────────────────────────────────────────────────────────────┤
│  1 │ bob  │ PRAGMA foreign_keys=OFF;                                            ↵│
│    │      │ BEGIN TRANSACTION;                                                  ↵│
│    │      │ CREATE TABLE user_config (key text, value text);                    ↵│
│    │      │ INSERT INTO user_config(rowid,"key",value) VALUES(1,'color','blue');↵│
│    │      │ COMMIT;                                                             ↵│
│    │      │                                                                      │
└────┴──────┴──────────────────────────────────────────────────────────────────────┘
(1 row)
```

Notice how the `DEFAULT` value for the sqlite column in the new table
will initialize a new sqlite database into that column.  All new rows
in `customer` will contain contain a sqlite database named `data`
which in turn contains a sqlite table `user_config`.

The `sqlite_exec(db, query)` function takes a sqlite database and a
query as an argument, executes that query and returns the same
database, so this can be used for chaining updates to the same
database through multiple calls.

## Querying SQLite Objects

The `sqlite_query(db, query)` function is a Set Returning Function
(SRF) that `RETURNS SETOF RECORD` with a postgres row for each sqlite
result row from the given sqlite query.  Using the standard 'AS'
syntax these values can be mapped to table like Postgres results:

```
SELECT * from sqlite_query(
        (SELECT data FROM customer),
        'SELECT rowid, key, value from user_config') 
    AS (id integer, key text, value text);
┌────┬───────┬───────┐
│ id │  key  │ value │
├────┼───────┼───────┤
│  1 │ color │ blue  │
└────┴───────┴───────┘
(1 row)
```

postgres-sqlite support integers, floats and text.

## Serialize/Deserialize

postgres-sqlite has support for serializing and deserializing sqlite
databases as `bytea` byte array types.

```
SELECT pg_typeof(sqlite_serialize('create table foo (x)'::sqlite));
┌───────────┐
│ pg_typeof │
├───────────┤
│ bytea     │
└───────────┘
(1 row)

SELECT sqlite_deserialize(sqlite_serialize('create table foo (x)'::sqlite));
┌──────────────────────────┐
│    sqlite_deserialize    │
├──────────────────────────┤
│ PRAGMA foreign_keys=OFF;↵│
│ BEGIN TRANSACTION;      ↵│
│ CREATE TABLE foo (x);   ↵│
│ COMMIT;                 ↵│
│                          │
└──────────────────────────┘
(1 row)
```

## How it Works

Most Postgres data types, like numbers and text, are "flat" and have
identical in-memory and on-disk representations.  Postgres loads those
from disk into memory as is, and copies those datum around during the
course of a SQL query.

Some data types however, like arrays, and in this case, sqlite
databases, are more efficiently represented in memory differently than
their on-disk representation.  The act of copying flat datum around
also can cost a lot of cycles and memory for large, flat variable
length objects.

Postgres has an API called the [Expanded Datum
API](https://www.postgresql.org/docs/current/xtypes.html#XTYPES-TOAST)
that will "expand" a flat value into a more efficient in-memory
representation (in this case, a live in-memory sqlite db deserialized
from TOAST storage) and "flatten" the expanded object back into TOAST
when it's time to write it to the database.  The
sqlite3_serialize/deserialize API is used to store the sqlite database
in its native compact binary storage format inside Postgres.

By expanding a serialized "flat" sqlite database into an in-memory
object, this datum copying just copies a single pointer to the
database around during a sql command instead of the entire flattened
object.  This is particularly useful for plpgsql which can detect
expanded objects and handle references to them as pointers instead of
flat objects.
