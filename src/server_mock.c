#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 2048
#define MAX_CHANNELS 10
#define MAX_USERS 100
#define MAX_MESSAGES 200
#define MAX_REACTIONS 50
#define MAX_MUTED_USERS 20

// Role definitions
#define ROLE_GUEST 0
#define ROLE_MEMBER 1
#define ROLE_MODERATOR 2
#define ROLE_ADMIN 3

// Message structure
typedef struct
{
  int id;
  int user_id;
  int channel_id;
  char content[BUFFER_SIZE];
  time_t timestamp;
  bool is_deleted;
} Message;

// Reaction structure
typedef struct
{
  int message_id;
  int user_id;
  char emoji[8];
} Reaction;

// Muted user structure
typedef struct
{
  int user_id;
  int channel_id;
  time_t end_time;
} MutedUser;

// User structure to store basic user information
typedef struct
{
  int socket;
  char username[64];
  bool is_online;
  char current_channel[32];
  int role; // 0: guest, 1: member, 2: moderator, 3: admin
} User;

// Channel structure
typedef struct
{
  char name[32];
  char type[10]; // "public" or "private"
  int creator_id;
} Channel;

// Clients list for broadcasting
typedef struct client_node
{
  int socket;
  char username[64];
  char current_channel[32];
  struct client_node *next;
} client_node;

// Global state
client_node *clients_head = NULL;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t channels_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t messages_mutex = PTHREAD_MUTEX_INITIALIZER;

Channel channels[MAX_CHANNELS];
int channel_count = 0;

User users[MAX_USERS];
int user_count = 0;

Message messages[MAX_MESSAGES];
int message_count = 0;

Reaction reactions[MAX_REACTIONS];
int reaction_count = 0;

MutedUser muted_users[MAX_MUTED_USERS];
int muted_user_count = 0;

// Function to check if a user has permission for an operation
bool has_permission(int user_id, const char *operation, int target_id)
{
  int user_role = ROLE_GUEST;

  // Find user's role
  for (int i = 0; i < user_count; i++)
  {
    if (i == user_id)
    {
      user_role = users[i].role;
      break;
    }
  }

  // Admin has all permissions
  if (user_role == ROLE_ADMIN)
  {
    return true;
  }

  // Check specific operations
  if (strcmp(operation, "create_channel") == 0 ||
      strcmp(operation, "delete_channel") == 0 ||
      strcmp(operation, "set_role") == 0)
  {
    // Only admin can do these
    return false;
  }
  else if (strcmp(operation, "kick_user") == 0 ||
           strcmp(operation, "mute_user") == 0)
  {
    // Moderator+ can do these
    return user_role >= ROLE_MODERATOR;
  }
  else if (strcmp(operation, "react") == 0)
  {
    // Member+ can do these
    return user_role >= ROLE_MEMBER;
  }

  // Default allow for unspecified operations
  return true;
}

// Initialize mock data
void initialize_mock_data()
{
  // Create default channels
  strcpy(channels[0].name, "general");
  strcpy(channels[0].type, "public");
  channels[0].creator_id = 0;

  strcpy(channels[1].name, "random");
  strcpy(channels[1].type, "public");
  channels[1].creator_id = 0;

  channel_count = 2;

  // Create test users with different roles
  strcpy(users[0].username, "admin");
  users[0].is_online = false;
  strcpy(users[0].current_channel, "general");
  users[0].role = ROLE_ADMIN;

  strcpy(users[1].username, "moderator");
  users[1].is_online = false;
  strcpy(users[1].current_channel, "general");
  users[1].role = ROLE_MODERATOR;

  strcpy(users[2].username, "member");
  users[2].is_online = false;
  strcpy(users[2].current_channel, "general");
  users[2].role = ROLE_MEMBER;

  user_count = 3;

  // Create some initial messages
  strcpy(messages[0].content, "Welcome to MyDiscord!");
  messages[0].user_id = 0;    // Posted by admin
  messages[0].channel_id = 0; // In general channel
  messages[0].timestamp = time(NULL);
  messages[0].is_deleted = false;
  messages[0].id = 0;

  message_count = 1;

  printf("[LOG] Initialized mock data: %d channels, %d users, %d messages\n",
         channel_count, user_count, message_count);
}

// Find user index by username
int find_user_by_name(const char *username)
{
  for (int i = 0; i < user_count; i++)
  {
    if (strcmp(users[i].username, username) == 0)
    {
      return i;
    }
  }
  return -1;
}

// Find user index by socket
int find_user_by_socket(int socket)
{
  for (int i = 0; i < user_count; i++)
  {
    if (users[i].socket == socket && users[i].is_online)
    {
      return i;
    }
  }
  return -1;
}

// Find channel index by name
int find_channel_by_name(const char *channel_name)
{
  for (int i = 0; i < channel_count; i++)
  {
    if (strcmp(channels[i].name, channel_name) == 0)
    {
      return i;
    }
  }
  return -1;
}

