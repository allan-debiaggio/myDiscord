#include "server.h"
#include <stdarg.h>

// Global server context
ServerContext server_ctx;

// Helper function to set socket to non-blocking mode
void set_nonblocking(int socket_fd)
{
  int flags = fcntl(socket_fd, F_GETFL, 0);
  fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
}

// Logging functions
void log_info(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  printf("[INFO] ");
  vprintf(format, args);
  printf("\n");
  va_end(args);
}

void log_error(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  fprintf(stderr, "[ERROR] ");
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
  va_end(args);
}

// Initialize the server data
void initialize_server_data()
{
  pthread_mutex_lock(&server_ctx.channels_mutex);

  // Add default channels
  strncpy(server_ctx.channels[0].name, "general", MAX_CHANNEL_NAME_LEN - 1);
  strncpy(server_ctx.channels[1].name, "random", MAX_CHANNEL_NAME_LEN - 1);
  strncpy(server_ctx.channels[2].name, "help", MAX_CHANNEL_NAME_LEN - 1);
  server_ctx.channel_count = 3;

  pthread_mutex_unlock(&server_ctx.channels_mutex);

  pthread_mutex_lock(&server_ctx.users_mutex);

  // Add admin user
  strncpy(server_ctx.users[0].username, "admin", MAX_USERNAME_LEN - 1);
  strncpy(server_ctx.users[0].email, "admin@example.com", MAX_EMAIL_LEN - 1);
  strncpy(server_ctx.users[0].password, "admin123", MAX_PASSWORD_LEN - 1);
  server_ctx.users[0].role = ROLE_ADMIN;
  server_ctx.users[0].is_online = 0;
  server_ctx.user_count = 1;

  pthread_mutex_unlock(&server_ctx.users_mutex);

  log_info("Server data initialized");
}

// Set up signal handlers
void setup_signal_handlers()
{
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));

  sa.sa_handler = handle_termination;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  // Ignore broken pipe signals
  signal(SIGPIPE, SIG_IGN);

  log_info("Signal handlers set up");
}

// Handle termination signals
void handle_termination(int sig)
{
  log_info("Received signal %d, shutting down gracefully", sig);
  server_ctx.running = 0;

  // Close the server socket to unblock accept() call
  if (server_ctx.server_socket != -1)
  {
    close(server_ctx.server_socket);
    server_ctx.server_socket = -1;
  }
}

