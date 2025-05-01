#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>

#define BUFFER_SIZE 1024
#define MAX_CHANNELS 10

// Role definitions
#define ROLE_GUEST 0
#define ROLE_MEMBER 1
#define ROLE_MODERATOR 2
#define ROLE_ADMIN 3

// Client state information
typedef struct
{
  int socket;
  char username[64];
  char current_channel[32];
  int role; // User's role level
  bool is_authenticated;
} ClientState;

// Global client state
ClientState client_state = {0};

// Function to receive messages from server
void *receive_messages(void *socket_desc)
{
  int sock = *(int *)socket_desc;
  char buffer[BUFFER_SIZE];

  while (1)
  {
    int bytes_read = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_read <= 0)
    {
      printf("Server disconnected\n");
      exit(EXIT_FAILURE);
    }

    buffer[bytes_read] = '\0';

    // Check for special messages

    // Check for role updates
    if (strncmp(buffer, "ROLE_UPDATE:", 12) == 0)
    {
      int new_role = atoi(buffer + 12);
      client_state.role = new_role;
      printf("\n[SYSTEM] Your role has been updated to: %s\n",
             new_role == ROLE_ADMIN ? "Administrator" : new_role == ROLE_MODERATOR ? "Moderator"
                                                    : new_role == ROLE_MEMBER      ? "Member"
                                                                                   : "Guest");
      continue;
    }

    // Check if message contains LOGIN_SUCCESS and ROLE_UPDATE combined
    char *login_success = strstr(buffer, "LOGIN_SUCCESS");
    char *role_update = strstr(buffer, "ROLE_UPDATE:");

    if (login_success && role_update)
    {
      // Extract role information
      int new_role = atoi(role_update + 12);
      client_state.role = new_role;

      // Process login success
      client_state.is_authenticated = true;
      strcpy(client_state.current_channel, "general"); // Default channel

      printf("\n[SYSTEM] Successfully logged in. You're now in #general channel.\n");
      printf("\n[SYSTEM] Your role has been updated to: %s\n",
             new_role == ROLE_ADMIN ? "Administrator" : new_role == ROLE_MODERATOR ? "Moderator"
                                                    : new_role == ROLE_MEMBER      ? "Member"
                                                                                   : "Guest");
      continue;
    }

    // Check for login success
    if (strcmp(buffer, "LOGIN_SUCCESS") == 0)
    {
      client_state.is_authenticated = true;
      strcpy(client_state.current_channel, "general"); // Default channel
      printf("\n[SYSTEM] Successfully logged in. You're now in #general channel.\n");
    }
    else
    {
      printf("Server: %s\n", buffer);
    }
  }

  return NULL;
}

// Get role name string
const char *get_role_name(int role)
{
  switch (role)
  {
  case ROLE_ADMIN:
    return "Admin";
  case ROLE_MODERATOR:
    return "Mod";
  case ROLE_MEMBER:
    return "Member";
  case ROLE_GUEST:
  default:
    return "Guest";
  }
}

// Check if user has permission for a command
bool has_permission(const char *command)
{
  // Commands available to all authenticated users
  if (strcmp(command, "help") == 0 ||
      strcmp(command, "list") == 0 ||
      strcmp(command, "users") == 0 ||
      strcmp(command, "quit") == 0 ||
      strcmp(command, "channel") == 0 ||
      strcmp(command, "msg") == 0)
  {
    return client_state.is_authenticated;
  }

  // Member+ commands
  if (strcmp(command, "react") == 0)
  {
    return client_state.role >= ROLE_MEMBER;
  }

  // Moderator+ commands
  if (strcmp(command, "kick") == 0 ||
      strcmp(command, "mute") == 0)
  {
    return client_state.role >= ROLE_MODERATOR;
  }

  // Admin commands
  if (strcmp(command, "create") == 0 ||
      strcmp(command, "delete") == 0 ||
      strcmp(command, "setrole") == 0)
  {
    return client_state.role >= ROLE_ADMIN;
  }

  return false;
}

