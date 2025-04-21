#include "common.h"
#include <signal.h>

void handle_client(int client_fd, struct sockaddr_in client_addr)
{
  char buffer[MAX_BUFFER_SIZE];
  struct json_object *json_msg;

  printf("New client connected from %s:%d\n",
         inet_ntoa(client_addr.sin_addr),
         ntohs(client_addr.sin_port));

  while (1)
  {
    // Receive and process client message
    memset(buffer, 0, MAX_BUFFER_SIZE);
    ssize_t bytes_received = recv(client_fd, buffer, MAX_BUFFER_SIZE, 0);

    if (bytes_received <= 0)
    {
      // Client disconnected or error
      printf("Client %s:%d disconnected\n",
             inet_ntoa(client_addr.sin_addr),
             ntohs(client_addr.sin_port));
      break;
    }

    json_msg = json_tokener_parse(buffer);
    if (json_msg != NULL)
    {
      struct json_object *type_obj, *content_obj;
      if (json_object_object_get_ex(json_msg, "type", &type_obj))
      {
        int msg_type = json_object_get_int(type_obj);
        printf("Received message type: %d\n", msg_type);

        if (msg_type == MSG_DISCONNECT)
        {
          printf("Client %s:%d requested disconnect\n",
                 inet_ntoa(client_addr.sin_addr),
                 ntohs(client_addr.sin_port));
          json_object_put(json_msg);
          break;
        }

        if (json_object_object_get_ex(json_msg, "content", &content_obj))
        {
          const char *content = json_object_get_string(content_obj);
          printf("Message content: %s\n", content);
        }
      }
      json_object_put(json_msg);
    }
  }

  close(client_fd);
  exit(0);
}

int main()
{
  int server_fd, client_fd;
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_len = sizeof(client_addr);

  // Ignore SIGCHLD to prevent zombie processes
  signal(SIGCHLD, SIG_IGN);

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
    exit(EXIT_FAILURE);
  }

  // Configure server address
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(SERVER_PORT);

  // Bind socket
  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    perror("Bind failed");
    exit(EXIT_FAILURE);
  }

  // Listen for connections
  if (listen(server_fd, MAX_CLIENTS) < 0)
  {
    perror("Listen failed");
    exit(EXIT_FAILURE);
  }

  printf("Server is listening on port %d...\n", SERVER_PORT);

  while (1)
  {
    // Accept client connection
    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0)
    {
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
    }
    else
    {
      // Parent process
      close(client_fd); // Close client socket in parent
    }
  }

  close(server_fd);
  return 0;
}