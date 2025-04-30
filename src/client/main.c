#include "../../include/client.h"

int main(int argc, char *argv[])
{
  ClientContext client;
  client.connected = false;
  char buffer[BUFFER_SIZE];
  char server_ip[16] = "127.0.0.1"; // Default to localhost

  // Check for command line arguments
  if (argc > 1)
  {
    strncpy(server_ip, argv[1], sizeof(server_ip) - 1);
  }

  // Get username
  printf("Enter your username: ");
  fgets(client.username, MAX_USERNAME_LEN, stdin);

  // Remove newline character
  size_t username_len = strlen(client.username);
  if (username_len > 0 && client.username[username_len - 1] == '\n')
  {
    client.username[username_len - 1] = '\0';
  }

  // Connect to server
  printf("Connecting to %s...\n", server_ip);
  if (!connect_to_server(&client, server_ip))
  {
    printf("Failed to connect to server\n");
    return EXIT_FAILURE;
  }
  printf("Connected to server\n");

  // Main loop
  printf("Type your messages (type 'exit' to quit):\n");
  while (client.connected)
  {
    // Get input
    if (fgets(buffer, BUFFER_SIZE, stdin) == NULL)
    {
      break;
    }

    // Remove newline character
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n')
    {
      buffer[len - 1] = '\0';
    }

    // Check for exit command
    if (strcmp(buffer, "exit") == 0)
    {
      break;
    }

    // Send message
    if (!send_message(&client, 1, buffer)) // Use channel ID 1 (General) for command-line client
    {
      printf("Failed to send message\n");
      break;
    }
  }

  // Disconnect from server
  disconnect_from_server(&client);

  return 0;
}