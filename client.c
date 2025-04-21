#include "common.h"

void send_message(int sock_fd, int type, const char *content)
{
  struct json_object *json_msg = json_object_new_object();
  json_object_object_add(json_msg, "type", json_object_new_int(type));
  json_object_object_add(json_msg, "content", json_object_new_string(content));

  const char *json_str = json_object_to_json_string(json_msg);
  send(sock_fd, json_str, strlen(json_str), 0);

  printf("Sent message: %s\n", json_str);
  json_object_put(json_msg);
}

int main()
{
  int sock_fd;
  struct sockaddr_in server_addr;
  char input[MAX_BUFFER_SIZE];

  // Create socket
  sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd < 0)
  {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }

  // Configure server address
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(SERVER_PORT);

  // Convert IP address from string to binary form
  if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0)
  {
    perror("Invalid address");
    exit(EXIT_FAILURE);
  }

  // Connect to server
  if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    perror("Connection failed");
    exit(EXIT_FAILURE);
  }

  printf("Connected to server\n");

  // Send initial connection message
  send_message(sock_fd, MSG_CONNECT, "Hello, Server!");

  printf("Enter messages (type 'quit' to exit):\n");
  while (1)
  {
    printf("> ");
    fflush(stdout);

    if (fgets(input, MAX_BUFFER_SIZE, stdin) == NULL)
    {
      break;
    }

    // Remove trailing newline
    input[strcspn(input, "\n")] = 0;

    if (strcmp(input, "quit") == 0)
    {
      send_message(sock_fd, MSG_DISCONNECT, "Goodbye!");
      break;
    }

    send_message(sock_fd, MSG_CHAT, input);
  }

  close(sock_fd);
  return 0;
}