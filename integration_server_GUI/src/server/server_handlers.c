#include "server.h"

// Handle authentication message
void handle_auth_message(ClientConnection *client, NetworkMessage *msg)
{
  NetworkMessage response;
  int authenticated = 0;
  int user_id = -1;

  // Lock users to check credentials
  pthread_mutex_lock(&server_ctx.users_mutex);

  // Find user
  for (int i = 0; i < server_ctx.user_count; i++)
  {
    if (strcmp(server_ctx.users[i].username, msg->username) == 0 &&
        strcmp(server_ctx.users[i].password, msg->content) == 0)
    {
      authenticated = 1;
      user_id = i;
      server_ctx.users[i].is_online = 1;
      break;
    }
  }

  pthread_mutex_unlock(&server_ctx.users_mutex);

  if (authenticated)
  {
    // Authentication successful
    client->user_id = user_id;
    client->is_authenticated = 1;
    strncpy(client->username, msg->username, MAX_USERNAME_LEN - 1);

    log_info("User authenticated: %s (ID: %d)", client->username, client->user_id);

    // Prepare response
    response.type = MSG_AUTH_RESPONSE;
    response.user_id = user_id;
    strncpy(response.username, msg->username, MAX_USERNAME_LEN - 1);
    strncpy(response.content, "Authentication successful", MAX_MESSAGE_LEN - 1);

    // Send response
    if (send(client->socket, &response, sizeof(NetworkMessage), 0) <= 0)
    {
      log_error("Failed to send auth response: %s", strerror(errno));
      return;
    }

    // Send user list and channel list
    handle_user_list_request(client);
    handle_channel_list_request(client);

    // Notify other clients about new online user
    NetworkMessage status_msg;
    status_msg.type = MSG_USER_STATUS;
    status_msg.user_id = user_id;
    status_msg.param = 1; // Online
    strncpy(status_msg.username, msg->username, MAX_USERNAME_LEN - 1);

    pthread_mutex_lock(&server_ctx.clients_mutex);
    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
      if (server_ctx.clients[i].socket != -1 &&
          server_ctx.clients[i].is_authenticated &&
          server_ctx.clients[i].socket != client->socket)
      {
        send(server_ctx.clients[i].socket, &status_msg, sizeof(NetworkMessage), 0);
      }
    }
    pthread_mutex_unlock(&server_ctx.clients_mutex);
  }
  else
  {
    // Authentication failed
    log_info("Authentication failed for user: %s", msg->username);

    response.type = MSG_ERROR;
    response.user_id = -1;
    strncpy(response.content, "Invalid username or password", MAX_MESSAGE_LEN - 1);
    send(client->socket, &response, sizeof(NetworkMessage), 0);
  }
}

