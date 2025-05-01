#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>

#define BUFFER_SIZE 4096
#define MAX_COMMAND_SIZE 1024

// Global variables
int server_socket = -1;
bool is_running = true;
bool is_logged_in = false;
char current_user[64] = {0};
char current_channel[32] = "general";
int user_role = 0;
pthread_t listener_thread;

// Forward declarations
void cleanup_resources();
void *message_listener(void *arg);

// Handle signals for graceful shutdown
void handle_signal(int sig)
{
  printf("\nExiting client...\n");
  is_running = false;
  cleanup_resources();
  exit(0);
}

// Function to send messages to the server
void send_to_server(const char *message)
{
  if (server_socket < 0)
  {
    printf("Error: Not connected to server\n");
    return;
  }

  if (send(server_socket, message, strlen(message), 0) < 0)
  {
    perror("Send failed");
  }
}

// Function to receive messages from the server
char *receive_from_server()
{
  static char buffer[BUFFER_SIZE];
  memset(buffer, 0, BUFFER_SIZE);

  int recv_size = recv(server_socket, buffer, BUFFER_SIZE - 1, 0);
  if (recv_size > 0)
  {
    buffer[recv_size] = '\0';
    return buffer;
  }
  else if (recv_size == 0)
  {
    printf("Connection closed by server\n");
    is_running = false;
    return NULL;
  }
  else
  {
    perror("Receive failed");
    return NULL;
  }
}

// Function to connect to the server
bool connect_to_server(const char *server_ip, int server_port)
{
  // Create socket
  server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket < 0)
  {
    perror("Failed to create socket");
    return false;
  }

  // Prepare the sockaddr_in structure
  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(server_ip);
  server.sin_port = htons(server_port);

  // Connect to server
  printf("Connecting to %s:%d...\n", server_ip, server_port);
  if (connect(server_socket, (struct sockaddr *)&server, sizeof(server)) < 0)
  {
    perror("Connection failed");
    close(server_socket);
    server_socket = -1;
    return false;
  }

  printf("Connected to server!\n");

  // Receive welcome message
  char *welcome = receive_from_server();
  if (welcome)
  {
    printf("Server: %s", welcome);
  }

  return true;
}

// Function to handle login
bool login()
{
  char username[64] = {0};
  char password[64] = {0};
  char command[BUFFER_SIZE] = {0};

  printf("Username: ");
  fgets(username, sizeof(username), stdin);
  username[strcspn(username, "\n")] = 0; // Remove newline

  printf("Password: ");
  fgets(password, sizeof(password), stdin);
  password[strcspn(password, "\n")] = 0; // Remove newline

  // Format login command
  snprintf(command, BUFFER_SIZE, "LOGIN:%s:%s", username, password);

  // Send login request
  send_to_server(command);

  // Receive response
  char *response = receive_from_server();
  if (response && strstr(response, "LOGIN_SUCCESS") != NULL)
  {
    printf("Login successful!\n");
    strncpy(current_user, username, sizeof(current_user) - 1);

    // Extract role from response
    char *role_str = strstr(response, "ROLE_UPDATE:");
    if (role_str)
    {
      user_role = atoi(role_str + 12); // Skip "ROLE_UPDATE:"
      printf("Your role: %d (%s)\n", user_role,
             user_role == 3 ? "Admin" : user_role == 2 ? "Moderator"
                                    : user_role == 1   ? "Member"
                                                       : "Guest");
    }

    is_logged_in = true;
    return true;
  }
  else
  {
    printf("Login failed!\n");
    return false;
  }
}

// Function to send a message to a channel
void send_message(const char *channel, const char *content)
{
  char command[BUFFER_SIZE] = {0};

  // Format message command
  snprintf(command, BUFFER_SIZE, "MSG:%s:%s", channel, content);

  // Send message
  send_to_server(command);
}

