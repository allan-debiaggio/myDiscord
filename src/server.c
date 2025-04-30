#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <libpq-fe.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 2048

typedef struct
{
  int socket;
  PGconn *db_conn;
} client_data;

void test_database_connection(PGconn *conn)
{
  PGresult *res = PQexec(conn, "SELECT 1");
  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "Database test failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    exit(EXIT_FAILURE);
  }
  PQclear(res);
  printf("Database connection test successful!\n");
}

void *handle_client(void *arg)
{
  client_data *data = (client_data *)arg;
  char buffer[BUFFER_SIZE];

  while (1)
  {
    ssize_t bytes_read = recv(data->socket, buffer, BUFFER_SIZE, 0);
    if (bytes_read <= 0)
      break;

    buffer[bytes_read] = '\0'; // Ensure null-termination
    printf("Received message: %s\n", buffer);

    // Store message in database for testing
    const char *query = "INSERT INTO messages (channel_id, user_id, content) VALUES (1, 1, $1)";
    const char *paramValues[1] = {buffer};
    PGresult *res = PQexecParams(data->db_conn, query, 1, NULL, paramValues, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
      fprintf(stderr, "Database error: %s\n", PQerrorMessage(data->db_conn));
    }
    else
    {
      printf("Message stored in database\n");
    }
    PQclear(res);

    // Echo back to client
    send(data->socket, buffer, bytes_read, 0);
  }

  close(data->socket);
  PQfinish(data->db_conn);
  free(data);
  return NULL;
}

int main()
{
  printf("Starting MyDiscord server...\n");

  // Test database connection
  printf("Testing database connection...\n");
  PGconn *test_conn = PQconnectdb("dbname=mydiscord_test");
  if (PQstatus(test_conn) != CONNECTION_OK)
  {
    fprintf(stderr, "Database connection failed: %s\n", PQerrorMessage(test_conn));
    PQfinish(test_conn);
    return EXIT_FAILURE;
  }

  test_database_connection(test_conn);
  PQfinish(test_conn);
  printf("Database connection test completed.\n");

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0)
  {
    perror("Failed to create socket");
    return EXIT_FAILURE;
  }

  // Allow reuse of address
  int opt = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
  {
    perror("setsockopt failed");
    return EXIT_FAILURE;
  }

  struct sockaddr_in address = {
      .sin_family = AF_INET,
      .sin_port = htons(8080),
      .sin_addr.s_addr = INADDR_ANY};

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
  {
    perror("Bind failed");
    return EXIT_FAILURE;
  }

  if (listen(server_fd, MAX_CLIENTS) < 0)
  {
    perror("Listen failed");
    return EXIT_FAILURE;
  }

  printf("Server listening on port 8080...\n");

  while (1)
  {
    client_data *data = malloc(sizeof(client_data));
    if (!data)
    {
      perror("Memory allocation failed");
      continue;
    }

    socklen_t addrlen = sizeof(address);

    data->socket = accept(server_fd,
                          (struct sockaddr *)&address,
                          &addrlen);

    if (data->socket < 0)
    {
      perror("Accept failed");
      free(data);
      continue;
    }

    printf("New client connected\n");

    // Initialize database connection for client
    data->db_conn = PQconnectdb("dbname=mydiscord_test");
    if (PQstatus(data->db_conn) != CONNECTION_OK)
    {
      fprintf(stderr, "Connection to database failed: %s",
              PQerrorMessage(data->db_conn));
      PQfinish(data->db_conn);
      close(data->socket);
      free(data);
      continue;
    }

    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, handle_client, data) != 0)
    {
      perror("Thread creation failed");
      PQfinish(data->db_conn);
      close(data->socket);
      free(data);
      continue;
    }
    pthread_detach(thread_id);
  }

  return 0;
}