// Check if a user is muted in a channel
bool is_user_muted(int user_id, int channel_id)
{
  time_t now = time(NULL);

  for (int i = 0; i < muted_user_count; i++)
  {
    if (muted_users[i].user_id == user_id &&
        muted_users[i].channel_id == channel_id)
    {
      if (muted_users[i].end_time > now)
      {
        return true;
      }
    }
  }
  return false;
}

// Add a user to the muted list
void mute_user(int user_id, int channel_id, int minutes)
{
  if (muted_user_count < MAX_MUTED_USERS)
  {
    muted_users[muted_user_count].user_id = user_id;
    muted_users[muted_user_count].channel_id = channel_id;
    muted_users[muted_user_count].end_time = time(NULL) + (minutes * 60);
    muted_user_count++;
    printf("[LOG] User ID %d muted in channel %d for %d minutes\n",
           user_id, channel_id, minutes);
  }
}

// Store a new message
int store_message(int user_id, int channel_id, const char *content)
{
  pthread_mutex_lock(&messages_mutex);

  if (message_count < MAX_MESSAGES)
  {
    messages[message_count].id = message_count;
    messages[message_count].user_id = user_id;
    messages[message_count].channel_id = channel_id;
    strncpy(messages[message_count].content, content, BUFFER_SIZE - 1);
    messages[message_count].content[BUFFER_SIZE - 1] = '\0';
    messages[message_count].timestamp = time(NULL);
    messages[message_count].is_deleted = false;

    int msg_id = message_count;
    message_count++;

    pthread_mutex_unlock(&messages_mutex);
    return msg_id;
  }

  pthread_mutex_unlock(&messages_mutex);
  return -1; // Failed to store
}

// Add a reaction to a message
bool add_reaction(int user_id, int message_id, const char *emoji)
{
  pthread_mutex_lock(&messages_mutex);

  // Check if message exists and is not deleted
  bool message_found = false;
  for (int i = 0; i < message_count; i++)
  {
    if (messages[i].id == message_id && !messages[i].is_deleted)
    {
      message_found = true;
      break;
    }
  }

  if (!message_found)
  {
    pthread_mutex_unlock(&messages_mutex);
    return false;
  }

  // Add the reaction
  if (reaction_count < MAX_REACTIONS)
  {
    reactions[reaction_count].message_id = message_id;
    reactions[reaction_count].user_id = user_id;
    strncpy(reactions[reaction_count].emoji, emoji, 7);
    reactions[reaction_count].emoji[7] = '\0';
    reaction_count++;

    pthread_mutex_unlock(&messages_mutex);
    return true;
  }

  pthread_mutex_unlock(&messages_mutex);
  return false;
}

// Get role name string
const char *get_role_name(int role)
{
  switch (role)
  {
  case ROLE_ADMIN:
    return "Admin";
  case ROLE_MODERATOR:
    return "Moderator";
  case ROLE_MEMBER:
    return "Member";
  case ROLE_GUEST:
  default:
    return "Guest";
  }
}

// Get current timestamp as string
void get_timestamp(char *buffer, size_t size)
{
  time_t now = time(NULL);
  strftime(buffer, size, "%H:%M:%S", localtime(&now));
}

// Add client to the list
void add_client(int socket, const char *username, const char *channel)
{
  pthread_mutex_lock(&clients_mutex);

  client_node *new_node = malloc(sizeof(client_node));
  new_node->socket = socket;
  strncpy(new_node->username, username, sizeof(new_node->username) - 1);
  new_node->username[sizeof(new_node->username) - 1] = '\0';
  strncpy(new_node->current_channel, channel, sizeof(new_node->current_channel) - 1);
  new_node->current_channel[sizeof(new_node->current_channel) - 1] = '\0';
  new_node->next = clients_head;
  clients_head = new_node;

  pthread_mutex_unlock(&clients_mutex);

  printf("[LOG] Added client %s to channel #%s\n", username, channel);
}

// Remove client from the list
void remove_client(int socket)
{
  pthread_mutex_lock(&clients_mutex);

  client_node *current = clients_head;
  client_node *prev = NULL;

  while (current != NULL)
  {
    if (current->socket == socket)
    {
      if (prev == NULL)
      {
        clients_head = current->next;
      }
      else
      {
        prev->next = current->next;
      }

      printf("[LOG] Removed client %s from list\n", current->username);
      free(current);
      break;
    }
    prev = current;
    current = current->next;
  }

  pthread_mutex_unlock(&clients_mutex);
}

// Update client channel
void update_client_channel(int socket, const char *channel)
{
  pthread_mutex_lock(&clients_mutex);

  client_node *current = clients_head;
  while (current != NULL)
  {
    if (current->socket == socket)
    {
      strncpy(current->current_channel, channel, sizeof(current->current_channel) - 1);
      current->current_channel[sizeof(current->current_channel) - 1] = '\0';
      printf("[LOG] Updated client channel to #%s\n", channel);
      break;
    }
    current = current->next;
  }

  pthread_mutex_unlock(&clients_mutex);
}