// Function to send a private message to a user
void send_private_message(const char *username, const char *content)
{
  char command[BUFFER_SIZE] = {0};

  // Format private message command
  snprintf(command, BUFFER_SIZE, "PRIVMSG:%s:%s", username, content);

  // Send message
  send_to_server(command);
}

// Function to create a new channel (admin only)
void create_channel(const char *name, const char *type)
{
  if (user_role < 3)
  {
    printf("Error: Only admins can create channels\n");
    return;
  }

  char command[BUFFER_SIZE] = {0};

  // Format create channel command
  snprintf(command, BUFFER_SIZE, "CREATE_CHANNEL:%s:%s", name, type);

  // Send command
  send_to_server(command);
}

// Function to delete a channel (admin only)
void delete_channel(const char *name)
{
  if (user_role < 3)
  {
    printf("Error: Only admins can delete channels\n");
    return;
  }

  char command[BUFFER_SIZE] = {0};

  // Format delete channel command
  snprintf(command, BUFFER_SIZE, "DELETE_CHANNEL:%s", name);

  // Send command
  send_to_server(command);
}

// Function to set a user's role (admin only)
void set_user_role(const char *username, int role)
{
  if (user_role < 3)
  {
    printf("Error: Only admins can set user roles\n");
    return;
  }

  if (role < 0 || role > 3)
  {
    printf("Error: Invalid role (must be 0-3)\n");
    return;
  }

  char command[BUFFER_SIZE] = {0};

  // Format set role command
  snprintf(command, BUFFER_SIZE, "SET_ROLE:%s:%d", username, role);

  // Send command
  send_to_server(command);
}

// Function to list all available channels
void list_channels()
{
  char command[BUFFER_SIZE] = "LIST_CHANNELS";

  // Send command
  send_to_server(command);
}

// Function to mute a user
void mute_user(const char *username, const char *channel, int duration)
{
  if (user_role < 2)
  {
    printf("Error: Only moderators and admins can mute users\n");
    return;
  }

  char command[BUFFER_SIZE] = {0};

  // Format mute command
  snprintf(command, BUFFER_SIZE, "MUTE:%s:%s:%d", username, channel, duration);

  // Send mute command
  send_to_server(command);
}

// Function to unmute a user
void unmute_user(const char *username, const char *channel)
{
  if (user_role < 2)
  {
    printf("Error: Only moderators and admins can unmute users\n");
    return;
  }

  char command[BUFFER_SIZE] = {0};

  // Format unmute command
  snprintf(command, BUFFER_SIZE, "UNMUTE:%s:%s", username, channel);

  // Send unmute command
  send_to_server(command);
}

// Function to list muted users in a channel
void list_muted_users(const char *channel)
{
  char command[BUFFER_SIZE] = {0};

  // Format list muted users command
  snprintf(command, BUFFER_SIZE, "LIST_MUTED:%s", channel);

  // Send list muted users command
  send_to_server(command);
}

// Clean up resources before exiting
void cleanup_resources()
{
  if (server_socket >= 0)
  {
    close(server_socket);
    server_socket = -1;
  }
}

// Thread to listen for incoming messages from the server
void *message_listener(void *arg)
{
  while (is_running)
  {
    char *message = receive_from_server();
    if (message)
    {
      // Handle different types of messages
      if (strncmp(message, "[Private from ", 14) == 0)
      {
        // It's a private message
        printf("\n%s\n", message);
      }
      else if (message[0] == '[' && strchr(message, '@') != NULL && strchr(message, ']') != NULL)
      {
        // It's a broadcast message from a channel
        printf("\n%s\n", message);
      }
      else if (strncmp(message, "Successfully stored message", 26) == 0)
      {
        // It's a confirmation of message storage, print briefly
        printf("\nServer: %s\n", message);
      }
      else
      {
        // Other server messages
        printf("\nServer: %s\n", message);
      }

      printf("Command> "); // Prompt again after showing message
      fflush(stdout);
    }
    else
    {
      // Exit if we can't receive messages anymore
      is_running = false;
      break;
    }

    // Small delay to prevent CPU hogging
    usleep(100000); // 100ms
  }
  return NULL;
}