// Handle registration message
void handle_register_message(ClientConnection *client, NetworkMessage *msg)
{
  // Extract email and password from content
  char email[MAX_EMAIL_LEN] = {0};
  char password[MAX_PASSWORD_LEN] = {0};
  sscanf(msg->content, "%[^:]:%s", email, password);

  pthread_mutex_lock(&server_ctx.users_mutex);

  // Check if username already exists
  int username_exists = 0;
  for (int i = 0; i < server_ctx.user_count; i++)
  {
    if (strcmp(server_ctx.users[i].username, msg->username) == 0)
    {
      username_exists = 1;
      break;
    }
  }

  NetworkMessage response;

  if (!username_exists && server_ctx.user_count < MAX_USERS)
  {
    // Add new user
    int new_user_id = server_ctx.user_count;

    strncpy(server_ctx.users[new_user_id].username, msg->username, MAX_USERNAME_LEN - 1);
    strncpy(server_ctx.users[new_user_id].email, email, MAX_EMAIL_LEN - 1);
    strncpy(server_ctx.users[new_user_id].password, password, MAX_PASSWORD_LEN - 1);
    server_ctx.users[new_user_id].role = ROLE_USER;
    server_ctx.users[new_user_id].is_online = 1;

    server_ctx.user_count++;

    // Authentication successful
    client->user_id = new_user_id;
    client->is_authenticated = 1;
    strncpy(client->username, msg->username, MAX_USERNAME_LEN - 1);

    log_info("New user registered: %s (ID: %d)", client->username, client->user_id);

    // Prepare response
    response.type = MSG_REG_RESPONSE;
    response.user_id = new_user_id;
    strncpy(response.username, msg->username, MAX_USERNAME_LEN - 1);
    strncpy(response.content, "Registration successful", MAX_MESSAGE_LEN - 1);

    // Unlock before sending notification
    pthread_mutex_unlock(&server_ctx.users_mutex);

    // Send response
    if (send(client->socket, &response, sizeof(NetworkMessage), 0) <= 0)
    {
      log_error("Failed to send registration response: %s", strerror(errno));
      return;
    }

    // Send channel and user lists
    handle_channel_list_request(client);
    handle_user_list_request(client);

    // Notify other clients about new user
    NetworkMessage status_msg;
    status_msg.type = MSG_USER_STATUS;
    status_msg.user_id = new_user_id;
    status_msg.param = 1; // Online
    strncpy(status_msg.username, msg->username, MAX_USERNAME_LEN - 1);

    pthread_mutex_lock(&server_ctx.clients_mutex);
    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
      if (server_ctx.clients[i].socket != -1 &&
          server_ctx.clients[i].is_authenticated &&
          server_ctx.clients[i].socket != client->socket)
      {
        send(server_ctx.clients[i].socket, &status_msg, sizeof(NetworkMessage), 0);
      }
    }
    pthread_mutex_unlock(&server_ctx.clients_mutex);
  }
  else
  {
    // Registration failed
    response.type = MSG_ERROR;
    response.user_id = -1;

    if (username_exists)
    {
      strncpy(response.content, "Username already exists", MAX_MESSAGE_LEN - 1);
      log_info("Registration failed: username %s already exists", msg->username);
    }
    else
    {
      strncpy(response.content, "Maximum user limit reached", MAX_MESSAGE_LEN - 1);
      log_info("Registration failed: maximum user limit reached");
    }

    pthread_mutex_unlock(&server_ctx.users_mutex);

    // Send response
    send(client->socket, &response, sizeof(NetworkMessage), 0);
  }
}

// Handle channel list request
void handle_channel_list_request(ClientConnection *client)
{
  NetworkMessage response;
  response.type = MSG_CHANNEL_LIST;
  response.user_id = client->user_id;

  // Prepare channel list
  pthread_mutex_lock(&server_ctx.channels_mutex);

  log_info("Sending channel list to %s (channel count: %d)",
           client->username, server_ctx.channel_count);

  char channels_str[MAX_MESSAGE_LEN] = {0};
  int offset = snprintf(channels_str, MAX_MESSAGE_LEN, "%d", server_ctx.channel_count);

  for (int i = 0; i < server_ctx.channel_count; i++)
  {
    offset += snprintf(channels_str + offset, MAX_MESSAGE_LEN - offset,
                       ":%s", server_ctx.channels[i].name);

    if (offset >= MAX_MESSAGE_LEN - 1)
    {
      // Buffer is full, stop adding channels
      break;
    }
  }

  pthread_mutex_unlock(&server_ctx.channels_mutex);

  strncpy(response.content, channels_str, MAX_MESSAGE_LEN - 1);

  // Send response
  if (send(client->socket, &response, sizeof(NetworkMessage), 0) <= 0)
  {
    log_error("Failed to send channel list to client: %s", strerror(errno));
  }
  else
  {
    log_info("Channel list sent to client: %s", channels_str);
  }
}

// Handle user list request
void handle_user_list_request(ClientConnection *client)
{
  NetworkMessage response;
  response.type = MSG_USER_LIST;
  response.user_id = client->user_id;

  // Prepare user list
  pthread_mutex_lock(&server_ctx.users_mutex);

  log_info("Sending user list to %s (user count: %d)",
           client->username, server_ctx.user_count);

  char users_str[MAX_MESSAGE_LEN] = {0};
  int offset = snprintf(users_str, MAX_MESSAGE_LEN, "%d", server_ctx.user_count);

  for (int i = 0; i < server_ctx.user_count; i++)
  {
    offset += snprintf(users_str + offset, MAX_MESSAGE_LEN - offset,
                       ":%s,%d,%d", server_ctx.users[i].username,
                       server_ctx.users[i].role, server_ctx.users[i].is_online);

    if (offset >= MAX_MESSAGE_LEN - 1)
    {
      // Buffer is full, stop adding users
      break;
    }
  }

  pthread_mutex_unlock(&server_ctx.users_mutex);

  strncpy(response.content, users_str, MAX_MESSAGE_LEN - 1);

  // Send response
  if (send(client->socket, &response, sizeof(NetworkMessage), 0) <= 0)
  {
    log_error("Failed to send user list to client: %s", strerror(errno));
  }
  else
  {
    log_info("User list sent to client: %s", users_str);
  }
}