// Create a new channel
bool create_channel(int creator_id, const char *name, const char *type)
{
  pthread_mutex_lock(&channels_mutex);

  // Check if channel already exists
  for (int i = 0; i < channel_count; i++)
  {
    if (strcmp(channels[i].name, name) == 0)
    {
      pthread_mutex_unlock(&channels_mutex);
      return false;
    }
  }

  // Create new channel
  if (channel_count < MAX_CHANNELS)
  {
    strncpy(channels[channel_count].name, name, sizeof(channels[channel_count].name) - 1);
    channels[channel_count].name[sizeof(channels[channel_count].name) - 1] = '\0';

    strncpy(channels[channel_count].type, type, sizeof(channels[channel_count].type) - 1);
    channels[channel_count].type[sizeof(channels[channel_count].type) - 1] = '\0';

    channels[channel_count].creator_id = creator_id;
    channel_count++;

    pthread_mutex_unlock(&channels_mutex);
    return true;
  }

  pthread_mutex_unlock(&channels_mutex);
  return false;
}

// Delete a channel
bool delete_channel(int user_id, const char *name)
{
  pthread_mutex_lock(&channels_mutex);

  // Can't delete the general channel
  if (strcmp(name, "general") == 0)
  {
    pthread_mutex_unlock(&channels_mutex);
    return false;
  }

  // Find channel
  int channel_idx = -1;
  for (int i = 0; i < channel_count; i++)
  {
    if (strcmp(channels[i].name, name) == 0)
    {
      channel_idx = i;
      break;
    }
  }

  if (channel_idx == -1)
  {
    pthread_mutex_unlock(&channels_mutex);
    return false;
  }

  // Check if user has permission
  int user_role = users[user_id].role;
  if (user_role < ROLE_ADMIN && channels[channel_idx].creator_id != user_id)
  {
    pthread_mutex_unlock(&channels_mutex);
    return false;
  }

  // Delete channel by shifting remaining channels
  for (int i = channel_idx; i < channel_count - 1; i++)
  {
    strcpy(channels[i].name, channels[i + 1].name);
    strcpy(channels[i].type, channels[i + 1].type);
    channels[i].creator_id = channels[i + 1].creator_id;
  }

  channel_count--;
  pthread_mutex_unlock(&channels_mutex);
  return true;
}

// Set a user's role
bool set_user_role(int target_user_idx, int new_role)
{
  pthread_mutex_lock(&users_mutex);

  if (target_user_idx >= 0 && target_user_idx < user_count)
  {
    users[target_user_idx].role = new_role;
    pthread_mutex_unlock(&users_mutex);
    return true;
  }

  pthread_mutex_unlock(&users_mutex);
  return false;
}

// Broadcast message to all clients in a specific channel
void broadcast_to_channel(int sender_socket, const char *message, const char *channel)
{
  pthread_mutex_lock(&clients_mutex);

  char timestamp[20];
  get_timestamp(timestamp, sizeof(timestamp));

  // Find sender's username
  client_node *sender = clients_head;
  char sender_name[64] = "Unknown";

  while (sender != NULL)
  {
    if (sender->socket == sender_socket)
    {
      strcpy(sender_name, sender->username);
      break;
    }
    sender = sender->next;
  }

  // Format message with timestamp and sender
  char formatted_message[BUFFER_SIZE + 128];
  snprintf(formatted_message, sizeof(formatted_message),
           "[%s] %s: %s", timestamp, sender_name, message);

  // Send to all clients in the specified channel
  client_node *current = clients_head;
  while (current != NULL)
  {
    if (current->socket != sender_socket &&
        strcmp(current->current_channel, channel) == 0)
    {
      send(current->socket, formatted_message, strlen(formatted_message), 0);
    }
    current = current->next;
  }

  pthread_mutex_unlock(&clients_mutex);

  printf("[LOG] Broadcast to #%s: %s\n", channel, formatted_message);
}

// Send private message to specific user
void send_private_message(int sender_socket, const char *recipient, const char *message)
{
  pthread_mutex_lock(&clients_mutex);

  char timestamp[20];
  get_timestamp(timestamp, sizeof(timestamp));

  // Find sender's username
  client_node *sender = clients_head;
  char sender_name[64] = "Unknown";

  while (sender != NULL)
  {
    if (sender->socket == sender_socket)
    {
      strcpy(sender_name, sender->username);
      break;
    }
    sender = sender->next;
  }

  // Format private message
  char formatted_message[BUFFER_SIZE + 128];
  snprintf(formatted_message, sizeof(formatted_message),
           "[%s] [PM from %s]: %s", timestamp, sender_name, message);

  // Find recipient and send message
  bool recipient_found = false;
  client_node *current = clients_head;
  while (current != NULL)
  {
    if (strcmp(current->username, recipient) == 0)
    {
      send(current->socket, formatted_message, strlen(formatted_message), 0);
      recipient_found = true;

      // Also send confirmation to sender
      snprintf(formatted_message, sizeof(formatted_message),
               "[%s] [PM to %s]: %s", timestamp, recipient, message);
      send(sender_socket, formatted_message, strlen(formatted_message), 0);
      break;
    }
    current = current->next;
  }

  // If recipient not found, inform sender
  if (!recipient_found)
  {
    snprintf(formatted_message, sizeof(formatted_message),
             "[SYSTEM] User '%s' is not online.", recipient);
    send(sender_socket, formatted_message, strlen(formatted_message), 0);
  }

  pthread_mutex_unlock(&clients_mutex);

  printf("[LOG] Private message from %s to %s\n", sender_name, recipient);
}