// Display help information
void display_help()
{
  printf("\nAvailable commands:\n");
  printf("  /help                 - Display this help information\n");
  printf("  /quit                 - Exit the client\n");
  printf("  /list                 - List all available channels\n");
  printf("  /msg <channel> <text> - Send a message to a channel\n");
  printf("  /pm <username> <text> - Send a private message to a user\n");
  printf("  /channel <name>       - Switch to a different channel\n");

  // Moderator/Admin commands
  if (user_role >= 2)
  {
    printf("\nModerator/Admin commands:\n");
    printf("  /mute <user> <duration> - Mute a user for specified minutes\n");
    printf("  /unmute <user>        - Unmute a user\n");
    printf("  /muted                - List all muted users in current channel\n");
  }

  // Admin-only commands
  if (user_role >= 3)
  {
    printf("\nAdmin-only commands:\n");
    printf("  /create <name> <type> - Create a new channel (type: public or private)\n");
    printf("  /delete <name>       - Delete a channel\n");
    printf("  /setrole <user> <role> - Set a user's role (0=guest, 1=member, 2=mod, 3=admin)\n");
  }

  printf("\nJust type text to send a message to the current channel\n");
}

// Main function
int main(int argc, char *argv[])
{
  // Set up signal handlers
  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);

  // Default settings
  const char *server_ip = "127.0.0.1";
  int server_port = 8082;

  // Parse command line arguments
  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "--port") == 0 && i + 1 < argc)
    {
      server_port = atoi(argv[i + 1]);
      i++;
    }
    else if (strcmp(argv[i], "--ip") == 0 && i + 1 < argc)
    {
      server_ip = argv[i + 1];
      i++;
    }
  }

  // Connect to server
  if (!connect_to_server(server_ip, server_port))
  {
    return 1;
  }

  // Login
  if (!login())
  {
    cleanup_resources();
    return 1;
  }

  // Create a thread to listen for incoming messages
  if (pthread_create(&listener_thread, NULL, message_listener, NULL) != 0)
  {
    perror("Failed to create listener thread");
    cleanup_resources();
    return 1;
  }

  // Detach the thread so we don't need to join it later
  pthread_detach(listener_thread);

  // Display help information
  display_help();

  // Main command loop
  char command[MAX_COMMAND_SIZE];

  printf("\nWelcome to MyDiscord client, %s!\n", current_user);
  printf("You are in channel: %s\n", current_channel);

  while (is_running)
  {
    printf("Command> ");
    if (fgets(command, MAX_COMMAND_SIZE, stdin) == NULL)
    {
      break;
    }

    // Remove newline
    command[strcspn(command, "\n")] = 0;

    // Skip empty commands
    if (strlen(command) == 0)
    {
      continue;
    }

    // Process commands
    if (command[0] == '/')
    {
      // Special commands
      if (strcmp(command, "/help") == 0)
      {
        display_help();
      }
      else if (strcmp(command, "/quit") == 0)
      {
        break;
      }
      else if (strcmp(command, "/list") == 0)
      {
        list_channels();
      }
      else if (strncmp(command, "/msg ", 5) == 0)
      {
        char channel[32] = {0};
        char content[MAX_COMMAND_SIZE] = {0};

        // Parse "/msg channel content"
        if (sscanf(command + 5, "%31s %[^\n]", channel, content) == 2)
        {
          send_message(channel, content);
        }
        else
        {
          printf("Usage: /msg <channel> <message>\n");
        }
      }
      else if (strncmp(command, "/pm ", 4) == 0)
      {
        char username[64] = {0};
        char content[MAX_COMMAND_SIZE] = {0};

        // Parse "/pm username content"
        if (sscanf(command + 4, "%63s %[^\n]", username, content) == 2)
        {
          send_private_message(username, content);
        }
        else
        {
          printf("Usage: /pm <username> <message>\n");
        }
      }
      else if (strncmp(command, "/channel ", 9) == 0)
      {
        char new_channel[32] = {0};

        // Parse "/channel name"
        if (sscanf(command + 9, "%31s", new_channel) == 1)
        {
          // Update local channel
          strncpy(current_channel, new_channel, sizeof(current_channel) - 1);

          // Send channel switch command to server
          char channel_cmd[BUFFER_SIZE];
          snprintf(channel_cmd, BUFFER_SIZE, "CHANNEL:%s\n", new_channel);
          send_to_server(channel_cmd);

          printf("Switching to channel: %s\n", current_channel);
        }
        else
        {
          printf("Usage: /channel <n>\n");
        }
      }
      else if (strncmp(command, "/mute ", 6) == 0)
      {
        if (user_role < 2)
        {
          printf("Error: Only moderators and admins can mute users\n");
        }
        else
        {
          char username[64] = {0};
          int duration = 10; // Default 10 minutes

          // Parse "/mute username [duration]"
          int parsed = sscanf(command + 6, "%63s %d", username, &duration);
          if (parsed >= 1)
          {
            mute_user(username, current_channel, duration);
          }
          else
          {
            printf("Usage: /mute <username> [duration_in_minutes]\n");
          }
        }
      }
      else if (strncmp(command, "/unmute ", 8) == 0)
      {
        if (user_role < 2)
        {
          printf("Error: Only moderators and admins can unmute users\n");
        }
        else
        {
          char username[64] = {0};

          // Parse "/unmute username"
          if (sscanf(command + 8, "%63s", username) == 1)
          {
            unmute_user(username, current_channel);
          }
          else
          {
            printf("Usage: /unmute <username>\n");
          }
        }
      }
      else if (strcmp(command, "/muted") == 0)
      {
        list_muted_users(current_channel);
      }
      else if (strncmp(command, "/create ", 8) == 0)
      {
        if (user_role < 3)
        {
          printf("Error: Only admins can create channels\n");
        }
        else
        {
          char name[32] = {0};
          char type[10] = "public"; // Default type

          // Parse "/create name [type]"
          int parsed = sscanf(command + 8, "%31s %9s", name, type);
          if (parsed >= 1)
          {
            create_channel(name, type);
          }
          else
          {
            printf("Usage: /create <channel_name> [public|private]\n");
          }
        }
      }
      else if (strncmp(command, "/delete ", 8) == 0)
      {
        if (user_role < 3)
        {
          printf("Error: Only admins can delete channels\n");
        }
        else
        {
          char name[32] = {0};

          // Parse "/delete name"
          if (sscanf(command + 8, "%31s", name) == 1)
          {
            delete_channel(name);
          }
          else
          {
            printf("Usage: /delete <channel_name>\n");
          }
        }
      }
      else if (strncmp(command, "/setrole ", 9) == 0)
      {
        if (user_role < 3)
        {
          printf("Error: Only admins can set user roles\n");
        }
        else
        {
          char username[64] = {0};
          int role = 1; // Default role (member)

          // Parse "/setrole username role"
          int parsed = sscanf(command + 9, "%63s %d", username, &role);
          if (parsed == 2)
          {
            set_user_role(username, role);
          }
          else
          {
            printf("Usage: /setrole <username> <role:0-3>\n");
          }
        }
      }
      else
      {
        printf("Unknown command. Type /help for available commands.\n");
      }
    }
    else
    {
      // Regular message to the current channel
      send_message(current_channel, command);

      // No need to print confirmation as the server will
      // broadcast it back if successful, shown through the listener thread
    }
  }

  printf("Exiting...\n");
  cleanup_resources();
  return 0;
}