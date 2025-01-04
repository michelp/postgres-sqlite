# postgres-sqlite

`postgres-sqlite` is a PostgreSQL extension that adds sqlite databases
as a first class data type for sqlite databases.

Using a combination of the Postgres "exapnded datum" API and the
sqlite serialize/deserialize API, sqlite can be used to create and
store small (up to 1GB) sqlite databases in postgres tables.

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

## Modifying SQLite Objects

This new sqlite instance can now be inserted into a table and
manipulated with the `sqlite()` function, for example:

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
in `customer` will contain a `foo` table if no default overriding
value is provided on insert as shown above.

The `sqlite_exec(db, query)` function takes a sqlite database and a
query as an argument, executes that query and returns the same
database.

## Querying SQLite Objects

The `sqlite_query(db, query)` function is a Set Returning Function
(SRF) that returns a record for each sqlite result row from the given
query:

```
SELECT * from sqlite_query(
   (SELECT data FROM customer),
  'SELECT rowid, key, value from user_config') AS (id integer, key text, value text);
┌────┬───────┬───────┐
│ id │  key  │ value │
├────┼───────┼───────┤
│  1 │ color │ blue  │
└────┴───────┴───────┘
(1 row)
```

postgres-sqlite support integers, floats and text.

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
when it's time to write it to the database.

By expanding a serialized "flat" sqlite database into an in-memory
object, this datum copying just copies a single pointer to the
database around during a sql command instead of the entire flattened
object.  This is particularly useful for plpgsql which can detect
expanded objects and handle references to them as pointers instead of
flat objects.