// Send channel list to client
void send_channel_list(int client_socket)
{
  char response[BUFFER_SIZE];
  strcpy(response, "[SYSTEM] Available channels:\n");

  for (int i = 0; i < channel_count; i++)
  {
    char channel_info[128];
    sprintf(channel_info, "  #%s (%s)\n", channels[i].name, channels[i].type);
    strcat(response, channel_info);
  }

  send(client_socket, response, strlen(response), 0);
  printf("[LOG] Sent channel list to client\n");
}

// Send user list to client
void send_user_list(int client_socket, const char *channel)
{
  pthread_mutex_lock(&clients_mutex);

  char response[BUFFER_SIZE];
  sprintf(response, "[SYSTEM] Users in #%s:\n", channel);

  client_node *current = clients_head;
  int user_count = 0;

  while (current != NULL)
  {
    if (strcmp(current->current_channel, channel) == 0)
    {
      // Find user role
      int user_idx = find_user_by_name(current->username);
      const char *role_name = "Unknown";
      if (user_idx >= 0)
      {
        role_name = get_role_name(users[user_idx].role);
      }

      char user_info[128];
      sprintf(user_info, "  %s (%s)\n", current->username, role_name);
      strcat(response, user_info);
      user_count++;
    }
    current = current->next;
  }

  if (user_count == 0)
  {
    strcat(response, "  No users currently in this channel\n");
  }

  pthread_mutex_unlock(&clients_mutex);

  send(client_socket, response, strlen(response), 0);
  printf("[LOG] Sent user list for #%s to client\n", channel);
}

