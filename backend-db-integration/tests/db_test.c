#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>

int main()
{
  printf("Testing PostgreSQL connection...\n");

  PGconn *conn = PQconnectdb("dbname=mydiscord30 user=postgres password=postgres host=localhost");

  if (PQstatus(conn) != CONNECTION_OK)
  {
    fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(conn));
    PQfinish(conn);
    return 1;
  }

  printf("Connected to database successfully!\n");

  // Test a simple query
  PGresult *res = PQexec(conn, "SELECT version()");

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "Query failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    PQfinish(conn);
    return 1;
  }

  // Print version
  printf("PostgreSQL version: %s\n", PQgetvalue(res, 0, 0));

  // Create users table if it doesn't exist
  PQclear(res);
  res = PQexec(conn,
               "CREATE TABLE IF NOT EXISTS users ("
               "id SERIAL PRIMARY KEY,"
               "username VARCHAR(64) UNIQUE NOT NULL,"
               "password VARCHAR(64) NOT NULL,"
               "role INTEGER NOT NULL DEFAULT 1,"
               "is_online BOOLEAN DEFAULT FALSE,"
               "current_channel VARCHAR(32) DEFAULT 'general'"
               ")");

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "Table creation failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    PQfinish(conn);
    return 1;
  }

  printf("Users table exists or was created successfully.\n");

  // Insert a test user
  PQclear(res);
  const char *paramValues[2] = {"test_user", "test_password"};

  res = PQexecParams(conn,
                     "INSERT INTO users (username, password) VALUES ($1, $2) ON CONFLICT (username) DO NOTHING",
                     2, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "User insertion failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    PQfinish(conn);
    return 1;
  }

  printf("Test user inserted or already exists.\n");

  // Query users
  PQclear(res);
  res = PQexec(conn, "SELECT id, username FROM users");

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "User query failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    PQfinish(conn);
    return 1;
  }

  int nRows = PQntuples(res);
  printf("Found %d users:\n", nRows);

  for (int i = 0; i < nRows; i++)
  {
    printf("%s. %s\n", PQgetvalue(res, i, 0), PQgetvalue(res, i, 1));
  }

  // Clean up
  PQclear(res);
  PQfinish(conn);

  printf("Database test completed successfully!\n");
  return 0;
}