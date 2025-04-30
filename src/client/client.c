#include "../../include/client.h"

// Connect to server
bool connect_to_server(ClientContext *client, const char *server_ip)
{
  // Create socket
  if ((client->socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    perror("Socket creation failed");
    return false;
  }

  // Initialize mutex
  if (pthread_mutex_init(&client->socket_mutex, NULL) != 0)
  {
    perror("Mutex initialization failed");
    close(client->socket);
    return false;
  }

  // Prepare server address
  client->server_address.sin_family = AF_INET;
  client->server_address.sin_port = htons(SERVER_PORT);

  // Convert IP address from text to binary
  if (inet_pton(AF_INET, server_ip, &client->server_address.sin_addr) <= 0)
  {
    perror("Invalid address");
    pthread_mutex_destroy(&client->socket_mutex);
    close(client->socket);
    return false;
  }

  // Connect to server
  if (connect(client->socket, (struct sockaddr *)&client->server_address, sizeof(client->server_address)) < 0)
  {
    perror("Connection failed");
    pthread_mutex_destroy(&client->socket_mutex);
    close(client->socket);
    return false;
  }

  // Set connected flag
  client->connected = true;

  // Create initial connect message
  Message connect_msg;
  connect_msg.type = MSG_CONNECT;
  strncpy(connect_msg.username, client->username, MAX_USERNAME_LEN);
  connect_msg.timestamp = time(NULL);
  strcpy(connect_msg.content, "has joined the chat");

  // Send connect message
  if (send(client->socket, &connect_msg, sizeof(Message), 0) < 0)
  {
    perror("Send failed");
    disconnect_from_server(client);
    return false;
  }

  // Create thread to receive messages
  if (pthread_create(&client->receive_thread, NULL, receive_messages, (void *)client) != 0)
  {
    perror("Thread creation failed");
    disconnect_from_server(client);
    return false;
  }

  return true;
}

// Receive messages from server
void *receive_messages(void *arg)
{
  ClientContext *client = (ClientContext *)arg;
  Message message;
  ssize_t bytes_received;

  while (client->connected)
  {
    // Receive message
    bytes_received = recv(client->socket, &message, sizeof(Message), 0);

    if (bytes_received <= 0)
    {
      // Connection closed or error
      printf("Disconnected from server\n");
      client->connected = false;
      break;
    }

    // Process message based on type
    switch (message.type)
    {
    case MSG_TEXT:
      printf("[%s] %s\n", message.username, message.content);
      break;
    case MSG_CONNECT:
      printf("%s has joined the chat\n", message.username);
      break;
    case MSG_DISCONNECT:
      printf("%s has left the chat\n", message.username);
      break;
    case MSG_STATUS:
      printf("[STATUS] %s: %s\n", message.username, message.content);
      break;
    default:
      printf("Unknown message type received\n");
    }
  }

  return NULL;
}

// Send message to server
bool send_message(ClientContext *client, int channel_id, const char *content)
{
  if (!client->connected)
  {
    printf("Not connected to server\n");
    return false;
  }

  // Create message
  Message message;
  message.type = MSG_TEXT;
  strncpy(message.username, client->username, MAX_USERNAME_LEN - 1);
  message.username[MAX_USERNAME_LEN - 1] = '\0';
  strncpy(message.content, content, BUFFER_SIZE - 1);
  message.content[BUFFER_SIZE - 1] = '\0';
  message.timestamp = time(NULL);
  message.channel_id = channel_id; // Set the channel ID

  // Send message
  pthread_mutex_lock(&client->socket_mutex);
  ssize_t bytes_sent = send(client->socket, &message, sizeof(Message), 0);
  pthread_mutex_unlock(&client->socket_mutex);

  if (bytes_sent < 0)
  {
    perror("Send failed");
    return false;
  }

  return true;
}

// Disconnect from server
void disconnect_from_server(ClientContext *client)
{
  if (!client->connected)
  {
    return;
  }

  // Set connected flag
  client->connected = false;

  // Close socket
  pthread_mutex_lock(&client->socket_mutex);
  close(client->socket);
  pthread_mutex_unlock(&client->socket_mutex);

  // Destroy mutex
  pthread_mutex_destroy(&client->socket_mutex);

  // Wait for receive thread to finish
  pthread_join(client->receive_thread, NULL);

  printf("Disconnected from server\n");
}