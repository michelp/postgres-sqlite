FROM postgres:17

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -y && apt-get install -y \
    build-essential \
    libreadline-dev \
    zlib1g-dev \
    postgresql-server-dev-17

USER postgres
WORKDIR /home/postgres

COPY . /home/postgres/sqlite

WORKDIR /home/postgres/sqlite
USER root
RUN ldconfig
    
RUN make && make install
    
RUN chown -R postgres:postgres /home/postgres/sqlite