// Function to display help information
void display_help()
{
  printf("\n=== MyDiscord Command Help ===\n");
  printf("Available commands for your role (%s):\n\n", get_role_name(client_state.role));

  // Commands for all authenticated users
  printf("General Commands:\n");
  printf("/help           - Display this help message\n");
  printf("/channel <name> - Switch to specified channel\n");
  printf("/list           - List available channels\n");
  printf("/users          - List users in current channel\n");
  printf("/msg <user>     - Send private message to user\n");
  printf("/quit           - Exit the application\n");

  // Role-specific commands
  if (client_state.role >= ROLE_MEMBER)
  {
    printf("\nMember Commands:\n");
    printf("/react <id> <emoji> - React to message with ID\n");
  }

  if (client_state.role >= ROLE_MODERATOR)
  {
    printf("\nModerator Commands:\n");
    printf("/kick <user>      - Kick user from channel\n");
    printf("/mute <user> <duration> - Mute user for specified duration\n");
  }

  if (client_state.role >= ROLE_ADMIN)
  {
    printf("\nAdmin Commands:\n");
    printf("/create <type> <name> - Create new channel\n");
    printf("/delete <channel>     - Delete a channel\n");
    printf("/setrole <user> <role> - Set user role (0=Guest, 1=Member, 2=Mod, 3=Admin)\n");
  }

  printf("\nAny other text will be sent as a message\n");
  printf("==============================\n\n");
}