// Handle channel message
void handle_channel_message(ClientConnection *client, NetworkMessage *msg)
{
  int channel_id = msg->target_id - 1; // Convert to 0-based index

  pthread_mutex_lock(&server_ctx.channels_mutex);

  // Validate channel
  if (channel_id < 0 || channel_id >= server_ctx.channel_count)
  {
    pthread_mutex_unlock(&server_ctx.channels_mutex);
    log_error("Invalid channel ID: %d (valid range: 0-%d)",
              channel_id, server_ctx.channel_count - 1);

    send_error_message(client, "Invalid channel ID");
    return;
  }

  // Add message to channel history
  Channel *channel = &server_ctx.channels[channel_id];
  if (channel->message_count < MAX_MESSAGES)
  {
    Message *message = &channel->messages[channel->message_count];
    strncpy(message->text, msg->content, MAX_MESSAGE_LEN - 1);
    strncpy(message->sender, client->username, MAX_USERNAME_LEN - 1);
    message->timestamp = time(NULL);
    channel->message_count++;

    log_info("Added message to channel %s: %s", channel->name, msg->content);
  }
  else
  {
    // Channel is full, shift messages to make room
    for (int i = 0; i < MAX_MESSAGES - 1; i++)
    {
      memcpy(&channel->messages[i], &channel->messages[i + 1], sizeof(Message));
    }

    // Add new message at the end
    Message *message = &channel->messages[MAX_MESSAGES - 1];
    strncpy(message->text, msg->content, MAX_MESSAGE_LEN - 1);
    strncpy(message->sender, client->username, MAX_USERNAME_LEN - 1);
    message->timestamp = time(NULL);

    log_info("Channel %s is full, shifted messages to add: %s",
             channel->name, msg->content);
  }

  pthread_mutex_unlock(&server_ctx.channels_mutex);

  // Broadcast message to all clients
  NetworkMessage broadcast;
  broadcast.type = MSG_CHANNEL_MSG;
  broadcast.user_id = client->user_id;
  broadcast.target_id = msg->target_id;
  strncpy(broadcast.username, client->username, MAX_USERNAME_LEN - 1);
  strncpy(broadcast.content, msg->content, MAX_MESSAGE_LEN - 1);

  broadcast_to_channel(channel_id + 1, &broadcast);
}

// Handle private message
void handle_private_message(ClientConnection *client, NetworkMessage *msg)
{
  // Find target user ID
  int target_user_id = -1;

  pthread_mutex_lock(&server_ctx.users_mutex);

  for (int i = 0; i < server_ctx.user_count; i++)
  {
    if (strcmp(server_ctx.users[i].username, msg->username) == 0)
    {
      target_user_id = i;
      break;
    }
  }

  pthread_mutex_unlock(&server_ctx.users_mutex);

  if (target_user_id == -1)
  {
    // User not found
    log_error("Private message target user not found: %s", msg->username);
    send_error_message(client, "User not found");
    return;
  }

  log_info("Private message from %s to %s: %s",
           client->username, msg->username, msg->content);

  // Forward message to recipient
  NetworkMessage forward;
  forward.type = MSG_PRIVATE_MSG;
  forward.user_id = client->user_id;
  forward.target_id = target_user_id;
  strncpy(forward.username, client->username, MAX_USERNAME_LEN - 1);
  strncpy(forward.content, msg->content, MAX_MESSAGE_LEN - 1);

  send_to_user(target_user_id, &forward);

  // Also send a copy back to the sender as confirmation
  if (send(client->socket, &forward, sizeof(NetworkMessage), 0) <= 0)
  {
    log_error("Failed to send private message confirmation: %s", strerror(errno));
  }
}

