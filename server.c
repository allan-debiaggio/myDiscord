#include "common.h"
#include "db_connection.h"
#include "db_operations.h"
#include "auth.h"
#include <json-c/json.h>
#include <signal.h>
#include <errno.h>

static volatile int running = 1;

void handle_signal(int sig)
{
  (void)sig; // Suppress unused parameter warning
  running = 0;
}

// Send error response to client
void send_error_response(int client_fd, const char *error_message)
{
  struct json_object *response = json_object_new_object();
  json_object_object_add(response, "success", json_object_new_boolean(false));
  json_object_object_add(response, "error", json_object_new_string(error_message));

  const char *response_str = json_object_to_json_string(response);
  send(client_fd, response_str, strlen(response_str), 0);
  json_object_put(response);
}

// Handle authentication requests
void handle_auth_request(int client_fd, PGconn *db_conn, const char *content)
{
  struct json_object *json = json_tokener_parse(content);
  if (!json)
  {
    send_error_response(client_fd, "Invalid JSON format");
    return;
  }

  struct json_object *type_obj;
  if (!json_object_object_get_ex(json, "auth_type", &type_obj))
  {
    send_error_response(client_fd, "Missing auth_type");
    json_object_put(json);
    return;
  }

  const char *auth_type = json_object_get_string(type_obj);
  AuthResult result;

  if (strcmp(auth_type, "register") == 0)
  {
    struct json_object *first_name_obj, *last_name_obj, *email_obj, *password_obj;
    if (!json_object_object_get_ex(json, "first_name", &first_name_obj) ||
        !json_object_object_get_ex(json, "last_name", &last_name_obj) ||
        !json_object_object_get_ex(json, "email", &email_obj) ||
        !json_object_object_get_ex(json, "password", &password_obj))
    {
      send_error_response(client_fd, "Missing required fields for registration");
      json_object_put(json);
      return;
    }

    result = register_user(db_conn,
                           json_object_get_string(first_name_obj),
                           json_object_get_string(last_name_obj),
                           json_object_get_string(email_obj),
                           json_object_get_string(password_obj));
  }
  else if (strcmp(auth_type, "login") == 0)
  {
    struct json_object *email_obj, *password_obj;
    if (!json_object_object_get_ex(json, "email", &email_obj) ||
        !json_object_object_get_ex(json, "password", &password_obj))
    {
      send_error_response(client_fd, "Missing email or password");
      json_object_put(json);
      return;
    }

    result = login_user(db_conn,
                        json_object_get_string(email_obj),
                        json_object_get_string(password_obj));
  }
  else
  {
    send_error_response(client_fd, "Invalid auth_type");
    json_object_put(json);
    return;
  }

  // Send response
  struct json_object *response = json_object_new_object();
  json_object_object_add(response, "success", json_object_new_boolean(result.success));
  if (result.success)
  {
    json_object_object_add(response, "user_id", json_object_new_int(result.user_id));
  }
  else
  {
    json_object_object_add(response, "error", json_object_new_string(result.error_message));
  }

  const char *response_str = json_object_to_json_string(response);
  send(client_fd, response_str, strlen(response_str), 0);

  json_object_put(json);
  json_object_put(response);
}

// Handle client connection
void handle_client(int client_fd, struct sockaddr_in client_addr)
{
  char buffer[MAX_BUFFER_SIZE];
  ssize_t bytes_read;
  PGconn *db_conn = connect_to_database();

  if (!db_conn)
  {
    printf("Failed to connect to database for client %s:%d\n",
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    close(client_fd);
    return;
  }

  printf("New client connected from %s:%d\n",
         inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

  while ((bytes_read = recv(client_fd, buffer, MAX_BUFFER_SIZE - 1, 0)) > 0)
  {
    buffer[bytes_read] = '\0';

    struct json_object *json = json_tokener_parse(buffer);
    if (!json)
    {
      send_error_response(client_fd, "Invalid JSON format");
      continue;
    }

    struct json_object *type_obj;
    if (!json_object_object_get_ex(json, "type", &type_obj))
    {
      send_error_response(client_fd, "Missing message type");
      json_object_put(json);
      continue;
    }

    int msg_type = json_object_get_int(type_obj);
    struct json_object *content_obj;
    const char *content = NULL;

    if (json_object_object_get_ex(json, "content", &content_obj))
    {
      content = json_object_get_string(content_obj);
    }

    switch (msg_type)
    {
    case MSG_CONNECT:
      handle_auth_request(client_fd, db_conn, content);
      break;
    case MSG_DISCONNECT:
      printf("Client %s:%d requested disconnect\n",
             inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
      close_database_connection(db_conn);
      close(client_fd);
      json_object_put(json);
      return;
    case MSG_CHAT:
      // Handle chat messages (to be implemented)
      break;
    default:
      send_error_response(client_fd, "Unknown message type");
    }

    json_object_put(json);
  }

  printf("Client %s:%d disconnected\n",
         inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
  close_database_connection(db_conn);
  close(client_fd);
}

int main()
{
  int server_fd, client_fd;
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_len = sizeof(client_addr);

  // Set up signal handlers
  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);
  signal(SIGCHLD, SIG_IGN); // Prevent zombie processes

  // Test database connection first
  PGconn *db_conn = connect_to_database();
  if (!db_conn)
  {
    printf("Failed to connect to database. Server startup aborted.\n");
    return 1;
  }
  printf("Database connection successful. Server starting...\n");
  close_database_connection(db_conn);

  // Create socket
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0)
  {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }

  // Set socket options
  int opt = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
  {
    perror("Setsockopt failed");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  // Configure server address
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(SERVER_PORT);

  // Bind socket
  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    perror("Bind failed");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  // Listen for connections
  if (listen(server_fd, MAX_CLIENTS) < 0)
  {
    perror("Listen failed");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  printf("Server is listening on port %d...\n", SERVER_PORT);

  while (running)
  {
    // Accept client connection
    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0)
    {
      if (errno == EINTR)
      {
        // Interrupted system call, check if we should continue running
        continue;
      }
      perror("Accept failed");
      continue;
    }

    // Fork a new process to handle the client
    pid_t pid = fork();
    if (pid < 0)
    {
      perror("Fork failed");
      close(client_fd);
      continue;
    }

    if (pid == 0)
    {
      // Child process
      close(server_fd); // Close server socket in child
      handle_client(client_fd, client_addr);
      exit(0); // Ensure child process exits after handling client
    }
    else
    {
      // Parent process
      close(client_fd); // Close client socket in parent
    }
  }

  // Cleanup
  close(server_fd);
  printf("\nServer shutting down...\n");
  return 0;
}