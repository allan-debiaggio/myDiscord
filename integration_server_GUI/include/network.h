#ifndef NETWORK_H
#define NETWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include "my_dispute.h"

// Network constants
#define SERVER_PORT 8888
#define SERVER_IP "127.0.0.1"
#define MAX_CONNECTIONS 32

// Message types
#define MSG_AUTH 1
#define MSG_AUTH_RESPONSE 2
#define MSG_REGISTER 3
#define MSG_REG_RESPONSE 4
#define MSG_CHANNEL_LIST 5
#define MSG_USER_LIST 6
#define MSG_CHANNEL_MSG 7
#define MSG_PRIVATE_MSG 8
#define MSG_CREATE_CHANNEL 9
#define MSG_DELETE_CHANNEL 10
#define MSG_MUTE_USER 11
#define MSG_SET_ROLE 12
#define MSG_USER_STATUS 13
#define MSG_ERROR 14
#define MSG_SUCCESS 15

// Network message structure
typedef struct
{
  int type;                        // Message type (see constants above)
  int user_id;                     // Sender's user ID
  int target_id;                   // Target channel or user ID
  int param;                       // Additional parameter
  char username[MAX_USERNAME_LEN]; // Username (for auth or target)
  char content[MAX_MESSAGE_LEN];   // Message content
} NetworkMessage;

// Message processing (network.c)
void process_channel_list(NetworkMessage *msg);
void process_user_list(NetworkMessage *msg);
void process_channel_message(NetworkMessage *msg);
void process_private_message(NetworkMessage *msg);
void process_user_status(NetworkMessage *msg);

// Client connection structure
typedef struct
{
  int socket;                      // Client socket
  int user_id;                     // User ID in the users array
  int is_authenticated;            // Whether client is authenticated
  char username[MAX_USERNAME_LEN]; // Client's username
} ClientConnection;

// Client-side network functions
int network_init();                      // Initialize network connection
int network_connect();                   // Connect to server
void network_disconnect();               // Disconnect from server
void *network_receive_thread(void *arg); // Thread for receiving server messages
int network_authenticate(const char *username, const char *password);
int network_register(const char *username, const char *email, const char *password);
int network_get_channels(void);
int network_get_users(void);
int network_send_channel_message(int channel_id, const char *message);
int network_send_private_message(const char *username, const char *message);
int network_create_channel(const char *name);
int network_delete_channel(const char *name);
int network_mute_user(const char *username, int channel_id, int minutes);
int network_set_user_role(const char *username, int role);

// Server-side network functions
int start_server();             // Start the server
void stop_server();             // Stop the server
void *handle_client(void *arg); // Client handler thread
void handle_auth_message(ClientConnection *client, NetworkMessage *msg);
void handle_register_message(ClientConnection *client, NetworkMessage *msg);
void handle_channel_list_request(ClientConnection *client);
void handle_user_list_request(ClientConnection *client);
void handle_channel_message(ClientConnection *client, NetworkMessage *msg);
void handle_private_message(ClientConnection *client, NetworkMessage *msg);
void handle_create_channel(ClientConnection *client, NetworkMessage *msg);
void handle_delete_channel(ClientConnection *client, NetworkMessage *msg);
void handle_mute_user(ClientConnection *client, NetworkMessage *msg);
void handle_set_role(ClientConnection *client, NetworkMessage *msg);
void broadcast_to_channel(int channel_id, NetworkMessage *msg);
void send_to_user(int user_id, NetworkMessage *msg);
void disconnect_client(ClientConnection *client);
void initialize_server_data();

#endif /* NETWORK_H */