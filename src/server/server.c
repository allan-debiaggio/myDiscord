#include "../../include/server.h"

// Global server context
ServerContext server;

// Signal handler for graceful shutdown
void handle_signal(int sig)
{
  extern ServerContext server; // Reference the global server context

  if (sig == SIGINT || sig == SIGTERM)
  {
    server.running = false;
    log_message("Server shutting down due to signal %d...", sig);
  }
}

// Initialize server
int initialize_server(ServerContext *server, int port)
{
  if (!server)
  {
    log_message("Cannot initialize NULL server context");
    return -1;
  }

  server->running = 1;
  server->port = port;

  int opt = 1;

  // Create socket
  if ((server->socket = socket(AF_INET, SOCK_STREAM, 0)) <= 0)
  {
    log_message("Socket creation failed: %s", strerror(errno));
    return -1;
  }

  // Set socket options to reuse the address and port
  if (setsockopt(server->socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
  {
    log_message("setsockopt SO_REUSEADDR failed: %s", strerror(errno));
    close(server->socket);
    return -1;
  }

  // Configure server address
  memset(&server->address, 0, sizeof(server->address));
  server->address.sin_family = AF_INET;
  server->address.sin_addr.s_addr = INADDR_ANY;
  server->address.sin_port = htons(port);

  // Bind socket to port
  if (bind(server->socket, (struct sockaddr *)&server->address, sizeof(server->address)) < 0)
  {
    log_message("Bind failed: %s", strerror(errno));
    close(server->socket);
    return -1;
  }

  // Listen for connections
  if (listen(server->socket, MAX_PENDING_CONNECTIONS) < 0)
  {
    log_message("Listen failed: %s", strerror(errno));
    close(server->socket);
    return -1;
  }

  log_message("Server initialized and listening on port %d", port);
  return 0;
}

// Create a new channel
int create_channel(ServerContext *server, const char *name, const char *description, bool is_public)
{
  if (!server || !name || strlen(name) == 0)
  {
    log_message("Invalid parameters for channel creation");
    return -1;
  }

  pthread_mutex_lock(&server->channels_mutex);

  // Check if channel already exists
  int existing = find_channel_by_name(server, name);
  if (existing != -1)
  {
    pthread_mutex_unlock(&server->channels_mutex);
    log_message("Channel '%s' already exists with ID %d", name, existing);
    return existing; // Return existing channel ID
  }

  // Find empty slot in channels array
  int channel_index = -1;
  for (int i = 0; i < MAX_CHANNELS; i++)
  {
    if (!server->channels[i].active)
    {
      channel_index = i;
      break;
    }
  }

  if (channel_index == -1 || server->channel_count >= MAX_CHANNELS)
  {
    pthread_mutex_unlock(&server->channels_mutex);
    log_message("Cannot create channel: maximum number of channels reached");
    return -1;
  }

  // Set up channel
  server->channels[channel_index].channel_id = channel_index + 1; // IDs start from 1
  server->channels[channel_index].active = true;
  server->channels[channel_index].is_public = is_public;
  strncpy(server->channels[channel_index].name, name, MAX_CHANNEL_NAME_LEN - 1);
  server->channels[channel_index].name[MAX_CHANNEL_NAME_LEN - 1] = '\0';
  strncpy(server->channels[channel_index].description, description, BUFFER_SIZE - 1);
  server->channels[channel_index].description[BUFFER_SIZE - 1] = '\0';

  server->channel_count++;

  int new_channel_id = server->channels[channel_index].channel_id;

  pthread_mutex_unlock(&server->channels_mutex);

  // Create the channel in the database if available
  if (server->db && server->db->connected)
  {
    db_create_channel(server->db, name, description ? description : "");
  }

  printf("Created channel: %s (ID: %d)\n", name, new_channel_id);
  return new_channel_id;
}

// Find a channel by name, returns channel ID or -1 if not found
int find_channel_by_name(ServerContext *server, const char *name)
{
  for (int i = 0; i < MAX_CHANNELS; i++)
  {
    if (server->channels[i].active &&
        strncmp(server->channels[i].name, name, MAX_CHANNEL_NAME_LEN) == 0)
    {
      return server->channels[i].channel_id;
    }
  }
  return -1;
}

// Send channel list to a client
void send_channel_list(ServerContext *server, int client_socket)
{
  char channel_list[BUFFER_SIZE] = {0};
  int offset = 0, remain = BUFFER_SIZE - 1;

  pthread_mutex_lock(&server->channels_mutex);

  // Format channel information as comma-separated list
  for (int i = 0; i < MAX_CHANNELS; i++)
  {
    if (server->channels[i].active)
    {
      const char *description = "";
      if (server->channels[i].description[0] != '\0')
      {
        description = server->channels[i].description;
      }

      int written = snprintf(
          channel_list + offset, remain,
          "%d:%s:%s:%d%s",
          i,                             // Channel ID
          server->channels[i].name,      // Channel name
          description,                   // Description
          server->channels[i].is_public, // Public flag (1/0)
          (offset > 0) ? "," : ""        // Comma separator
      );

      if (written >= remain || written < 0)
      {
        // Buffer full or error, stop adding channels
        log_message("Channel list truncated due to buffer limitations");
        break;
      }

      offset += written;
      remain -= written;
    }
  }

  pthread_mutex_unlock(&server->channels_mutex);

  // Send channel list as a message
  Message msg;
  memset(&msg, 0, sizeof(Message));
  msg.type = MSG_CHANNEL_LIST;
  strncpy(msg.content, channel_list, BUFFER_SIZE - 1);
  strncpy(msg.username, "SERVER", MAX_USERNAME_LEN - 1);
  msg.timestamp = time(NULL);

  if (send(client_socket, &msg, sizeof(Message), 0) < 0)
  {
    log_message("Failed to send channel list: %s", strerror(errno));
  }
  else
  {
    log_message("Sent channel list: %s", channel_list);
  }
}

// Broadcast a message to all clients in a specific channel
void channel_broadcast(ServerContext *server, Message *message, int sender_socket, int channel_id)
{
  pthread_mutex_lock(&server->clients_mutex);

  // Store message in the database if content is provided
  if (server->db && server->db->connected && message->content[0] != '\0' && message->type == MSG_TEXT)
  {
    db_store_message(server->db, message->channel_id, message->user_id,
                     message->username, message->timestamp, message->type, message->content);
  }

  for (int i = 0; i < MAX_CLIENTS; i++)
  {
    // Only send to active clients in the specified channel
    if (server->clients[i].socket != 0 &&
        server->clients[i].active &&
        server->clients[i].socket != sender_socket &&
        server->clients[i].current_channel_id == channel_id)
    {
      if (send(server->clients[i].socket, message, sizeof(Message), 0) < 0)
      {
        log_message("Send failed to client %s (socket %d) in channel %d: %s. Closing connection.",
                    server->clients[i].username, server->clients[i].socket, channel_id, strerror(errno));
        server->clients[i].active = false; // Mark inactive immediately
        close(server->clients[i].socket);  // Close socket immediately
        server->clients[i].socket = 0;     // Reset socket fd
      }
    }
  }

  pthread_mutex_unlock(&server->clients_mutex);
}

// Broadcast a message to all clients (used for non-channel specific messages like connect/disconnect/server notices)
void broadcast_message(ServerContext *server, Message *message, int sender_socket)
{
  pthread_mutex_lock(&server->clients_mutex);

  // Ensure message strings are null-terminated before broadcasting
  if (message)
  {
    message->username[MAX_USERNAME_LEN - 1] = '\0';
    message->content[BUFFER_SIZE - 1] = '\0';
  }
  else
  {
    log_message("Warning: Attempted to broadcast NULL message");
    pthread_mutex_unlock(&server->clients_mutex);
    return;
  }

  for (int i = 0; i < MAX_CLIENTS; i++)
  {
    if (server->clients[i].socket != 0 &&
        server->clients[i].active &&
        server->clients[i].socket != sender_socket)
    {
      if (send(server->clients[i].socket, message, sizeof(Message), 0) < 0)
      {
        log_message("Broadcast send failed to client %s (socket %d): %s. Closing connection.",
                    server->clients[i].username, server->clients[i].socket, strerror(errno));
        server->clients[i].active = false; // Mark inactive immediately
        close(server->clients[i].socket);  // Close socket immediately
        server->clients[i].socket = 0;     // Reset socket fd
      }
    }
  }

  pthread_mutex_unlock(&server->clients_mutex);
}

// Handle client messages
void *handle_client(void *arg)
{
  HandlerArgs *args = (HandlerArgs *)arg;
  ServerContext *server = args->server;
  int client_socket = args->client_socket;
  struct sockaddr_in client_address = args->client_address;
  free(args); // Args struct is no longer needed after copying data

  char client_ip[INET_ADDRSTRLEN];
  // Ensure client_address is valid before using inet_ntop
  if (inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN) == NULL)
  {
    log_message("Failed to get client IP address: %s", strerror(errno));
    strncpy(client_ip, "?.?.?.?", INET_ADDRSTRLEN);
  }
  log_message("New connection from %s:%d (socket %d)",
              client_ip, ntohs(client_address.sin_port), client_socket);

  // Get the username from the client
  Message message;
  ssize_t initial_recv = recv(client_socket, &message, sizeof(Message), 0);
  if (initial_recv <= 0)
  {
    if (initial_recv == 0)
    {
      log_message("Client disconnected immediately (socket %d)", client_socket);
    }
    else
    {
      log_message("Initial username receive failed for socket %d: %s", client_socket, strerror(errno));
    }
    close(client_socket);
    return NULL;
  }

  // Check if the received message is a connection message
  if (message.type != MSG_CONNECT)
  {
    log_message("Invalid initial message type %d from client (socket %d)", message.type, client_socket);
    close(client_socket);
    return NULL;
  }

  // Check if username is provided
  if (strlen(message.username) == 0)
  {
    log_message("Empty username provided by client (socket %d), assigning anonymous", client_socket);
    strncpy(message.username, "anonymous", MAX_USERNAME_LEN - 1);
    message.username[MAX_USERNAME_LEN - 1] = '\0';
  }
  message.username[MAX_USERNAME_LEN - 1] = '\0'; // Ensure null termination

  // Add client to the clients array
  int client_index = -1;
  pthread_mutex_lock(&server->clients_mutex);

  for (int i = 0; i < MAX_CLIENTS; i++)
  {
    if (server->clients[i].socket == 0) // Find empty slot
    {
      server->clients[i].socket = client_socket;
      server->clients[i].address = client_address;
      strncpy(server->clients[i].username, message.username, MAX_USERNAME_LEN - 1);
      server->clients[i].username[MAX_USERNAME_LEN - 1] = '\0';
      server->clients[i].active = true;
      server->clients[i].current_channel_id = 1; // Default to General channel
      client_index = i;
      break;
    }
  }

  pthread_mutex_unlock(&server->clients_mutex);

  if (client_index == -1)
  {
    log_message("Maximum clients reached, rejecting connection from %s", client_ip);
    // Send rejection message?
    close(client_socket);
    return NULL;
  }

  log_message("Client '%s' (socket %d) added at index %d",
              server->clients[client_index].username, client_socket, client_index);

  // Send channel list to the new client
  send_channel_list(server, client_socket);

  // Send welcome message
  Message welcome_msg;
  memset(&welcome_msg, 0, sizeof(Message));
  welcome_msg.type = MSG_SERVER;
  strncpy(welcome_msg.username, "Server", MAX_USERNAME_LEN - 1);
  welcome_msg.username[MAX_USERNAME_LEN - 1] = '\0';
  snprintf(welcome_msg.content, BUFFER_SIZE, "Welcome to the chat server, %s!", server->clients[client_index].username);
  welcome_msg.timestamp = time(NULL);
  send(client_socket, &welcome_msg, sizeof(Message), 0);

  // Broadcast join message to other clients
  Message join_broadcast_msg;
  memset(&join_broadcast_msg, 0, sizeof(Message));
  join_broadcast_msg.type = MSG_CONNECT;
  strncpy(join_broadcast_msg.username, server->clients[client_index].username, MAX_USERNAME_LEN - 1);
  join_broadcast_msg.username[MAX_USERNAME_LEN - 1] = '\0';
  join_broadcast_msg.timestamp = time(NULL);
  broadcast_message(server, &join_broadcast_msg, client_socket);

  log_message("Client connected notification broadcast for: %s", server->clients[client_index].username);

  // Main message loop
  while (1)
  {
    memset(&message, 0, sizeof(Message));
    ssize_t received = recv(client_socket, &message, sizeof(Message), 0);

    if (received <= 0)
    {
      // Client disconnected or error
      pthread_mutex_lock(&server->clients_mutex); // Lock before accessing shared data
      if (server->clients[client_index].active)
      { // Check if already marked inactive
        if (received == 0)
        {
          log_message("Client disconnected: %s (socket %d)", server->clients[client_index].username, client_socket);
        }
        else
        {
          log_message("Receive failed for client %s (socket %d): %s",
                      server->clients[client_index].username, client_socket, strerror(errno));
        }

        // Prepare disconnect message before clearing client data
        Message disconnect_msg;
        memset(&disconnect_msg, 0, sizeof(Message));
        disconnect_msg.type = MSG_DISCONNECT;
        strncpy(disconnect_msg.username, server->clients[client_index].username, MAX_USERNAME_LEN - 1);
        disconnect_msg.username[MAX_USERNAME_LEN - 1] = '\0';
        disconnect_msg.timestamp = time(NULL);

        // Remove client from the clients array (mark inactive)
        server->clients[client_index].active = false;
        close(server->clients[client_index].socket); // Close socket here
        server->clients[client_index].socket = 0;
        pthread_mutex_unlock(&server->clients_mutex);

        // Broadcast disconnect message (after unlocking mutex for client array)
        broadcast_message(server, &disconnect_msg, client_socket); // Pass client_socket so it doesn't send to itself
      }
      else
      {
        pthread_mutex_unlock(&server->clients_mutex);
      }
      break; // Exit loop
    }

    // Timestamp the message
    message.timestamp = time(NULL);

    // Handle different message types
    switch (message.type)
    {
    case MSG_TEXT:
      // Ensure username and content are null-terminated before logging/using
      message.username[MAX_USERNAME_LEN - 1] = '\0';
      message.content[BUFFER_SIZE - 1] = '\0';
      log_message("Message from %s (socket %d): %s", message.username, client_socket, message.content);

      // Update channel ID for the message based on sender's current channel
      pthread_mutex_lock(&server->clients_mutex);
      message.channel_id = server->clients[client_index].current_channel_id;
      pthread_mutex_unlock(&server->clients_mutex);

      // Broadcast only to the specific channel
      channel_broadcast(server, &message, client_socket, message.channel_id);
      break;

    case MSG_CHANNEL_JOIN:
    {
      int channel_id = atoi(message.content);

      // Validate channel ID
      bool valid_channel = false;
      pthread_mutex_lock(&server->channels_mutex);
      for (int i = 0; i < MAX_CHANNELS; i++)
      {
        if (server->channels[i].active && server->channels[i].channel_id == channel_id)
        {
          valid_channel = true;
          break;
        }
      }
      pthread_mutex_unlock(&server->channels_mutex);

      if (valid_channel)
      {
        // Prepare leave message for the old channel
        Message leave_msg;
        memset(&leave_msg, 0, sizeof(Message));
        leave_msg.type = MSG_SERVER;
        pthread_mutex_lock(&server->clients_mutex);
        leave_msg.channel_id = server->clients[client_index].current_channel_id;
        snprintf(leave_msg.username, MAX_USERNAME_LEN, "Server");
        snprintf(leave_msg.content, BUFFER_SIZE, "%s has left the channel.", server->clients[client_index].username);
        pthread_mutex_unlock(&server->clients_mutex);
        leave_msg.timestamp = time(NULL);
        channel_broadcast(server, &leave_msg, client_socket, leave_msg.channel_id);

        // Update client's current channel
        pthread_mutex_lock(&server->clients_mutex);
        server->clients[client_index].current_channel_id = channel_id;
        pthread_mutex_unlock(&server->clients_mutex);

        // Prepare join message for the new channel
        Message join_msg;
        memset(&join_msg, 0, sizeof(Message));
        join_msg.type = MSG_SERVER;
        join_msg.channel_id = channel_id;
        snprintf(join_msg.username, MAX_USERNAME_LEN, "Server");
        pthread_mutex_lock(&server->clients_mutex);
        snprintf(join_msg.content, BUFFER_SIZE, "%s has joined the channel.", server->clients[client_index].username);
        pthread_mutex_unlock(&server->clients_mutex);
        join_msg.timestamp = time(NULL);
        channel_broadcast(server, &join_msg, client_socket, channel_id);

        // Send confirmation to client
        Message confirm_msg;
        memset(&confirm_msg, 0, sizeof(Message));
        confirm_msg.type = MSG_CHANNEL_JOIN; // Use dedicated type for confirmation
        confirm_msg.channel_id = channel_id;
        snprintf(confirm_msg.username, MAX_USERNAME_LEN, "Server");
        // Find channel name to include in confirmation
        char joined_channel_name[MAX_CHANNEL_NAME_LEN] = "Unknown";
        pthread_mutex_lock(&server->channels_mutex);
        for (int i = 0; i < MAX_CHANNELS; ++i)
        {
          if (server->channels[i].active && server->channels[i].channel_id == channel_id)
          {
            strncpy(joined_channel_name, server->channels[i].name, MAX_CHANNEL_NAME_LEN - 1);
            break;
          }
        }
        pthread_mutex_unlock(&server->channels_mutex);
        snprintf(confirm_msg.content, BUFFER_SIZE, "%s", joined_channel_name); // Send name back
        confirm_msg.timestamp = time(NULL);
        send(client_socket, &confirm_msg, sizeof(Message), 0);
        log_message("Client %s joined channel %d (%s)", server->clients[client_index].username, channel_id, joined_channel_name);

        // Send recent messages for this channel if database is connected
        if (server->db && server->db->connected)
        {
          Message *recent_messages = NULL;
          MessageList *msg_list = (MessageList *)db_get_recent_messages(server->db, channel_id, 50, &recent_messages);

          if (msg_list && msg_list->count > 0 && recent_messages)
          {
            log_message("Sending %d recent messages for channel %d to client %s",
                        msg_list->count, channel_id, server->clients[client_index].username);
            for (int i = 0; i < msg_list->count; i++)
            {
              // Send each message to the client
              send(client_socket, &recent_messages[i], sizeof(Message), 0);
            }
          }
          if (msg_list)
            free(msg_list); // Assuming db_get_recent_messages allocates this
          // Assuming recent_messages buffer is handled by db_get_recent_messages or msg_list free
        }
      }
      else
      {
        log_message("Client %s failed to join invalid channel %d", server->clients[client_index].username, channel_id);
        // Invalid channel message
        Message error_msg;
        memset(&error_msg, 0, sizeof(Message));
        error_msg.type = MSG_SERVER;
        pthread_mutex_lock(&server->clients_mutex);
        error_msg.channel_id = server->clients[client_index].current_channel_id; // Stay in current channel
        pthread_mutex_unlock(&server->clients_mutex);
        snprintf(error_msg.username, MAX_USERNAME_LEN, "Server");
        snprintf(error_msg.content, BUFFER_SIZE, "Cannot join channel %d: channel does not exist", channel_id);
        error_msg.timestamp = time(NULL);
        send(client_socket, &error_msg, sizeof(Message), 0);
      }
    }
    break;

    case MSG_CHANNEL_CREATE:
    {
      // Format is "name|description|is_public"
      log_message("Channel creation request from %s: %s",
                  server->clients[client_index].username,
                  message.content);

      char name[MAX_CHANNEL_NAME_LEN] = {0};
      char description[BUFFER_SIZE] = {0};
      bool is_public = true;

      char *mutable_content = message.content; // Use a pointer for strtok
      char *token = strtok(mutable_content, "|");
      if (token)
      {
        strncpy(name, token, MAX_CHANNEL_NAME_LEN - 1);
        name[MAX_CHANNEL_NAME_LEN - 1] = '\0';

        token = strtok(NULL, "|");
        if (token)
        {
          strncpy(description, token, BUFFER_SIZE - 1);
          description[BUFFER_SIZE - 1] = '\0';

          token = strtok(NULL, "|");
          if (token)
          {
            is_public = atoi(token) != 0;
          }
        }
      }

      if (strlen(name) == 0)
      {
        log_message("Channel creation failed for %s: no name provided", server->clients[client_index].username);
        // Error: no channel name
        Message error_msg = {0};
        error_msg.type = MSG_SERVER;
        strncpy(error_msg.username, "Server", MAX_USERNAME_LEN - 1);
        error_msg.username[MAX_USERNAME_LEN - 1] = '\0';
        strncpy(error_msg.content, "Cannot create channel: no name provided", BUFFER_SIZE - 1);
        error_msg.content[BUFFER_SIZE - 1] = '\0';
        error_msg.timestamp = time(NULL);
        send(client_socket, &error_msg, sizeof(Message), 0);
      }
      else
      {
        int channel_id = create_channel(server, name, description, is_public);

        if (channel_id != -1)
        {
          // Notify all clients about the new channel
          Message notify_msg = {0};
          notify_msg.type = MSG_SERVER;
          strncpy(notify_msg.username, "Server", MAX_USERNAME_LEN - 1);
          notify_msg.username[MAX_USERNAME_LEN - 1] = '\0';
          snprintf(notify_msg.content, BUFFER_SIZE, "New channel created: %s", name);
          notify_msg.timestamp = time(NULL);
          broadcast_message(server, &notify_msg, -1); // Send to all including creator

          // Send confirmation to the requester
          Message confirm_msg = {0};
          confirm_msg.type = MSG_SERVER; // Keep as SERVER for now
          strncpy(confirm_msg.username, "Server", MAX_USERNAME_LEN - 1);
          confirm_msg.username[MAX_USERNAME_LEN - 1] = '\0';
          snprintf(confirm_msg.content, BUFFER_SIZE, "Channel '%s' created successfully", name);
          confirm_msg.timestamp = time(NULL);
          send(client_socket, &confirm_msg, sizeof(Message), 0);

          // Send updated channel list to all clients
          pthread_mutex_lock(&server->clients_mutex);
          for (int i = 0; i < MAX_CLIENTS; i++)
          {
            if (server->clients[i].socket != 0 && server->clients[i].active)
            {
              send_channel_list(server, server->clients[i].socket);
            }
          }
          pthread_mutex_unlock(&server->clients_mutex);

          log_message("New channel created: %s (ID: %d) by %s",
                      name, channel_id, server->clients[client_index].username);
        }
        else
        {
          log_message("Failed to create channel '%s' for %s (already exists or max reached)",
                      name, server->clients[client_index].username);
          // Error creating channel
          Message error_msg = {0};
          error_msg.type = MSG_SERVER;
          strncpy(error_msg.username, "Server", MAX_USERNAME_LEN - 1);
          error_msg.username[MAX_USERNAME_LEN - 1] = '\0';
          strncpy(error_msg.content, "Failed to create channel: channel may already exist or server full", BUFFER_SIZE - 1);
          error_msg.content[BUFFER_SIZE - 1] = '\0';
          error_msg.timestamp = time(NULL);
          send(client_socket, &error_msg, sizeof(Message), 0);
        }
      }
    }
    break;

    case MSG_CHANNEL_LIST:
      // Client requested channel list
      log_message("Channel list explicitly requested by %s (socket %d)",
                  server->clients[client_index].username, client_socket);
      send_channel_list(server, client_socket);
      break;

    default:
      log_message("Unknown message type %d received from %s (socket %d)",
                  message.type, server->clients[client_index].username, client_socket);
    }
  }

  return NULL;
}

// Clean up server resources
void cleanup_server(ServerContext *server)
{
  printf("Cleaning up server resources...\n");

  // Close all client sockets
  pthread_mutex_lock(&server->clients_mutex);
  for (int i = 0; i < MAX_CLIENTS; i++)
  {
    if (server->clients[i].socket != 0)
    {
      close(server->clients[i].socket);
      server->clients[i].socket = 0;
      server->clients[i].active = false;
    }
  }
  pthread_mutex_unlock(&server->clients_mutex);

  // Close server socket
  close(server->socket);

  // Destroy mutexes
  pthread_mutex_destroy(&server->clients_mutex);
  pthread_mutex_destroy(&server->channels_mutex);

  printf("Server cleanup completed\n");
}

// The server's main loop - accepting connections and creating threads for clients
void run_server(ServerContext *server)
{
  if (!server)
  {
    log_message("Cannot run NULL server context");
    return;
  }

  log_message("Server running on port %d", server->port);

  // Set up signal handling
  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);

  // Main server loop
  while (server->running)
  {
    // Accept incoming connection
    struct sockaddr_in client_address;
    socklen_t addrlen = sizeof(client_address);

    // Set up select with timeout to check for server->running flag periodically
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(server->socket, &readfds);

    // Set timeout to 1 second to check running flag
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    // Wait for activity on server socket
    int activity = select(server->socket + 1, &readfds, NULL, NULL, &timeout);

    if (activity < 0 && errno != EINTR)
    {
      log_message("Select error: %s", strerror(errno));
      continue;
    }

    // If timeout occurred, just check running flag
    if (activity == 0)
    {
      continue;
    }

    // If server socket is readable, accept connection
    if (FD_ISSET(server->socket, &readfds))
    {
      int client_socket = accept(server->socket, (struct sockaddr *)&client_address, &addrlen);
      if (client_socket < 0)
      {
        if (errno != EINTR) // Ignore interrupted syscall (e.g., due to signal)
        {
          log_message("Accept failed: %s", strerror(errno));
        }
        continue;
      }

      // Create handler args
      HandlerArgs *args = malloc(sizeof(HandlerArgs));
      if (!args)
      {
        log_message("Failed to allocate memory for handler args");
        close(client_socket);
        continue;
      }

      args->server = server;
      args->client_socket = client_socket;
      args->client_address = client_address;

      // Create thread for client
      pthread_t thread_id;
      if (pthread_create(&thread_id, NULL, handle_client, args) != 0)
      {
        log_message("Failed to create thread for client: %s", strerror(errno));
        free(args);
        close(client_socket);
        continue;
      }

      // Detach thread so resources are freed automatically when it exits
      pthread_detach(thread_id);
    }
  }

  log_message("Server shutting down");
}