// Function to process commands
bool process_command(const char *input)
{
  if (input[0] != '/')
  {
    return false; // Not a command
  }

  // Skip the '/' character
  char command[BUFFER_SIZE];
  strcpy(command, input + 1);

  // Extract the command and its arguments
  char *cmd = strtok(command, " ");

  if (cmd == NULL)
  {
    return false;
  }

  // Check permissions for this command
  if (!has_permission(cmd))
  {
    printf("[SYSTEM] You don't have permission to use this command.\n");
    return true;
  }

  // Process different commands
  if (strcmp(cmd, "help") == 0)
  {
    display_help();
    return true;
  }
  else if (strcmp(cmd, "quit") == 0)
  {
    printf("[SYSTEM] Exiting application...\n");
    close(client_state.socket);
    exit(0);
    return true;
  }
  else if (strcmp(cmd, "channel") == 0)
  {
    char *channel_name = strtok(NULL, " ");
    if (channel_name != NULL)
    {
      printf("[SYSTEM] Attempting to switch to channel: #%s\n", channel_name);
      char channel_cmd[BUFFER_SIZE];
      sprintf(channel_cmd, "CHANNEL:%s", channel_name);
      send(client_state.socket, channel_cmd, strlen(channel_cmd), 0);
      strcpy(client_state.current_channel, channel_name);
      printf("[SYSTEM] Switched to #%s channel\n", channel_name);
    }
    else
    {
      printf("[SYSTEM] Error: Please specify a channel name\n");
    }
    return true;
  }
  else if (strcmp(cmd, "list") == 0)
  {
    printf("[SYSTEM] Requesting channel list...\n");
    send(client_state.socket, "LIST_CHANNELS", strlen("LIST_CHANNELS"), 0);
    return true;
  }
  else if (strcmp(cmd, "users") == 0)
  {
    printf("[SYSTEM] Requesting user list for #%s...\n", client_state.current_channel);
    char user_cmd[BUFFER_SIZE];
    sprintf(user_cmd, "LIST_USERS:%s", client_state.current_channel);
    send(client_state.socket, user_cmd, strlen(user_cmd), 0);
    return true;
  }
  else if (strcmp(cmd, "msg") == 0)
  {
    char *recipient = strtok(NULL, " ");
    char *message = strtok(NULL, "");

    if (recipient != NULL && message != NULL)
    {
      printf("[SYSTEM] Sending private message to %s\n", recipient);
      char pm_cmd[BUFFER_SIZE];
      sprintf(pm_cmd, "PM:%s:%s", recipient, message);
      send(client_state.socket, pm_cmd, strlen(pm_cmd), 0);
    }
    else
    {
      printf("[SYSTEM] Error: Usage: /msg <username> <message>\n");
    }
    return true;
  }
  // Role-based commands
  else if (strcmp(cmd, "create") == 0 && client_state.role >= ROLE_ADMIN)
  {
    char *channel_type = strtok(NULL, " ");
    char *channel_name = strtok(NULL, " ");

    if (channel_type != NULL && channel_name != NULL)
    {
      printf("[SYSTEM] Creating %s channel: #%s\n", channel_type, channel_name);
      char create_cmd[BUFFER_SIZE];
      sprintf(create_cmd, "CREATE_CHANNEL:%s:%s", channel_type, channel_name);
      send(client_state.socket, create_cmd, strlen(create_cmd), 0);
    }
    else
    {
      printf("[SYSTEM] Error: Usage: /create <type> <name>\n");
      printf("[SYSTEM] Types: public, private\n");
    }
    return true;
  }
  else if (strcmp(cmd, "delete") == 0 && client_state.role >= ROLE_ADMIN)
  {
    char *channel_name = strtok(NULL, " ");

    if (channel_name != NULL)
    {
      printf("[SYSTEM] Deleting channel: #%s\n", channel_name);
      char delete_cmd[BUFFER_SIZE];
      sprintf(delete_cmd, "DELETE_CHANNEL:%s", channel_name);
      send(client_state.socket, delete_cmd, strlen(delete_cmd), 0);
    }
    else
    {
      printf("[SYSTEM] Error: Usage: /delete <channel>\n");
    }
    return true;
  }
  else if (strcmp(cmd, "setrole") == 0 && client_state.role >= ROLE_ADMIN)
  {
    char *username = strtok(NULL, " ");
    char *role_str = strtok(NULL, " ");

    if (username != NULL && role_str != NULL)
    {
      int role = atoi(role_str);
      if (role >= 0 && role <= 3)
      {
        printf("[SYSTEM] Setting %s's role to %s\n", username, get_role_name(role));
        char role_cmd[BUFFER_SIZE];
        sprintf(role_cmd, "SET_ROLE:%s:%d", username, role);
        send(client_state.socket, role_cmd, strlen(role_cmd), 0);
      }
      else
      {
        printf("[SYSTEM] Error: Invalid role (0=Guest, 1=Member, 2=Mod, 3=Admin)\n");
      }
    }
    else
    {
      printf("[SYSTEM] Error: Usage: /setrole <username> <role>\n");
      printf("[SYSTEM] Roles: 0=Guest, 1=Member, 2=Mod, 3=Admin\n");
    }
    return true;
  }
  else if (strcmp(cmd, "kick") == 0 && client_state.role >= ROLE_MODERATOR)
  {
    char *username = strtok(NULL, " ");

    if (username != NULL)
    {
      printf("[SYSTEM] Kicking user: %s\n", username);
      char kick_cmd[BUFFER_SIZE];
      sprintf(kick_cmd, "KICK:%s:%s", client_state.current_channel, username);
      send(client_state.socket, kick_cmd, strlen(kick_cmd), 0);
    }
    else
    {
      printf("[SYSTEM] Error: Usage: /kick <username>\n");
    }
    return true;
  }
  else if (strcmp(cmd, "mute") == 0 && client_state.role >= ROLE_MODERATOR)
  {
    char *username = strtok(NULL, " ");
    char *duration = strtok(NULL, " ");

    if (username != NULL && duration != NULL)
    {
      printf("[SYSTEM] Muting %s for %s minutes\n", username, duration);
      char mute_cmd[BUFFER_SIZE];
      sprintf(mute_cmd, "MUTE:%s:%s:%s", client_state.current_channel, username, duration);
      send(client_state.socket, mute_cmd, strlen(mute_cmd), 0);
    }
    else
    {
      printf("[SYSTEM] Error: Usage: /mute <username> <duration_minutes>\n");
    }
    return true;
  }
  else if (strcmp(cmd, "react") == 0 && client_state.role >= ROLE_MEMBER)
  {
    char *msg_id = strtok(NULL, " ");
    char *emoji = strtok(NULL, " ");

    if (msg_id != NULL && emoji != NULL)
    {
      printf("[SYSTEM] Reacting to message %s with %s\n", msg_id, emoji);
      char react_cmd[BUFFER_SIZE];
      sprintf(react_cmd, "REACT:%s:%s", msg_id, emoji);
      send(client_state.socket, react_cmd, strlen(react_cmd), 0);
    }
    else
    {
      printf("[SYSTEM] Error: Usage: /react <message_id> <emoji>\n");
    }
    return true;
  }

  printf("[SYSTEM] Unknown command. Type /help for available commands.\n");
  return true;
}

