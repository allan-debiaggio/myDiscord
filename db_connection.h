#ifndef DB_CONNECTION_H
#define DB_CONNECTION_H

#include <libpq-fe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Database connection parameters
#define DB_NAME "mydispute"
#define DB_USER "postgres"
#define DB_PASSWORD "postgres"
#define DB_HOST "localhost"
#define DB_PORT "5432"

// Function declarations
PGconn *connect_to_database();
void close_database_connection(PGconn *conn);
void check_database_connection(PGconn *conn);

#endif // DB_CONNECTION_H