// Handle create channel
void handle_create_channel(ClientConnection *client, NetworkMessage *msg)
{
  // Check if user is admin (role 3)
  int is_admin = 0;

  pthread_mutex_lock(&server_ctx.users_mutex);

  if (client->user_id >= 0 && client->user_id < server_ctx.user_count)
  {
    is_admin = (server_ctx.users[client->user_id].role == ROLE_ADMIN);
  }

  pthread_mutex_unlock(&server_ctx.users_mutex);

  if (!is_admin)
  {
    // Not authorized
    log_info("User %s attempted to create channel without admin permissions",
             client->username);

    send_error_message(client, "You don't have permission to create channels");
    return;
  }

  // Check for space and duplicate names
  pthread_mutex_lock(&server_ctx.channels_mutex);

  int duplicate = 0;
  for (int i = 0; i < server_ctx.channel_count; i++)
  {
    if (strcmp(server_ctx.channels[i].name, msg->content) == 0)
    {
      duplicate = 1;
      break;
    }
  }

  if (duplicate || server_ctx.channel_count >= MAX_CHANNELS)
  {
    pthread_mutex_unlock(&server_ctx.channels_mutex);

    // Failed to create channel
    if (duplicate)
    {
      log_info("Channel creation failed: name already exists (%s)", msg->content);
      send_error_message(client, "Channel name already exists");
    }
    else
    {
      log_info("Channel creation failed: maximum limit reached");
      send_error_message(client, "Maximum channel limit reached");
    }

    return;
  }

  // Create new channel
  strncpy(server_ctx.channels[server_ctx.channel_count].name, msg->content, MAX_CHANNEL_NAME_LEN - 1);
  server_ctx.channels[server_ctx.channel_count].message_count = 0;
  server_ctx.channel_count++;

  log_info("Channel created: %s (total: %d)", msg->content, server_ctx.channel_count);

  pthread_mutex_unlock(&server_ctx.channels_mutex);

  // Notify all clients about new channel
  NetworkMessage notification;
  notification.type = MSG_SUCCESS;
  notification.user_id = client->user_id;
  snprintf(notification.content, MAX_MESSAGE_LEN, "Channel '%s' created", msg->content);

  pthread_mutex_lock(&server_ctx.clients_mutex);
  for (int i = 0; i < MAX_CONNECTIONS; i++)
  {
    if (server_ctx.clients[i].socket != -1 && server_ctx.clients[i].is_authenticated)
    {
      send(server_ctx.clients[i].socket, &notification, sizeof(NetworkMessage), 0);
    }
  }
  pthread_mutex_unlock(&server_ctx.clients_mutex);

  // Also send an updated channel list to all clients
  pthread_mutex_lock(&server_ctx.clients_mutex);
  for (int i = 0; i < MAX_CONNECTIONS; i++)
  {
    if (server_ctx.clients[i].socket != -1 && server_ctx.clients[i].is_authenticated)
    {
      handle_channel_list_request(&server_ctx.clients[i]);
    }
  }
  pthread_mutex_unlock(&server_ctx.clients_mutex);
}

// Handle delete channel
void handle_delete_channel(ClientConnection *client, NetworkMessage *msg)
{
  // Check if user is admin
  int is_admin = 0;

  pthread_mutex_lock(&server_ctx.users_mutex);

  if (client->user_id >= 0 && client->user_id < server_ctx.user_count)
  {
    is_admin = (server_ctx.users[client->user_id].role == ROLE_ADMIN);
  }

  pthread_mutex_unlock(&server_ctx.users_mutex);

  if (!is_admin)
  {
    // Not authorized
    send_error_message(client, "You don't have permission to delete channels");
    return;
  }

  // Find the channel
  int channel_id = -1;

  pthread_mutex_lock(&server_ctx.channels_mutex);

  for (int i = 0; i < server_ctx.channel_count; i++)
  {
    if (strcmp(server_ctx.channels[i].name, msg->content) == 0)
    {
      channel_id = i;
      break;
    }
  }

  if (channel_id == -1 || channel_id < 3) // Don't allow deletion of default channels
  {
    pthread_mutex_unlock(&server_ctx.channels_mutex);

    if (channel_id < 3 && channel_id >= 0)
    {
      send_error_message(client, "Cannot delete default channels");
    }
    else
    {
      send_error_message(client, "Channel not found");
    }

    return;
  }

  // Remove the channel by shifting all channels after it
  for (int i = channel_id; i < server_ctx.channel_count - 1; i++)
  {
    memcpy(&server_ctx.channels[i], &server_ctx.channels[i + 1], sizeof(Channel));
  }

  server_ctx.channel_count--;

  log_info("Channel deleted: %s (new total: %d)", msg->content, server_ctx.channel_count);

  pthread_mutex_unlock(&server_ctx.channels_mutex);

  // Notify all clients
  NetworkMessage notification;
  notification.type = MSG_SUCCESS;
  notification.user_id = client->user_id;
  snprintf(notification.content, MAX_MESSAGE_LEN, "Channel '%s' deleted", msg->content);

  pthread_mutex_lock(&server_ctx.clients_mutex);
  for (int i = 0; i < MAX_CONNECTIONS; i++)
  {
    if (server_ctx.clients[i].socket != -1 && server_ctx.clients[i].is_authenticated)
    {
      send(server_ctx.clients[i].socket, &notification, sizeof(NetworkMessage), 0);

      // Also send an updated channel list
      handle_channel_list_request(&server_ctx.clients[i]);
    }
  }
  pthread_mutex_unlock(&server_ctx.clients_mutex);
}