void *handle_client(void *arg)
{
  int client_socket = *((int *)arg);
  free(arg);

  char username[64] = "Anonymous";
  char current_channel[32] = "general";
  bool is_authenticated = false;
  int user_id = -1;

  char buffer[BUFFER_SIZE];
  char response[BUFFER_SIZE + 32];

  // Send welcome message
  sprintf(response, "Welcome to MyDiscord! You are now connected.");
  send(client_socket, response, strlen(response), 0);

  while (1)
  {
    ssize_t bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_read <= 0)
      break;

    buffer[bytes_read] = '\0';
    printf("[LOG] Received: %s\n", buffer);

    // Check message type based on prefix
    if (strncmp(buffer, "LOGIN:", 6) == 0)
    {
      // Process login message: LOGIN:username:password
      char login_username[64] = {0};
      char login_password[64] = {0};

      // Extract username and password
      char *username_start = strchr(buffer, ':') + 1;
      char *username_end = strchr(username_start, ':');

      if (username_end != NULL)
      {
        size_t username_len = username_end - username_start;
        strncpy(login_username, username_start, username_len);
        login_username[username_len] = '\0';

        strncpy(login_password, username_end + 1, 63);

        // In a real app, we would verify the password
        // For this mock, just accept any non-empty password
        if (strlen(login_password) > 0)
        {
          strcpy(username, login_username);
          is_authenticated = true;

          // Add or update user in our user list
          user_id = find_user_by_name(username);
          if (user_id == -1)
          {
            // New user - add as member
            user_id = user_count++;
            strcpy(users[user_id].username, username);
            users[user_id].role = ROLE_MEMBER; // Default role is member
          }

          users[user_id].socket = client_socket;
          users[user_id].is_online = true;
          strcpy(users[user_id].current_channel, current_channel);

          // Announce that user has joined
          char join_msg[256];
          sprintf(join_msg, "[SYSTEM] %s has joined the chat!", username);
          broadcast_to_channel(-1, join_msg, current_channel);

          // Add to clients list for broadcasting
          add_client(client_socket, username, current_channel);

          // Send success response
          send(client_socket, "LOGIN_SUCCESS", strlen("LOGIN_SUCCESS"), 0);
          printf("[LOG] User %s authenticated successfully with role %s\n",
                 username, get_role_name(users[user_id].role));

          // Send role information to client (as separate message)
          sleep(1); // Small delay to ensure messages don't get combined
          char role_msg[32];
          sprintf(role_msg, "ROLE_UPDATE:%d", users[user_id].role);
          send(client_socket, role_msg, strlen(role_msg), 0);
        }
        else
        {
          // Password empty
          send(client_socket, "[ERROR] Password cannot be empty", strlen("[ERROR] Password cannot be empty"), 0);
        }
      }
      else
      {
        // Invalid format
        send(client_socket, "[ERROR] Invalid login format", strlen("[ERROR] Invalid login format"), 0);
      }
    }
    else if (strncmp(buffer, "MSG:", 4) == 0 && is_authenticated)
    {
      // Process message: MSG:channel:content
      char msg_channel[32] = {0};
      char msg_content[BUFFER_SIZE] = {0};

      // Extract channel and content
      char *channel_start = strchr(buffer, ':') + 1;
      char *channel_end = strchr(channel_start, ':');

      if (channel_end != NULL)
      {
        size_t channel_len = channel_end - channel_start;
        strncpy(msg_channel, channel_start, channel_len);
        msg_channel[channel_len] = '\0';

        strcpy(msg_content, channel_end + 1);

        // Verify channel exists
        int channel_id = find_channel_by_name(msg_channel);
        if (channel_id != -1)
        {
          // Check if user is muted
          if (is_user_muted(user_id, channel_id))
          {
            sprintf(response, "[SYSTEM] You are currently muted in #%s", msg_channel);
            send(client_socket, response, strlen(response), 0);
            continue;
          }

          // Store message and get ID
          int msg_id = store_message(user_id, channel_id, msg_content);

          // Broadcast to the specified channel
          broadcast_to_channel(client_socket, msg_content, msg_channel);

          // Echo back to sender with message ID
          char timestamp[20];
          get_timestamp(timestamp, sizeof(timestamp));
          sprintf(response, "[%s] You: %s (ID: %d)", timestamp, msg_content, msg_id);
          send(client_socket, response, strlen(response), 0);
        }
        else
        {
          sprintf(response, "[ERROR] Channel #%s doesn't exist", msg_channel);
          send(client_socket, response, strlen(response), 0);
        }
      }
      else
      {
        send(client_socket, "[ERROR] Invalid message format", strlen("[ERROR] Invalid message format"), 0);
      }
    }
    else if (strncmp(buffer, "CHANNEL:", 8) == 0 && is_authenticated)
    {
      // Process channel switch: CHANNEL:channel_name
      char new_channel[32] = {0};
      strcpy(new_channel, buffer + 8);

      // Check if channel exists
      if (find_channel_by_name(new_channel) != -1)
      {
        // Update user's current channel
        strcpy(current_channel, new_channel);

        // Update user in our tables
        if (user_id != -1)
        {
          strcpy(users[user_id].current_channel, new_channel);
        }

        // Update in clients list
        update_client_channel(client_socket, new_channel);

        // Inform client
        sprintf(response, "[SYSTEM] You've joined #%s", new_channel);
        send(client_socket, response, strlen(response), 0);

        // Announce to the new channel
        char join_msg[256];
        sprintf(join_msg, "[SYSTEM] %s has joined #%s", username, new_channel);
        broadcast_to_channel(-1, join_msg, new_channel);
      }
      else
      {
        sprintf(response, "[ERROR] Channel #%s doesn't exist", new_channel);
        send(client_socket, response, strlen(response), 0);
      }
    }
    else if (strncmp(buffer, "LIST_CHANNELS", 13) == 0 && is_authenticated)
    {
      // Send channel list to client
      send_channel_list(client_socket);
    }
    else if (strncmp(buffer, "LIST_USERS:", 11) == 0 && is_authenticated)
    {
      // Process user list request: LIST_USERS:channel
      char channel_name[32] = {0};
      strcpy(channel_name, buffer + 11);

      // Check if channel exists
      if (find_channel_by_name(channel_name) != -1)
      {
        send_user_list(client_socket, channel_name);
      }
      else
      {
        sprintf(response, "[ERROR] Channel #%s doesn't exist", channel_name);
        send(client_socket, response, strlen(response), 0);
      }
    }
    else if (strncmp(buffer, "PM:", 3) == 0 && is_authenticated)
    {
      // Process private message: PM:recipient:content
      char recipient[64] = {0};
      char pm_content[BUFFER_SIZE] = {0};

      // Extract recipient and content
      char *recipient_start = strchr(buffer, ':') + 1;
      char *recipient_end = strchr(recipient_start, ':');

      if (recipient_end != NULL)
      {
        size_t recipient_len = recipient_end - recipient_start;
        strncpy(recipient, recipient_start, recipient_len);
        recipient[recipient_len] = '\0';

        strcpy(pm_content, recipient_end + 1);

        // Send the private message
        send_private_message(client_socket, recipient, pm_content);
      }
      else
      {
        send(client_socket, "[ERROR] Invalid PM format", strlen("[ERROR] Invalid PM format"), 0);
      }
    }
    else if (strncmp(buffer, "CREATE_CHANNEL:", 15) == 0 && is_authenticated)
    {
      // Process channel creation: CREATE_CHANNEL:type:name
      // Check permission first
      if (users[user_id].role < ROLE_ADMIN)
      {
        send(client_socket, "[ERROR] You don't have permission to create channels",
             strlen("[ERROR] You don't have permission to create channels"), 0);
        continue;
      }

      char channel_type[10] = {0};
      char channel_name[32] = {0};

      char *type_start = strchr(buffer, ':') + 1;
      char *type_end = strchr(type_start, ':');

      if (type_end != NULL)
      {
        size_t type_len = type_end - type_start;
        strncpy(channel_type, type_start, type_len);
        channel_type[type_len] = '\0';

        strcpy(channel_name, type_end + 1);

        // Validate type
        if (strcmp(channel_type, "public") != 0 && strcmp(channel_type, "private") != 0)
        {
          send(client_socket, "[ERROR] Channel type must be 'public' or 'private'",
               strlen("[ERROR] Channel type must be 'public' or 'private'"), 0);
          continue;
        }

        // Create the channel
        if (create_channel(user_id, channel_name, channel_type))
        {
          sprintf(response, "[SYSTEM] Channel #%s created successfully", channel_name);
          send(client_socket, response, strlen(response), 0);

          // Announce to all users
          char announce[256];
          sprintf(announce, "[SYSTEM] New %s channel #%s has been created by %s",
                  channel_type, channel_name, username);
          broadcast_to_channel(-1, announce, "general");
        }
        else
        {
          sprintf(response, "[ERROR] Failed to create channel #%s", channel_name);
          send(client_socket, response, strlen(response), 0);
        }
      }
      else
      {
        send(client_socket, "[ERROR] Invalid channel creation format",
             strlen("[ERROR] Invalid channel creation format"), 0);
      }
    }
    else if (strncmp(buffer, "DELETE_CHANNEL:", 15) == 0 && is_authenticated)
    {
      // Process channel deletion: DELETE_CHANNEL:name
      // Check permission first
      if (users[user_id].role < ROLE_ADMIN)
      {
        send(client_socket, "[ERROR] You don't have permission to delete channels",
             strlen("[ERROR] You don't have permission to delete channels"), 0);
        continue;
      }

      char channel_name[32] = {0};
      strcpy(channel_name, buffer + 15);

      // Delete the channel
      if (delete_channel(user_id, channel_name))
      {
        sprintf(response, "[SYSTEM] Channel #%s deleted successfully", channel_name);
        send(client_socket, response, strlen(response), 0);

        // Announce to all users
        char announce[256];
        sprintf(announce, "[SYSTEM] Channel #%s has been deleted by %s",
                channel_name, username);
        broadcast_to_channel(-1, announce, "general");
      }
      else
      {
        sprintf(response, "[ERROR] Failed to delete channel #%s", channel_name);
        send(client_socket, response, strlen(response), 0);
      }
    }
    else if (strncmp(buffer, "SET_ROLE:", 9) == 0 && is_authenticated)
    {
      // Process role setting: SET_ROLE:username:role
      // Check permission first
      if (users[user_id].role < ROLE_ADMIN)
      {
        send(client_socket, "[ERROR] You don't have permission to set roles",
             strlen("[ERROR] You don't have permission to set roles"), 0);
        continue;
      }

      char target_user[64] = {0};
      int new_role = ROLE_MEMBER;

      char *user_start = strchr(buffer, ':') + 1;
      char *user_end = strchr(user_start, ':');

      if (user_end != NULL)
      {
        size_t user_len = user_end - user_start;
        strncpy(target_user, user_start, user_len);
        target_user[user_len] = '\0';

        new_role = atoi(user_end + 1);

        // Validate role
        if (new_role < ROLE_GUEST || new_role > ROLE_ADMIN)
        {
          send(client_socket, "[ERROR] Invalid role (0=Guest, 1=Member, 2=Mod, 3=Admin)",
               strlen("[ERROR] Invalid role (0=Guest, 1=Member, 2=Mod, 3=Admin)"), 0);
          continue;
        }

        // Find target user
        int target_id = find_user_by_name(target_user);
        if (target_id == -1)
        {
          sprintf(response, "[ERROR] User %s not found", target_user);
          send(client_socket, response, strlen(response), 0);
          continue;
        }

        // Set the role
        if (set_user_role(target_id, new_role))
        {
          sprintf(response, "[SYSTEM] %s's role updated to %s",
                  target_user, get_role_name(new_role));
          send(client_socket, response, strlen(response), 0);

          // Notify the target user if online
          client_node *current = clients_head;
          while (current != NULL)
          {
            if (strcmp(current->username, target_user) == 0)
            {
              char role_msg[32];
              sprintf(role_msg, "ROLE_UPDATE:%d", new_role);
              send(current->socket, role_msg, strlen(role_msg), 0);

              sprintf(response, "[SYSTEM] Your role has been updated to %s by %s",
                      get_role_name(new_role), username);
              send(current->socket, response, strlen(response), 0);
              break;
            }
            current = current->next;
          }
        }
        else
        {
          sprintf(response, "[ERROR] Failed to update %s's role", target_user);
          send(client_socket, response, strlen(response), 0);
        }
      }
      else
      {
        send(client_socket, "[ERROR] Invalid role setting format",
             strlen("[ERROR] Invalid role setting format"), 0);
      }
    }
    else if (strncmp(buffer, "KICK:", 5) == 0 && is_authenticated)
    {
      // Process kick: KICK:channel:username
      // Check permission first
      if (users[user_id].role < ROLE_MODERATOR)
      {
        send(client_socket, "[ERROR] You don't have permission to kick users",
             strlen("[ERROR] You don't have permission to kick users"), 0);
        continue;
      }

      char kick_channel[32] = {0};
      char kick_user[64] = {0};

      char *channel_start = strchr(buffer, ':') + 1;
      char *channel_end = strchr(channel_start, ':');

      if (channel_end != NULL)
      {
        size_t channel_len = channel_end - channel_start;
        strncpy(kick_channel, channel_start, channel_len);
        kick_channel[channel_len] = '\0';

        strcpy(kick_user, channel_end + 1);

        // Find target user
        int target_id = find_user_by_name(kick_user);
        if (target_id == -1)
        {
          sprintf(response, "[ERROR] User %s not found", kick_user);
          send(client_socket, response, strlen(response), 0);
          continue;
        }

        // Check target user's role (can't kick higher roles)
        if (users[target_id].role >= users[user_id].role && user_id != 0)
        {
          sprintf(response, "[ERROR] You cannot kick a user with equal or higher role");
          send(client_socket, response, strlen(response), 0);
          continue;
        }

        // Find channel
        int channel_id = find_channel_by_name(kick_channel);
        if (channel_id == -1)
        {
          sprintf(response, "[ERROR] Channel #%s not found", kick_channel);
          send(client_socket, response, strlen(response), 0);
          continue;
        }

        // Move user to general channel
        client_node *current = clients_head;
        while (current != NULL)
        {
          if (strcmp(current->username, kick_user) == 0 &&
              strcmp(current->current_channel, kick_channel) == 0)
          {

            // Update their channel
            strcpy(current->current_channel, "general");
            strcpy(users[target_id].current_channel, "general");

            // Notify the kicked user
            sprintf(response, "[SYSTEM] You have been kicked from #%s by %s",
                    kick_channel, username);
            send(current->socket, response, strlen(response), 0);

            // Notify the channel
            char announce[256];
            sprintf(announce, "[SYSTEM] %s has been kicked from the channel by %s",
                    kick_user, username);
            broadcast_to_channel(-1, announce, kick_channel);

            // Confirm to the moderator
            sprintf(response, "[SYSTEM] %s has been kicked from #%s",
                    kick_user, kick_channel);
            send(client_socket, response, strlen(response), 0);
            break;
          }
          current = current->next;
        }
      }
      else
      {
        send(client_socket, "[ERROR] Invalid kick format",
             strlen("[ERROR] Invalid kick format"), 0);
      }
    }
    else if (strncmp(buffer, "MUTE:", 5) == 0 && is_authenticated)
    {
      // Process mute: MUTE:channel:username:duration
      // Check permission first
      if (users[user_id].role < ROLE_MODERATOR)
      {
        send(client_socket, "[ERROR] You don't have permission to mute users",
             strlen("[ERROR] You don't have permission to mute users"), 0);
        continue;
      }

      char mute_channel[32] = {0};
      char muted_username[64] = {0};
      int duration = 0;

      // Extract data using strtok
      char *cmd_copy = strdup(buffer);
      char *token = strtok(cmd_copy, ":");

      token = strtok(NULL, ":"); // channel
      if (token)
        strcpy(mute_channel, token);

      token = strtok(NULL, ":"); // username
      if (token)
        strcpy(muted_username, token);

      token = strtok(NULL, ":"); // duration
      if (token)
        duration = atoi(token);

      free(cmd_copy);

      if (strlen(mute_channel) > 0 && strlen(muted_username) > 0 && duration > 0)
      {
        // Find target user
        int target_id = find_user_by_name(muted_username);
        if (target_id == -1)
        {
          sprintf(response, "[ERROR] User %s not found", muted_username);
          send(client_socket, response, strlen(response), 0);
          continue;
        }

        // Check target user's role (can't mute higher roles)
        if (users[target_id].role >= users[user_id].role && user_id != 0)
        {
          sprintf(response, "[ERROR] You cannot mute a user with equal or higher role");
          send(client_socket, response, strlen(response), 0);
          continue;
        }

        // Find channel
        int channel_id = find_channel_by_name(mute_channel);
        if (channel_id == -1)
        {
          sprintf(response, "[ERROR] Channel #%s not found", mute_channel);
          send(client_socket, response, strlen(response), 0);
          continue;
        }

        // Mute user
        mute_user(target_id, channel_id, duration);

        // Notify the muted user
        client_node *current = clients_head;
        while (current != NULL)
        {
          if (strcmp(current->username, muted_username) == 0)
          {
            sprintf(response, "[SYSTEM] You have been muted in #%s for %d minutes by %s",
                    mute_channel, duration, username);
            send(current->socket, response, strlen(response), 0);
            break;
          }
          current = current->next;
        }

        // Notify the channel
        char announce[256];
        sprintf(announce, "[SYSTEM] %s has been muted for %d minutes by %s",
                muted_username, duration, username);
        broadcast_to_channel(-1, announce, mute_channel);

        // Confirm to the moderator
        sprintf(response, "[SYSTEM] %s has been muted in #%s for %d minutes",
                muted_username, mute_channel, duration);
        send(client_socket, response, strlen(response), 0);
      }
      else
      {
        send(client_socket, "[ERROR] Invalid mute format: MUTE:channel:username:duration",
             strlen("[ERROR] Invalid mute format: MUTE:channel:username:duration"), 0);
      }
    }
    else if (strncmp(buffer, "REACT:", 6) == 0 && is_authenticated)
    {
      // Process reaction: REACT:message_id:emoji
      // Check permission first
      if (users[user_id].role < ROLE_MEMBER)
      {
        send(client_socket, "[ERROR] You don't have permission to react to messages",
             strlen("[ERROR] You don't have permission to react to messages"), 0);
        continue;
      }

      int message_id = -1;
      char emoji[8] = {0};

      char *mid_start = strchr(buffer, ':') + 1;
      char *mid_end = strchr(mid_start, ':');

      if (mid_end != NULL)
      {
        message_id = atoi(mid_start);
        strncpy(emoji, mid_end + 1, 7);
        emoji[7] = '\0';

        // Add reaction
        if (add_reaction(user_id, message_id, emoji))
        {
          sprintf(response, "[SYSTEM] Added reaction %s to message %d", emoji, message_id);
          send(client_socket, response, strlen(response), 0);

          // TODO: Broadcast reaction to other users in same channel as the message
        }
        else
        {
          sprintf(response, "[ERROR] Failed to add reaction to message %d", message_id);
          send(client_socket, response, strlen(response), 0);
        }
      }
      else
      {
        send(client_socket, "[ERROR] Invalid reaction format",
             strlen("[ERROR] Invalid reaction format"), 0);
      }
    }
    else if (!is_authenticated)
    {
      // Not authenticated yet
      send(client_socket, "[ERROR] Please login first", strlen("[ERROR] Please login first"), 0);
    }
    else
    {
      // Unrecognized message format, treat as plain text to current channel
      broadcast_to_channel(client_socket, buffer, current_channel);

      // Echo back to sender
      char timestamp[20];
      get_timestamp(timestamp, sizeof(timestamp));
      sprintf(response, "[%s] You: %s", timestamp, buffer);
      send(client_socket, response, strlen(response), 0);
    }
  }

  // Handle client disconnection
  if (is_authenticated)
  {
    // Mark user as offline
    if (user_id != -1)
    {
      users[user_id].is_online = false;
    }

    // Announce departure
    char leave_msg[256];
    sprintf(leave_msg, "[SYSTEM] %s has left the chat", username);
    broadcast_to_channel(-1, leave_msg, current_channel);

    // Remove from clients list
    remove_client(client_socket);
  }

  close(client_socket);
  printf("[LOG] Client disconnected: %s\n", username);

  return NULL;
}

