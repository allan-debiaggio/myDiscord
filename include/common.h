#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>

// Constants
#define SERVER_PORT 8888
#define BUFFER_SIZE 4096
#define MAX_CLIENTS 100
#define MAX_USERNAME_LEN 32
#define MAX_CHANNELS 20
#define MAX_CHANNEL_NAME_LEN 50
#define DEFAULT_CHANNEL_ID 0
#define DEFAULT_CHANNEL_NAME "General"
#define MAX_PENDING_CONNECTIONS 10 // Maximum number of pending connections in the listen queue

// Message types
typedef enum
{
  MSG_TEXT,
  MSG_CONNECT,
  MSG_DISCONNECT,
  MSG_STATUS,
  MSG_CHANNEL_JOIN,   // New message type for joining a channel
  MSG_CHANNEL_LEAVE,  // New message type for leaving a channel
  MSG_CHANNEL_LIST,   // New message type for listing available channels
  MSG_CHANNEL_CREATE, // New message type for creating a new channel
  MSG_SERVER          // New message type for server messages
} MessageType;

// Message structure
typedef struct
{
  MessageType type;
  char username[MAX_USERNAME_LEN];
  char content[BUFFER_SIZE];
  time_t timestamp;
  int channel_id; // Added channel_id to message structure
  int user_id;    // Added user_id to message structure
} Message;

// Function prototypes
void error_exit(const char *msg);
void log_message(const char *format, ...);

#endif /* COMMON_H */