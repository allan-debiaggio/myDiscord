#ifndef SERVER_H
#define SERVER_H

#include "network.h"
#include <signal.h>
#include <fcntl.h>

// Server context
typedef struct
{
  int server_socket;
  int running;
  ClientConnection clients[MAX_CONNECTIONS];
  pthread_mutex_t clients_mutex;
  Channel channels[MAX_CHANNELS];
  int channel_count;
  pthread_mutex_t channels_mutex;
  User users[MAX_USERS];
  int user_count;
  pthread_mutex_t users_mutex;
} ServerContext;

// Thread argument structure
typedef struct
{
  int client_index;
} ClientThreadArg;

// Global server context
extern ServerContext server_ctx;

// Server functions
int start_server();
void stop_server();
void initialize_server_data();
void accept_connections();
void handle_new_connection(int client_socket);
void *client_thread(void *arg);
void disconnect_client(ClientConnection *client);
void setup_signal_handlers();
void handle_termination(int sig);

// Helper functions
void set_nonblocking(int socket_fd);
void log_info(const char *format, ...);
void log_error(const char *format, ...);
void send_error_message(ClientConnection *client, const char *message);

#endif /* SERVER_H */