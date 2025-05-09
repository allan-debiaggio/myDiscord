#include "network.h"
#include <errno.h>
#include "my_dispute.h"

// Global variables
int network_socket = -1;
int network_user_id = -1;
pthread_t receive_thread;
pthread_mutex_t app_state_mutex = PTHREAD_MUTEX_INITIALIZER;
extern AppState app_state;

// Initialize network connection and authenticate the user
int network_init()
{
  // First establish a connection to the server
  if (!network_connect())
  {
    return 0;
  }

  // Then authenticate with current user credentials
  const char *username = app_state.users[app_state.current_user_index].username;
  const char *password = app_state.users[app_state.current_user_index].password;

  if (!network_authenticate(username, password))
  {
    network_disconnect();
    return 0;
  }

  return 1;
}

// Connect to server
int network_connect()
{
  struct sockaddr_in server_addr;

  // Create socket
  if ((network_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    fprintf(stderr, "Socket creation error\n");
    return 0;
  }

  // Prepare server address structure
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(SERVER_PORT);

  // Convert IP address from text to binary
  if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0)
  {
    fprintf(stderr, "Invalid address or address not supported\n");
    return 0;
  }

  // Connect to server
  if (connect(network_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    fprintf(stderr, "Connection failed: %s\n", strerror(errno));
    return 0;
  }

  // Start the receive thread
  if (pthread_create(&receive_thread, NULL, network_receive_thread, NULL) != 0)
  {
    fprintf(stderr, "Failed to create receive thread\n");
    close(network_socket);
    network_socket = -1;
    return 0;
  }

  return 1;
}

// Disconnect from server
void network_disconnect()
{
  if (network_socket != -1)
  {
    // Signal thread to exit
    pthread_cancel(receive_thread);
    pthread_join(receive_thread, NULL);

    // Close the socket
    close(network_socket);
    network_socket = -1;
    network_user_id = -1;
  }
}

// Thread for receiving messages from server
void *network_receive_thread(void *arg)
{
  NetworkMessage msg;

  while (1)
  {
    // Receive message from server
    int recv_result = recv(network_socket, &msg, sizeof(NetworkMessage), 0);
    if (recv_result <= 0)
    {
      // Connection closed or error
      fprintf(stderr, "Server disconnected (recv_result=%d): %s\n",
              recv_result, strerror(errno));
      close(network_socket);
      network_socket = -1;
      pthread_exit(NULL);
    }

    // Process message based on type
    pthread_mutex_lock(&app_state_mutex);

    switch (msg.type)
    {
    case MSG_CHANNEL_LIST:
      process_channel_list(&msg);
      break;

    case MSG_USER_LIST:
      process_user_list(&msg);
      break;

    case MSG_CHANNEL_MSG:
      process_channel_message(&msg);
      break;

    case MSG_PRIVATE_MSG:
      process_private_message(&msg);
      break;

    case MSG_AUTH_RESPONSE:
      network_user_id = msg.user_id;
      break;

    case MSG_REG_RESPONSE:
      network_user_id = msg.user_id;
      break;

    case MSG_USER_STATUS:
      process_user_status(&msg);
      break;

    case MSG_ERROR:
      // Display error message
      fprintf(stderr, "Server error: %s\n", msg.content);
      break;

    default:
      fprintf(stderr, "Unknown message type: %d\n", msg.type);
      break;
    }

    pthread_mutex_unlock(&app_state_mutex);
  }

  return NULL;
}

// Process channel list message
void process_channel_list(NetworkMessage *msg)
{
  int channel_count = 0;
  sscanf(msg->content, "%d", &channel_count);

  // Reset channel count
  app_state.channel_count = 0;

  // Parse channel data
  char *token = strtok(msg->content, ":");
  if (token)
    token = strtok(NULL, ":"); // Skip the count

  while (token && app_state.channel_count < MAX_CHANNELS)
  {
    strncpy(app_state.channels[app_state.channel_count].name, token, MAX_CHANNEL_NAME_LEN - 1);
    app_state.channels[app_state.channel_count].message_count = 0;
    app_state.channel_count++;
    token = strtok(NULL, ":");
  }
}

// Process user list message
void process_user_list(NetworkMessage *msg)
{
  int user_count = 0;
  sscanf(msg->content, "%d", &user_count);

  // Preserve current user
  char current_username[MAX_USERNAME_LEN];
  strncpy(current_username, app_state.users[app_state.current_user_index].username, MAX_USERNAME_LEN - 1);
  char current_password[MAX_PASSWORD_LEN];
  strncpy(current_password, app_state.users[app_state.current_user_index].password, MAX_PASSWORD_LEN - 1);
  char current_email[MAX_EMAIL_LEN];
  strncpy(current_email, app_state.users[app_state.current_user_index].email, MAX_EMAIL_LEN - 1);
  int current_role = app_state.users[app_state.current_user_index].role;

  // Parse user data
  char *user_token = strtok(msg->content, ":");
  if (user_token)
    user_token = strtok(NULL, ":"); // Skip the count

  // Reset user count but keep current user
  app_state.user_count = 1;

  while (user_token && app_state.user_count < MAX_USERS)
  {
    char username[MAX_USERNAME_LEN] = {0};
    int role = 0, is_online = 0;

    if (sscanf(user_token, "%[^,],%d,%d", username, &role, &is_online) == 3)
    {
      // Skip if this is current user
      if (strcmp(username, current_username) != 0)
      {
        // Add as a new user
        strncpy(app_state.users[app_state.user_count].username, username, MAX_USERNAME_LEN - 1);
        app_state.users[app_state.user_count].role = role;
        app_state.users[app_state.user_count].is_online = is_online;
        app_state.user_count++;
      }
    }

    user_token = strtok(NULL, ":");
  }

  // Make sure current user info is preserved
  strncpy(app_state.users[app_state.current_user_index].username, current_username, MAX_USERNAME_LEN - 1);
  strncpy(app_state.users[app_state.current_user_index].password, current_password, MAX_PASSWORD_LEN - 1);
  strncpy(app_state.users[app_state.current_user_index].email, current_email, MAX_EMAIL_LEN - 1);
  app_state.users[app_state.current_user_index].role = current_role;
  app_state.users[app_state.current_user_index].is_online = 1;
}

// Process channel message
void process_channel_message(NetworkMessage *msg)
{
  // Add message to the target channel
  int channel_id = msg->target_id - 1; // Convert to 0-based index
  if (channel_id >= 0 && channel_id < app_state.channel_count)
  {
    Channel *channel = &app_state.channels[channel_id];
    if (channel->message_count < MAX_MESSAGES)
    {
      Message *new_msg = &channel->messages[channel->message_count];
      strncpy(new_msg->text, msg->content, MAX_MESSAGE_LEN - 1);
      strncpy(new_msg->sender, msg->username, MAX_USERNAME_LEN - 1);
      new_msg->timestamp = time(NULL);
      channel->message_count++;
    }
    else
    {
      // Channel is full, shift messages to make room
      for (int i = 0; i < MAX_MESSAGES - 1; i++)
      {
        memcpy(&channel->messages[i], &channel->messages[i + 1], sizeof(Message));
      }

      // Add new message at the end
      Message *new_msg = &channel->messages[MAX_MESSAGES - 1];
      strncpy(new_msg->text, msg->content, MAX_MESSAGE_LEN - 1);
      strncpy(new_msg->sender, msg->username, MAX_USERNAME_LEN - 1);
      new_msg->timestamp = time(NULL);
    }
  }
}

// Process private message
void process_private_message(NetworkMessage *msg)
{
  // Create a dedicated DM channel if it doesn't exist yet
  char dm_channel_name[MAX_CHANNEL_NAME_LEN];
  snprintf(dm_channel_name, MAX_CHANNEL_NAME_LEN, "DM-%s", msg->username);

  // Look for existing DM channel
  int dm_channel_index = -1;
  for (int i = 0; i < app_state.channel_count; i++)
  {
    if (strcmp(app_state.channels[i].name, dm_channel_name) == 0)
    {
      dm_channel_index = i;
      break;
    }
  }

  // Create new DM channel if needed
  if (dm_channel_index == -1 && app_state.channel_count < MAX_CHANNELS)
  {
    dm_channel_index = app_state.channel_count;
    strncpy(app_state.channels[dm_channel_index].name, dm_channel_name, MAX_CHANNEL_NAME_LEN - 1);
    app_state.channels[dm_channel_index].message_count = 0;
    app_state.channel_count++;
  }

  // Add message to DM channel
  if (dm_channel_index >= 0 && dm_channel_index < app_state.channel_count)
  {
    Channel *channel = &app_state.channels[dm_channel_index];
    if (channel->message_count < MAX_MESSAGES)
    {
      Message *new_msg = &channel->messages[channel->message_count];
      strncpy(new_msg->text, msg->content, MAX_MESSAGE_LEN - 1);
      strncpy(new_msg->sender, msg->username, MAX_USERNAME_LEN - 1);
      new_msg->timestamp = time(NULL);
      channel->message_count++;
    }
    else
    {
      // Channel is full, shift messages
      for (int i = 0; i < MAX_MESSAGES - 1; i++)
      {
        memcpy(&channel->messages[i], &channel->messages[i + 1], sizeof(Message));
      }

      // Add new message at the end
      Message *new_msg = &channel->messages[MAX_MESSAGES - 1];
      strncpy(new_msg->text, msg->content, MAX_MESSAGE_LEN - 1);
      strncpy(new_msg->sender, msg->username, MAX_USERNAME_LEN - 1);
      new_msg->timestamp = time(NULL);
    }
  }
}

// Process user status update
void process_user_status(NetworkMessage *msg)
{
  // Update user status
  for (int i = 0; i < app_state.user_count; i++)
  {
    if (strcmp(app_state.users[i].username, msg->username) == 0)
    {
      app_state.users[i].is_online = msg->param;
      break;
    }
  }

  // If not found and user is online, add them to our local list
  if (msg->param && app_state.user_count < MAX_USERS)
  {
    int found = 0;
    for (int i = 0; i < app_state.user_count; i++)
    {
      if (strcmp(app_state.users[i].username, msg->username) == 0)
      {
        found = 1;
        break;
      }
    }

    if (!found && strcmp(msg->username, app_state.users[app_state.current_user_index].username) != 0)
    {
      strncpy(app_state.users[app_state.user_count].username, msg->username, MAX_USERNAME_LEN - 1);
      app_state.users[app_state.user_count].is_online = 1;
      app_state.users[app_state.user_count].role = ROLE_USER; // Default role
      app_state.user_count++;
    }
  }
}

// Authentication functions
int network_authenticate(const char *username, const char *password)
{
  if (network_socket == -1)
    return 0;

  NetworkMessage msg;
  msg.type = MSG_AUTH;
  strncpy(msg.username, username, MAX_USERNAME_LEN - 1);
  strncpy(msg.content, password, MAX_MESSAGE_LEN - 1);

  // Send authentication request
  if (send(network_socket, &msg, sizeof(NetworkMessage), 0) <= 0)
  {
    return 0;
  }

  // Response will be handled by the receive thread
  return 1;
}

int network_register(const char *username, const char *email, const char *password)
{
  if (network_socket == -1)
    return 0;

  NetworkMessage msg;
  msg.type = MSG_REGISTER;
  strncpy(msg.username, username, MAX_USERNAME_LEN - 1);

  // Concatenate email and password in content
  snprintf(msg.content, MAX_MESSAGE_LEN, "%s:%s", email, password);

  // Send registration request
  if (send(network_socket, &msg, sizeof(NetworkMessage), 0) <= 0)
  {
    return 0;
  }

  // Response will be handled by the receive thread
  return 1;
}

// Channel operations
int network_get_channels()
{
  if (network_socket == -1)
    return 0;

  NetworkMessage msg;
  msg.type = MSG_CHANNEL_LIST;

  // Send channel list request
  if (send(network_socket, &msg, sizeof(NetworkMessage), 0) <= 0)
  {
    return 0;
  }

  // Response will be handled by the receive thread
  return 1;
}

int network_create_channel(const char *name)
{
  if (network_socket == -1)
    return 0;

  NetworkMessage msg;
  msg.type = MSG_CREATE_CHANNEL;
  msg.user_id = network_user_id;
  strncpy(msg.content, name, MAX_MESSAGE_LEN - 1);

  // Send create channel request
  if (send(network_socket, &msg, sizeof(NetworkMessage), 0) <= 0)
  {
    return 0;
  }

  return 1;
}

int network_delete_channel(const char *name)
{
  if (network_socket == -1)
    return 0;

  NetworkMessage msg;
  msg.type = MSG_DELETE_CHANNEL;
  msg.user_id = network_user_id;
  strncpy(msg.content, name, MAX_MESSAGE_LEN - 1);

  // Send delete channel request
  if (send(network_socket, &msg, sizeof(NetworkMessage), 0) <= 0)
  {
    return 0;
  }

  return 1;
}

// Messaging functions
int network_send_channel_message(int channel_id, const char *message)
{
  if (network_socket == -1)
    return 0;

  NetworkMessage msg;
  msg.type = MSG_CHANNEL_MSG;
  msg.user_id = network_user_id;
  msg.target_id = channel_id;
  strncpy(msg.content, message, MAX_MESSAGE_LEN - 1);

  // Send channel message
  if (send(network_socket, &msg, sizeof(NetworkMessage), 0) <= 0)
  {
    return 0;
  }

  return 1;
}

int network_send_private_message(const char *username, const char *message)
{
  if (network_socket == -1)
    return 0;

  NetworkMessage msg;
  msg.type = MSG_PRIVATE_MSG;
  msg.user_id = network_user_id;
  strncpy(msg.username, username, MAX_USERNAME_LEN - 1);
  strncpy(msg.content, message, MAX_MESSAGE_LEN - 1);

  // Send private message
  if (send(network_socket, &msg, sizeof(NetworkMessage), 0) <= 0)
  {
    return 0;
  }

  return 1;
}

// User management functions
int network_get_users()
{
  if (network_socket == -1)
    return 0;

  NetworkMessage msg;
  msg.type = MSG_USER_LIST;

  // Send user list request
  if (send(network_socket, &msg, sizeof(NetworkMessage), 0) <= 0)
  {
    return 0;
  }

  // Response will be handled by the receive thread
  return 1;
}

int network_mute_user(const char *username, int channel_id, int minutes)
{
  if (network_socket == -1)
    return 0;

  NetworkMessage msg;
  msg.type = MSG_MUTE_USER;
  msg.user_id = network_user_id;
  msg.target_id = channel_id;
  msg.param = minutes;
  strncpy(msg.username, username, MAX_USERNAME_LEN - 1);

  // Send mute user request
  if (send(network_socket, &msg, sizeof(NetworkMessage), 0) <= 0)
  {
    return 0;
  }

  return 1;
}

int network_set_user_role(const char *username, int role)
{
  if (network_socket == -1)
    return 0;

  NetworkMessage msg;
  msg.type = MSG_SET_ROLE;
  msg.user_id = network_user_id;
  msg.param = role;
  strncpy(msg.username, username, MAX_USERNAME_LEN - 1);

  // Send set user role request
  if (send(network_socket, &msg, sizeof(NetworkMessage), 0) <= 0)
  {
    return 0;
  }

  return 1;
}