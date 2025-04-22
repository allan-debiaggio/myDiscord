#include "db_connection.h"

PGconn *connect_to_database()
{
  char conninfo[256];
  snprintf(conninfo, sizeof(conninfo),
           "dbname=%s user=%s password=%s host=%s port=%s",
           DB_NAME, DB_USER, DB_PASSWORD, DB_HOST, DB_PORT);

  PGconn *conn = PQconnectdb(conninfo);

  if (PQstatus(conn) != CONNECTION_OK)
  {
    fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(conn));
    PQfinish(conn);
    return NULL;
  }

  return conn;
}

void close_database_connection(PGconn *conn)
{
  if (conn)
  {
    PQfinish(conn);
  }
}

void check_database_connection(PGconn *conn)
{
  if (PQstatus(conn) == CONNECTION_OK)
  {
    printf("Database connection successful!\n");
    printf("Server version: %d\n", PQserverVersion(conn));
    printf("Database name: %s\n", PQdb(conn));
    printf("User: %s\n", PQuser(conn));
    printf("Host: %s\n", PQhost(conn));
    printf("Port: %s\n", PQport(conn));
  }
  else
  {
    fprintf(stderr, "Database connection failed: %s\n", PQerrorMessage(conn));
  }
}