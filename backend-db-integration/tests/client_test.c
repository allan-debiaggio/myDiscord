#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024
#define DEFAULT_PORT 8081

int main(int argc, char *argv[])
{
  int sock;
  struct sockaddr_in server;
  char message[BUFFER_SIZE];
  char server_reply[BUFFER_SIZE];
  int port = DEFAULT_PORT;

  // Check if port is provided as command-line argument
  if (argc > 1)
  {
    port = atoi(argv[1]);
    if (port <= 0 || port > 65535)
    {
      printf("Invalid port number. Using default port %d.\n", DEFAULT_PORT);
      port = DEFAULT_PORT;
    }
  }

  // Create socket
  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1)
  {
    perror("Socket creation failed");
    return 1;
  }

  printf("Socket created\n");

  // Prepare the sockaddr_in structure
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr("127.0.0.1");
  server.sin_port = htons(port);

  // Connect to the server
  if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
  {
    perror("Connection failed");
    return 1;
  }

  printf("Connected to server on port %d\n", port);

  // Print available commands
  printf("\nAvailable commands:\n");
  printf("  /mute <username> [channel]    - Mute a user in a channel (default: general)\n");
  printf("  /unmute <username> [channel]  - Unmute a user in a channel (default: general)\n");
  printf("  /list muted [channel]         - List all muted users in a channel (default: general)\n");
  printf("  exit                          - Quit the client\n\n");

  // Communication loop
  while (1)
  {
    printf("Enter message (or 'exit' to quit): ");
    fgets(message, BUFFER_SIZE, stdin);

    // Remove newline character
    size_t len = strlen(message);
    if (len > 0 && message[len - 1] == '\n')
    {
      message[len - 1] = '\0';
    }

    // Check if user wants to exit
    if (strcmp(message, "exit") == 0)
    {
      break;
    }

    // Send the message
    if (send(sock, message, strlen(message), 0) < 0)
    {
      perror("Send failed");
      break;
    }

    // Receive a reply from the server
    int recv_size;
    if ((recv_size = recv(sock, server_reply, BUFFER_SIZE, 0)) < 0)
    {
      perror("Receive failed");
      break;
    }

    server_reply[recv_size] = '\0';
    printf("Server response: %s\n", server_reply);
  }

  // Close the socket
  close(sock);
  return 0;
}