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
  memset(buffer, 0, BUFFER_SIZE);
  int read_size = recv(sock, buffer, BUFFER_SIZE, 0);
  if (read_size > 0)
  {
    buffer[read_size] = '\0';
    printf("Received: %s\n", buffer);
  }

  // Login
  printf("\n=== Testing Login ===\n");
  // Test with admin account
  send_message(sock, "LOGIN:admin:password");

  // Receive response
  memset(buffer, 0, BUFFER_SIZE);
  read_size = recv(sock, buffer, BUFFER_SIZE, 0);
  if (read_size > 0)
  {
    buffer[read_size] = '\0';
    printf("Received: %s\n", buffer);
  }

  // If login successful, test sending a message
  if (strstr(buffer, "LOGIN_SUCCESS") != NULL)
  {
    printf("\n=== Testing Message Sending ===\n");
    send_message(sock, "MSG:general:Hello from the test client!");

    // Receive response
    memset(buffer, 0, BUFFER_SIZE);
    read_size = recv(sock, buffer, BUFFER_SIZE, 0);
    if (read_size > 0)
    {
      buffer[read_size] = '\0';
      printf("Received: %s\n", buffer);
    }
  }

  close(sock);
  printf("Test complete. Disconnected from server.\n");
  return 0;
}