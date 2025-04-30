#ifndef CLIENT_H
#define CLIENT_H

#include "common.h"

// Client context
typedef struct
{
  int socket;
  struct sockaddr_in server_address;
  char username[MAX_USERNAME_LEN];
  bool connected;
  pthread_t receive_thread;
  pthread_mutex_t socket_mutex;
} ClientContext;

// Function prototypes
bool connect_to_server(ClientContext *client, const char *server_ip);
void *receive_messages(void *arg);
bool send_message(ClientContext *client, int channel_id, const char *content);
void disconnect_from_server(ClientContext *client);

// GTK-specific handlers are moved to gtk_client.h

#endif /* CLIENT_H */