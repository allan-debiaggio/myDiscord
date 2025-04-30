#ifndef SERVER_H
#define SERVER_H

#include "common.h"
#include "database.h"

// Client structure
typedef struct
{
  int socket;
  struct sockaddr_in address;
  char username[MAX_USERNAME_LEN];
  bool active;
  int current_channel_id; // Added to track which channel the client is in
} Client;

// Channel structure
typedef struct
{
  int channel_id;
  char name[MAX_CHANNEL_NAME_LEN];
  char description[BUFFER_SIZE];
  bool is_public;
  bool active;
} Channel;

// Server context
typedef struct
{
  int server_fd;
  int socket; // Socket for the server
  int port;   // Port the server is running on
  struct sockaddr_in address;
  Client clients[MAX_CLIENTS];
  Channel channels[MAX_CHANNELS]; // Added array of channels
  int channel_count;              // Added to track number of channels
  pthread_mutex_t clients_mutex;
  pthread_mutex_t channels_mutex; // Added mutex for thread-safe channel operations
  bool running;
  DatabaseContext *db; // Database context pointer
} ServerContext;

// Handler arguments structure (for passing data to client threads)
typedef struct
{
  ServerContext *server;
  int client_socket;
  struct sockaddr_in client_address;
} HandlerArgs;

// Function prototypes
int initialize_server(ServerContext *server, int port);
void run_server(ServerContext *server);
void *handle_client(void *arg);
void broadcast_message(ServerContext *server, Message *message, int sender_socket);
void channel_broadcast(ServerContext *server, Message *message, int sender_socket, int channel_id);
void send_channel_list(ServerContext *server, int client_socket);
int create_channel(ServerContext *server, const char *name, const char *description, bool is_public);
int find_channel_by_name(ServerContext *server, const char *name);
void cleanup_server(ServerContext *server);

#endif /* SERVER_H */