// Start the server
int start_server()
{
  struct sockaddr_in server_addr;
  int opt = 1;

  // Initialize server context
  memset(&server_ctx, 0, sizeof(ServerContext));
  pthread_mutex_init(&server_ctx.clients_mutex, NULL);
  pthread_mutex_init(&server_ctx.channels_mutex, NULL);
  pthread_mutex_init(&server_ctx.users_mutex, NULL);

  // Initialize clients array
  for (int i = 0; i < MAX_CONNECTIONS; i++)
  {
    server_ctx.clients[i].socket = -1;
    server_ctx.clients[i].user_id = -1;
    server_ctx.clients[i].is_authenticated = 0;
  }

  // Create socket
  if ((server_ctx.server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
  {
    log_error("Socket creation failed: %s", strerror(errno));
    return 0;
  }

  // Set socket options to reuse address
  if (setsockopt(server_ctx.server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
  {
    log_error("Setsockopt SO_REUSEADDR failed: %s", strerror(errno));
    return 0;
  }

  // Optional: Set non-blocking
  // set_nonblocking(server_ctx.server_socket);

  // Prepare server address structure
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(SERVER_PORT);

  // Bind socket to address and port
  if (bind(server_ctx.server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    log_error("Bind failed: %s", strerror(errno));
    close(server_ctx.server_socket);
    return 0;
  }

  // Listen for connections
  if (listen(server_ctx.server_socket, MAX_CONNECTIONS) < 0)
  {
    log_error("Listen failed: %s", strerror(errno));
    close(server_ctx.server_socket);
    return 0;
  }

  // Initialize server data
  initialize_server_data();

  // Set up signal handlers
  setup_signal_handlers();

  // Mark server as running
  server_ctx.running = 1;

  log_info("Server started on port %d", SERVER_PORT);

  // Accept connections
  accept_connections();

  return 1;
}

// Stop the server
void stop_server()
{
  log_info("Stopping server...");

  // Close the server socket
  if (server_ctx.server_socket != -1)
  {
    close(server_ctx.server_socket);
    server_ctx.server_socket = -1;
  }

  // Close all client connections
  pthread_mutex_lock(&server_ctx.clients_mutex);
  for (int i = 0; i < MAX_CONNECTIONS; i++)
  {
    if (server_ctx.clients[i].socket != -1)
    {
      close(server_ctx.clients[i].socket);
      server_ctx.clients[i].socket = -1;
    }
  }
  pthread_mutex_unlock(&server_ctx.clients_mutex);

  // Destroy mutexes
  pthread_mutex_destroy(&server_ctx.clients_mutex);
  pthread_mutex_destroy(&server_ctx.channels_mutex);
  pthread_mutex_destroy(&server_ctx.users_mutex);

  log_info("Server stopped");
}

// Accept client connections
void accept_connections()
{
  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);
  int client_socket;

  log_info("Waiting for connections...");

  while (server_ctx.running)
  {
    // Accept a new connection
    client_socket = accept(server_ctx.server_socket, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);

    if (client_socket < 0)
    {
      if (errno == EINTR)
      {
        // Interrupted by signal, check if we should continue running
        continue;
      }

      log_error("Accept failed: %s", strerror(errno));
      continue;
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    log_info("New connection from %s:%d", client_ip, ntohs(client_addr.sin_port));

    // Handle the new connection
    handle_new_connection(client_socket);
  }
}

// Handle a new client connection
void handle_new_connection(int client_socket)
{
  pthread_mutex_lock(&server_ctx.clients_mutex);

  // Find a free slot
  int slot = -1;
  for (int i = 0; i < MAX_CONNECTIONS; i++)
  {
    if (server_ctx.clients[i].socket == -1)
    {
      slot = i;
      break;
    }
  }

  if (slot == -1)
  {
    // No free slots
    pthread_mutex_unlock(&server_ctx.clients_mutex);
    log_error("No free slots for new client");
    close(client_socket);
    return;
  }

  // Initialize client connection
  server_ctx.clients[slot].socket = client_socket;
  server_ctx.clients[slot].user_id = -1;
  server_ctx.clients[slot].is_authenticated = 0;
  memset(server_ctx.clients[slot].username, 0, MAX_USERNAME_LEN);

  pthread_mutex_unlock(&server_ctx.clients_mutex);

  // Create a thread for this client
  pthread_t thread;
  ClientThreadArg *arg = malloc(sizeof(ClientThreadArg));
  arg->client_index = slot;

  if (pthread_create(&thread, NULL, client_thread, arg) != 0)
  {
    log_error("Thread creation failed: %s", strerror(errno));

    pthread_mutex_lock(&server_ctx.clients_mutex);
    server_ctx.clients[slot].socket = -1;
    pthread_mutex_unlock(&server_ctx.clients_mutex);

    close(client_socket);
    free(arg);
    return;
  }

  // Detach thread
  pthread_detach(thread);
}

// Client handler thread
void *client_thread(void *arg)
{
  ClientThreadArg *thread_arg = (ClientThreadArg *)arg;
  int client_idx = thread_arg->client_index;
  free(thread_arg);

  ClientConnection *client = &server_ctx.clients[client_idx];
  NetworkMessage msg;

  log_info("Client thread started for socket %d", client->socket);

  while (server_ctx.running)
  {
    // Receive message from client
    ssize_t recv_result = recv(client->socket, &msg, sizeof(NetworkMessage), 0);

    if (recv_result <= 0)
    {
      if (recv_result == 0)
      {
        log_info("Client disconnected (socket: %d)", client->socket);
      }
      else
      {
        log_error("Receive error: %s", strerror(errno));
      }
      break;
    }

    // Process message based on type
    switch (msg.type)
    {
    case MSG_AUTH:
      handle_auth_message(client, &msg);
      break;

    case MSG_REGISTER:
      handle_register_message(client, &msg);
      break;

    case MSG_CHANNEL_LIST:
      handle_channel_list_request(client);
      break;

    case MSG_USER_LIST:
      handle_user_list_request(client);
      break;

    case MSG_CHANNEL_MSG:
      if (client->is_authenticated)
      {
        handle_channel_message(client, &msg);
      }
      break;

    case MSG_PRIVATE_MSG:
      if (client->is_authenticated)
      {
        handle_private_message(client, &msg);
      }
      break;

    case MSG_CREATE_CHANNEL:
      if (client->is_authenticated)
      {
        handle_create_channel(client, &msg);
      }
      break;

    case MSG_DELETE_CHANNEL:
      if (client->is_authenticated)
      {
        handle_delete_channel(client, &msg);
      }
      break;

    case MSG_MUTE_USER:
      if (client->is_authenticated)
      {
        handle_mute_user(client, &msg);
      }
      break;

    case MSG_SET_ROLE:
      if (client->is_authenticated)
      {
        handle_set_role(client, &msg);
      }
      break;

    default:
      log_error("Unknown message type %d from client %d", msg.type, client->socket);
      break;
    }
  }

  // Client disconnected, clean up
  disconnect_client(client);

  return NULL;
}

// Disconnect a client
void disconnect_client(ClientConnection *client)
{
  log_info("Disconnecting client (socket: %d, user: %s)", client->socket, client->username);

  // If client was authenticated, mark user as offline
  if (client->is_authenticated && client->user_id >= 0)
  {
    pthread_mutex_lock(&server_ctx.users_mutex);

    if (client->user_id < server_ctx.user_count)
    {
      server_ctx.users[client->user_id].is_online = 0;
      log_info("Marked user %s as offline", server_ctx.users[client->user_id].username);

      // Prepare user status notification
      NetworkMessage status_msg;
      status_msg.type = MSG_USER_STATUS;
      status_msg.user_id = client->user_id;
      status_msg.param = 0; // Offline
      strncpy(status_msg.username, client->username, MAX_USERNAME_LEN - 1);

      pthread_mutex_unlock(&server_ctx.users_mutex);

      // Broadcast status change
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
      pthread_mutex_unlock(&server_ctx.users_mutex);
    }
  }

  // Close socket and reset client data
  if (client->socket != -1)
  {
    close(client->socket);
    client->socket = -1;
    client->user_id = -1;
    client->is_authenticated = 0;
    memset(client->username, 0, MAX_USERNAME_LEN);
  }
}

// Send error message to client
void send_error_message(ClientConnection *client, const char *message)
{
  NetworkMessage response;
  response.type = MSG_ERROR;
  response.user_id = client->user_id;
  strncpy(response.content, message, MAX_MESSAGE_LEN - 1);

  if (send(client->socket, &response, sizeof(NetworkMessage), 0) <= 0)
  {
    log_error("Failed to send error message: %s", strerror(errno));
  }
}