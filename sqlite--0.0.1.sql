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
