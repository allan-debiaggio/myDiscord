#include "db_connection.h"

int main()
{
  PGconn *conn = connect_to_database();

  if (conn)
  {
    check_database_connection(conn);
    close_database_connection(conn);
    printf("Database connection test completed successfully.\n");
  }
  else
  {
    printf("Failed to connect to the database.\n");
    return 1;
  }

  return 0;
}