// Handle mute user
void handle_mute_user(ClientConnection *client, NetworkMessage *msg)
{
  // Check if user is admin or moderator
  int has_permission = 0;

  pthread_mutex_lock(&server_ctx.users_mutex);

  if (client->user_id >= 0 && client->user_id < server_ctx.user_count)
  {
    int role = server_ctx.users[client->user_id].role;
    has_permission = (role == ROLE_ADMIN || role == ROLE_MODERATOR);
  }

  pthread_mutex_unlock(&server_ctx.users_mutex);

  if (!has_permission)
  {
    send_error_message(client, "You don't have permission to mute users");
    return;
  }

  // Find the target user
  int target_user_id = -1;

  pthread_mutex_lock(&server_ctx.users_mutex);

  for (int i = 0; i < server_ctx.user_count; i++)
  {
    if (strcmp(server_ctx.users[i].username, msg->username) == 0)
    {
      target_user_id = i;
      break;
    }
  }

  if (target_user_id == -1)
  {
    pthread_mutex_unlock(&server_ctx.users_mutex);
    send_error_message(client, "User not found");
    return;
  }

  // Check if target is an admin (admins can't be muted)
  if (server_ctx.users[target_user_id].role == ROLE_ADMIN)
  {
    pthread_mutex_unlock(&server_ctx.users_mutex);
    send_error_message(client, "Cannot mute an admin");
    return;
  }

  // Set mute time in the specified channel
  int channel_id = msg->target_id - 1; // Convert to 0-based index

  if (channel_id < 0 || channel_id >= server_ctx.channel_count)
  {
    pthread_mutex_unlock(&server_ctx.users_mutex);
    send_error_message(client, "Invalid channel ID");
    return;
  }

  int minutes = msg->param;
  if (minutes <= 0)
    minutes = 10; // Default mute time

  time_t mute_until = time(NULL) + (minutes * 60);
  server_ctx.users[target_user_id].muted_until[channel_id] = mute_until;

  pthread_mutex_unlock(&server_ctx.users_mutex);

  log_info("User %s muted in channel %d for %d minutes",
           msg->username, channel_id + 1, minutes);

  // Notify the muted user
  NetworkMessage notification;
  notification.type = MSG_ERROR;
  notification.user_id = client->user_id;
  snprintf(notification.content, MAX_MESSAGE_LEN,
           "You have been muted in %s for %d minutes",
           server_ctx.channels[channel_id].name, minutes);

  send_to_user(target_user_id, &notification);

  // Send confirmation to the admin/mod
  NetworkMessage confirm;
  confirm.type = MSG_SUCCESS;
  confirm.user_id = client->user_id;
  snprintf(confirm.content, MAX_MESSAGE_LEN,
           "User %s has been muted in %s for %d minutes",
           msg->username, server_ctx.channels[channel_id].name, minutes);

  send(client->socket, &confirm, sizeof(NetworkMessage), 0);
}

