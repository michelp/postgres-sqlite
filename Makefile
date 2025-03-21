#http://blog.pgxn.org/post/4783001135/extension-makefiles pg makefiles

EXTENSION = sqlite
PG_CONFIG ?= pg_config
DATA = $(wildcard *--*.sql)
PGXS := $(shell $(PG_CONFIG) --pgxs)
MODULE_big = sqlite
OBJS = $(patsubst %.c,%.o,$(wildcard src/*.c))
SHLIB_LINK = -lc -lsqlite3 -lpq
CFLAGS = -std=c11
#PG_CPPFLAGS = -O0

TESTS        = $(wildcard test/sql/*.sql)
REGRESS      = $(patsubst test/sql/%.sql,%,$(TESTS))
REGRESS_OPTS = --inputdir=test --load-language=plpgsql
include $(PGXS)

sql: sqlite--0.0.1.sql
	bash sqlite--0.0.1-gen.sql.sh
