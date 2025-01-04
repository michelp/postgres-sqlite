FROM ubuntu:latest
ARG UID    
ARG GID

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -y && \
    apt-get install -y make cmake git curl build-essential m4 sudo gdbserver python3-pip \
    gdb libreadline-dev bison flex zlib1g-dev libicu-dev icu-devtools tmux zile zip vim \
    gawk wget python3-full python3-virtualenv graphviz python3-dev sqlite3 libsqlite3-dev

RUN deluser --remove-home ubuntu
# add postgres user and make data dir        
RUN groupadd --gid ${GID} -r postgres && useradd --uid ${UID} --gid ${GID} --no-log-init -r -m -s /bin/bash -g postgres -G sudo postgres

# make postgres a sudoer
RUN echo "postgres ALL=(root) NOPASSWD:ALL" > /etc/sudoers.d/user && \
    chmod 0440 /etc/sudoers.d/user

USER postgres
    
ENV PGDATA /home/postgres/data
RUN /bin/rm -Rf "$PGDATA" && mkdir "$PGDATA"
WORKDIR "/home/postgres"

COPY --chown=postgres:postgres postgres-upstream postgres-upstream
RUN cd postgres-upstream && ./configure \
    --prefix=/usr/ \
    --with-python \
    --without-icu \
    --enable-debug \
#    --enable-depend --enable-cassert --enable-profiling \
#    CFLAGS="-ggdb -Og -g3 -fno-omit-frame-pointer" \
    CFLAGS="-O3" \
    && make -j 4 && sudo make install

USER postgres
WORKDIR /home/postgres

COPY . /home/postgres/sqlite

WORKDIR /home/postgres/sqlite
USER root
RUN ldconfig
    
RUN make && make install
    
RUN chown -R postgres:postgres /home/postgres/sqlite

USER postgres
# start the database            
RUN initdb -D "$PGDATA" --encoding UTF8
RUN sleep 5    
RUN pg_ctl -D "$PGDATA" -l /home/postgres/postgresql.log start
# wait forever
EXPOSE 5432
CMD tail -f /dev/null
