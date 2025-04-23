#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <json-c/json.h>

#define SERVER_PORT 8081
#define MAX_BUFFER_SIZE 1024
#define MAX_CLIENTS 10

// Message types
#define MSG_CONNECT 1
#define MSG_DISCONNECT 2
#define MSG_CHAT 3

// Structure for JSON messages
typedef struct
{
  int type;
  char content[MAX_BUFFER_SIZE];
} Message;

// Function declarations
void send_error_response(int client_fd, const char *error_message);

#endif // COMMON_H