// Function to handle Ctrl+C gracefully
void sigint_handler(int sig)
{
  printf("\n[SYSTEM] Caught SIGINT signal. Exiting...\n");
  exit(0);
}

int main(int argc, char *argv[])
{
  // Default settings
  int port = 8080;

  // Parse command line arguments
  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "--port") == 0 && i + 1 < argc)
    {
      port = atoi(argv[i + 1]);
      i++; // Skip the port value
    }
  }

  int sock = 0;
  struct sockaddr_in serv_addr;
  char buffer[BUFFER_SIZE] = {0};

  // Initialize signals to handle Ctrl+C
  signal(SIGINT, sigint_handler);

  // Create socket
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    perror("Socket creation error");
    return EXIT_FAILURE;
  }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);

  // Convert IPv4 address from text to binary form
  if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
  {
    perror("Invalid address or address not supported");
    return EXIT_FAILURE;
  }

  printf("[LOG] Attempting to connect to server...\n");

  // Connect to server
  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
  {
    perror("Connection failed");
    return EXIT_FAILURE;
  }

  printf("[LOG] Connected to server successfully!\n");

  // Store socket in client state
  client_state.socket = sock;
  client_state.is_authenticated = false;
  client_state.role = ROLE_GUEST; // Start as guest

  // Start thread to receive messages
  pthread_t thread_id;
  if (pthread_create(&thread_id, NULL, receive_messages, &sock) < 0)
  {
    perror("Thread creation failed");
    return EXIT_FAILURE;
  }

  // Send login message
  char login_msg[256];
  printf("Enter username: ");
  fgets(client_state.username, sizeof(client_state.username), stdin);
  client_state.username[strcspn(client_state.username, "\n")] = 0; // Remove newline

  printf("Enter password: ");
  char password[64];
  fgets(password, sizeof(password), stdin);
  password[strcspn(password, "\n")] = 0; // Remove newline

  sprintf(login_msg, "LOGIN:%s:%s", client_state.username, password);
  send(sock, login_msg, strlen(login_msg), 0);

  // Display initial help
  printf("\n[LOG] Waiting for server authentication...\n");
  sleep(1); // Wait for login response

  if (client_state.is_authenticated)
  {
    display_help();
  }

  // Main loop for sending messages
  char message[BUFFER_SIZE];
  printf("\nChat started. Type messages or commands (start with /)...\n");

  while (1)
  {
    printf("[%s(%s):#%s]> ", client_state.username, get_role_name(client_state.role), client_state.current_channel);
    fgets(message, BUFFER_SIZE, stdin);
    message[strcspn(message, "\n")] = 0; // Remove newline

    // Check if this is a command
    if (message[0] == '/')
    {
      if (strcmp(message, "/quit") == 0)
      {
        break;
      }

      if (process_command(message))
      {
        continue;
      }
    }

    // If not processed as a command, send as a regular message
    char formatted_msg[BUFFER_SIZE + 64];
    sprintf(formatted_msg, "MSG:%s:%s", client_state.current_channel, message);

    if (send(sock, formatted_msg, strlen(formatted_msg), 0) < 0)
    {
      perror("Failed to send message");
      break;
    }
  }

  close(sock);
  return 0;
}