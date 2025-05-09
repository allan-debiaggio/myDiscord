#include "server.h"

int main(int argc, char *argv[])
{
  // Display welcome message
  printf("My Dispute Server\n");
  printf("=================\n");

  // Start the server
  if (!start_server())
  {
    fprintf(stderr, "Failed to start server\n");
    return 1;
  }

  // Server is running in the current thread
  // When it returns, it means the server has stopped

  // Clean up resources
  stop_server();

  return 0;
}