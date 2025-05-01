#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFFER_SIZE 4096

void send_message(int sock, const char *message)
{
  printf("Sending: %s\n", message);
  if (send(sock, message, strlen(message), 0) < 0)
  {
    perror("Send failed");
  }
}

void receive_message(int sock, char *buffer)
{
  memset(buffer, 0, BUFFER_SIZE);
  int read_size = recv(sock, buffer, BUFFER_SIZE, 0);
  if (read_size > 0)
  {
    buffer[read_size] = '\0';
    printf("Received: %s\n", buffer);
  }
  else if (read_size == 0)
  {
    printf("Server disconnected\n");
  }
  else
  {
    perror("Receive failed");
  }
}

int main(int argc, char *argv[])
{
  const char *server_ip = "127.0.0.1";
  int server_port = 8082;

  // Parse command line arguments
  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "--port") == 0 && i + 1 < argc)
    {
      server_port = atoi(argv[i + 1]);
      i++;
    }
    else if (strcmp(argv[i], "--ip") == 0 && i + 1 < argc)
    {
      server_ip = argv[i + 1];
      i++;
    }
  }

  // Create socket
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
  {
    perror("Failed to create socket");
    return 1;
  }

  // Prepare the sockaddr_in structure
  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(server_ip);
  server.sin_port = htons(server_port);

  // Connect to server
  printf("Connecting to %s:%d...\n", server_ip, server_port);
  if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
  {
    perror("Connect failed");
    return 1;
  }

  printf("Connected to server!\n");

  // Receive welcome message
  char buffer[BUFFER_SIZE];
  receive_message(sock, buffer);

  // Login as admin
  printf("\n=== Login as admin ===\n");
  send_message(sock, "LOGIN:admin:password");
  receive_message(sock, buffer);

  if (strstr(buffer, "LOGIN_SUCCESS") == NULL)
  {
    printf("Failed to login as admin. Exiting.\n");
    close(sock);
    return 1;
  }

  // Test mute functionality
  printf("\n=== Testing Mute Functionality ===\n");

  // First, list muted users to see initial state
  printf("\nTest 1: List muted users (should be empty)\n");
  send_message(sock, "LIST_MUTED:general");
  receive_message(sock, buffer);

  // Try to mute a user
  printf("\nTest 2: Mute user 'user1' in general channel\n");
  send_message(sock, "MUTE:user1:general:5"); // Mute for 5 minutes
  receive_message(sock, buffer);

  // Check muted users list again
  printf("\nTest 3: List muted users (should include user1)\n");
  send_message(sock, "LIST_MUTED:general");
  receive_message(sock, buffer);

  // Try to mute another user
  printf("\nTest 4: Mute user 'user2' in general channel\n");
  send_message(sock, "MUTE:user2:general:10"); // Mute for 10 minutes
  receive_message(sock, buffer);

  // Check muted users list with two users
  printf("\nTest 5: List muted users (should include user1 and user2)\n");
  send_message(sock, "LIST_MUTED:general");
  receive_message(sock, buffer);

  // Try to unmute a user
  printf("\nTest 6: Unmute user 'user1'\n");
  send_message(sock, "UNMUTE:user1:general");
  receive_message(sock, buffer);

  // Check muted users list again (only user2 should remain)
  printf("\nTest 7: List muted users (only user2 should remain)\n");
  send_message(sock, "LIST_MUTED:general");
  receive_message(sock, buffer);

  close(sock);
  printf("\nTest complete. Disconnected from server.\n");
  return 0;
}