// Handle set role
void handle_set_role(ClientConnection *client, NetworkMessage *msg)
{
  // Check if user is admin
  int is_admin = 0;

  pthread_mutex_lock(&server_ctx.users_mutex);

  if (client->user_id >= 0 && client->user_id < server_ctx.user_count)
  {
    is_admin = (server_ctx.users[client->user_id].role == ROLE_ADMIN);
  }

  pthread_mutex_unlock(&server_ctx.users_mutex);

  if (!is_admin)
  {
    send_error_message(client, "You don't have permission to change roles");
    return;
  }

  // Find the target user
  int target_user_id = -1;

  pthread_mutex_lock(&server_ctx.users_mutex);

  for (int i = 0; i < server_ctx.user_count; i++)
  {
    if (strcmp(server_ctx.users[i].username, msg->username) == 0)
    {
      target_user_id = i;
      break;
    }
  }

  if (target_user_id == -1)
  {
    pthread_mutex_unlock(&server_ctx.users_mutex);
    send_error_message(client, "User not found");
    return;
  }

  // Set the new role
  int new_role = msg->param;
  if (new_role < ROLE_USER || new_role > ROLE_ADMIN)
  {
    pthread_mutex_unlock(&server_ctx.users_mutex);
    send_error_message(client, "Invalid role value");
    return;
  }

  server_ctx.users[target_user_id].role = new_role;

  log_info("User %s role changed to %d", msg->username, new_role);

  pthread_mutex_unlock(&server_ctx.users_mutex);

  // Notify the user about their new role
  NetworkMessage notification;
  notification.type = MSG_SUCCESS;
  notification.user_id = client->user_id;

  const char *role_name;
  switch (new_role)
  {
  case ROLE_ADMIN:
    role_name = "Admin";
    break;
  case ROLE_MODERATOR:
    role_name = "Moderator";
    break;
  default:
    role_name = "User";
    break;
  }

  snprintf(notification.content, MAX_MESSAGE_LEN,
           "Your role has been changed to %s", role_name);

  send_to_user(target_user_id, &notification);

  // Send confirmation to the admin
  NetworkMessage confirm;
  confirm.type = MSG_SUCCESS;
  confirm.user_id = client->user_id;
  snprintf(confirm.content, MAX_MESSAGE_LEN,
           "User %s role changed to %s", msg->username, role_name);

  send(client->socket, &confirm, sizeof(NetworkMessage), 0);

  // Send updated user list to all clients
  pthread_mutex_lock(&server_ctx.clients_mutex);
  for (int i = 0; i < MAX_CONNECTIONS; i++)
  {
    if (server_ctx.clients[i].socket != -1 && server_ctx.clients[i].is_authenticated)
    {
      handle_user_list_request(&server_ctx.clients[i]);
    }
  }
  pthread_mutex_unlock(&server_ctx.clients_mutex);
}

// Broadcast message to all clients in a channel
void broadcast_to_channel(int channel_id, NetworkMessage *msg)
{
  log_info("Broadcasting message to channel %d", channel_id);

  pthread_mutex_lock(&server_ctx.clients_mutex);

  for (int i = 0; i < MAX_CONNECTIONS; i++)
  {
    if (server_ctx.clients[i].socket != -1 && server_ctx.clients[i].is_authenticated)
    {
      if (send(server_ctx.clients[i].socket, msg, sizeof(NetworkMessage), 0) <= 0)
      {
        log_error("Failed to send to client %d: %s", i, strerror(errno));
      }
    }
  }

  pthread_mutex_unlock(&server_ctx.clients_mutex);
}

// Send a message to a specific user
void send_to_user(int user_id, NetworkMessage *msg)
{
  log_info("Sending message to user_id %d", user_id);

  pthread_mutex_lock(&server_ctx.clients_mutex);

  int sent = 0;
  for (int i = 0; i < MAX_CONNECTIONS; i++)
  {
    if (server_ctx.clients[i].socket != -1 &&
        server_ctx.clients[i].is_authenticated &&
        server_ctx.clients[i].user_id == user_id)
    {

      if (send(server_ctx.clients[i].socket, msg, sizeof(NetworkMessage), 0) <= 0)
      {
        log_error("Failed to send message to user %d: %s", user_id, strerror(errno));
      }
      else
      {
        sent = 1;
      }
      break;
    }
  }

  if (!sent)
  {
    log_info("User with ID %d not found or not connected", user_id);
  }

  pthread_mutex_unlock(&server_ctx.clients_mutex);
}