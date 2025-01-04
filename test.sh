#!/bin/bash

DB_HOST="sqlite-test-db"
DB_NAME="postgres"
SU="postgres"
EXEC="docker exec $DB_HOST"
EXECIT="docker exec --user postgres -it $DB_HOST"
export BUILDKIT_PROGRESS=plain

echo force rm previous container
docker rm -f sqlite-test

set -e

echo building test image
docker build --build-arg UID=$(id -u) --build-arg GID=$(id -g) . -t sqlite/test

echo running test container
docker run -e POSTGRES_HOST_AUTH_METHOD=trust -v $(pwd)/tests/:/tests --cap-add=SYS_PTRACE --security-opt seccomp=unconfined -d --name "$DB_HOST" sqlite/test

sleep 3

if [ $# -eq 0 ]
then
    echo running tests
    $EXECIT psql
else
    echo running repl
    $EXEC
    $EXECIT $*
fi

echo destroying test container and image

docker rm --force "$DB_HOST"
