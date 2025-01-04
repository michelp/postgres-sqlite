-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION sqlite" to load this file. \quit

CREATE TYPE sqlite;

CREATE FUNCTION sqlite_in(cstring)
RETURNS sqlite
AS '$libdir/sqlite', 'sqlite_in'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION sqlite_out(sqlite)
RETURNS cstring
AS '$libdir/sqlite', 'sqlite_out'
LANGUAGE C IMMUTABLE STRICT;

CREATE TYPE sqlite (
    input = sqlite_in,
    output = sqlite_out,
    alignment = int4,
    storage = 'extended',
    internallength = -1
);

CREATE FUNCTION sqlite_query(sqlite, text)
RETURNS SETOF RECORD
AS '$libdir/sqlite', 'sqlite_query'
LANGUAGE C STRICT;

CREATE FUNCTION sqlite_exec(sqlite, text)
RETURNS sqlite
AS '$libdir/sqlite', 'sqlite_exec'
LANGUAGE C STRICT;

CREATE FUNCTION sqlite_serialize(sqlite)
RETURNS bytea
AS '$libdir/sqlite', 'sqlite_serialize'
LANGUAGE C STRICT;

CREATE FUNCTION sqlite_deserialize(bytea)
RETURNS sqlite
AS '$libdir/sqlite', 'sqlite_deserialize'
LANGUAGE C STRICT;