int main()
{
  printf("Starting MyDiscord mock server...\n");

  // Initialize mock data
  initialize_mock_data();

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0)
  {
    perror("Socket creation failed");
    return EXIT_FAILURE;
  }

  // Allow reuse of address
  int opt = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
  {
    perror("Setsockopt failed");
    return EXIT_FAILURE;
  }

  struct sockaddr_in address = {
      .sin_family = AF_INET,
      .sin_port = htons(8080),
      .sin_addr.s_addr = INADDR_ANY};

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
  {
    perror("Bind failed");
    return EXIT_FAILURE;
  }

  if (listen(server_fd, MAX_CLIENTS) < 0)
  {
    perror("Listen failed");
    return EXIT_FAILURE;
  }

  printf("Mock server listening on port 8080...\n");
  printf("Channels: general, random\n");
  printf("Test users: admin (role=Admin), moderator (role=Moderator), member (role=Member)\n");

  while (1)
  {
    struct sockaddr_in client_addr;
    socklen_t addr_size = sizeof(client_addr);

    int *client_socket = malloc(sizeof(int));
    *client_socket = accept(server_fd,
                            (struct sockaddr *)&client_addr,
                            &addr_size);

    if (*client_socket < 0)
    {
      perror("Accept failed");
      free(client_socket);
      continue;
    }

    printf("New client connected: %s:%d\n",
           inet_ntoa(client_addr.sin_addr),
           ntohs(client_addr.sin_port));

    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, handle_client, client_socket) != 0)
    {
      perror("Thread creation failed");
      close(*client_socket);
      free(client_socket);
      continue;
    }

    pthread_detach(thread_id);
  }

  return 0;
}