#include "../../include/database.h"
#include "../../include/common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Simple file-based "database" implementation

bool db_connect(DatabaseContext *db)
{
  if (!db)
    return false;

  pthread_mutex_init(&db->db_mutex, NULL);

  // Try to open the database file in append mode to create it if it doesn't exist
  FILE *file = fopen(DB_FILE, "a+");
  if (!file)
  {
    fprintf(stderr, "Failed to open database file: %s\n", DB_FILE);
    return false;
  }
  fclose(file);

  db->connected = true;
  printf("Connected to file-based database: %s\n", DB_FILE);
  return true;
}

bool db_disconnect(DatabaseContext *db)
{
  if (!db || !db->connected)
    return false;

  pthread_mutex_destroy(&db->db_mutex);
  db->connected = false;
  printf("Disconnected from database\n");
  return true;
}

bool db_execute(DatabaseContext *db, const char *query)
{
  if (!db || !db->connected || !query)
    return false;

  pthread_mutex_lock(&db->db_mutex);

  // In a file-based implementation, this would append the "query" to the log file
  FILE *file = fopen(DB_FILE, "a");
  if (!file)
  {
    pthread_mutex_unlock(&db->db_mutex);
    return false;
  }

  // For simplicity, just log the command with timestamp
  time_t now = time(NULL);
  fprintf(file, "[%ld] EXECUTE: %s\n", now, query);
  fclose(file);

  pthread_mutex_unlock(&db->db_mutex);
  return true;
}

void *db_query(DatabaseContext *db, const char *query)
{
  if (!db || !db->connected || !query)
    return NULL;

  // For a file-based implementation, we would parse the query and return appropriate data
  // This is a very simplified version just to make the code compile

  // Return a dummy result for now
  char *result = malloc(100);
  if (result)
  {
    sprintf(result, "Dummy result for query: %.50s", query);
  }
  return result;
}

bool db_create_user(DatabaseContext *db, const char *username, const char *password_hash, const char *email)
{
  if (!db || !db->connected)
    return false;

  pthread_mutex_lock(&db->db_mutex);

  // Simple file-based implementation
  FILE *file = fopen(DB_FILE, "a");
  if (!file)
  {
    pthread_mutex_unlock(&db->db_mutex);
    return false;
  }

  // Generate a simple user ID (not secure for production)
  int user_id = rand() % 10000;

  fprintf(file, "USER_CREATE,%d,%s,%s,%s\n",
          user_id, username, password_hash, email ? email : "");
  fclose(file);

  pthread_mutex_unlock(&db->db_mutex);
  return true;
}

bool db_authenticate_user(DatabaseContext *db, const char *username, const char *password_hash)
{
  if (!db || !db->connected)
    return false;

  // For simplicity in this demo, we'll accept any login
  // In a real implementation, we would search the file for matching credentials

  // Log the authentication attempt
  FILE *file = fopen(DB_FILE, "a");
  if (file)
  {
    fprintf(file, "AUTH_ATTEMPT,%s\n", username);
    fclose(file);
  }

  return true; // Accept all logins for demo
}

bool db_update_user_last_login(DatabaseContext *db, const char *username)
{
  if (!db || !db->connected)
    return false;

  pthread_mutex_lock(&db->db_mutex);

  FILE *file = fopen(DB_FILE, "a");
  if (!file)
  {
    pthread_mutex_unlock(&db->db_mutex);
    return false;
  }

  time_t now = time(NULL);
  fprintf(file, "USER_LOGIN,%s,%ld\n", username, now);
  fclose(file);

  pthread_mutex_unlock(&db->db_mutex);
  return true;
}

bool db_create_channel(DatabaseContext *db, const char *name, const char *description)
{
  if (!db || !db->connected || !name)
    return false;

  pthread_mutex_lock(&db->db_mutex);

  // Open the DB file in append mode
  FILE *file = fopen(DB_FILE, "a");
  if (!file)
  {
    pthread_mutex_unlock(&db->db_mutex);
    return false;
  }

  // Generate a simple channel ID (not secure for production)
  int channel_id = rand() % 1000;

  // Format: CHANNEL_CREATE with easy-to-parse fields
  fprintf(file, "CHANNEL_CREATE,%d,%s,%s\n",
          channel_id, name, description ? description : "");

  fclose(file);
  pthread_mutex_unlock(&db->db_mutex);
  return true;
}

bool db_store_message(DatabaseContext *db, int channel_id, int user_id, const char *username, time_t timestamp, MessageType type, const char *content)
{
  if (!db || !db->connected || !content)
  {
    return false;
  }

  pthread_mutex_lock(&db->db_mutex);

  // Open the DB file in append mode
  FILE *file = fopen(DB_FILE, "a");
  if (!file)
  {
    pthread_mutex_unlock(&db->db_mutex);
    return false;
  }

  // Format timestamp
  char timestamp_str[32];
  strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%d %H:%M:%S", localtime(&timestamp));

  // Format: STORE_MESSAGE with easy-to-parse fields
  fprintf(file, "[%s] STORE_MESSAGE: channel_id=%d, user_id=%d, username=%s, type=%d, content=%s\n",
          timestamp_str, channel_id, user_id, username, type, content);

  fclose(file);
  pthread_mutex_unlock(&db->db_mutex);
  return true;
}

void *db_get_recent_messages(DatabaseContext *db, int channel_id, int limit, Message **out_messages)
{
  if (!db || !db->connected || limit <= 0)
  {
    return NULL;
  }

  // Allocate memory for message list including the flexible array member
  MessageList *result = (MessageList *)malloc(sizeof(MessageList) + limit * sizeof(Message));
  if (!result)
  {
    return NULL;
  }

  // Initialize count
  result->count = 0;

  pthread_mutex_lock(&db->db_mutex);

  // Reopen the file in read mode to get the messages
  FILE *read_file = fopen(DB_FILE, "r");
  if (!read_file)
  {
    pthread_mutex_unlock(&db->db_mutex);

    // If no file or no messages, return a default welcome message
    result->count = 1;
    Message *welcome = &result->messages[0];
    welcome->type = MSG_STATUS;
    strcpy(welcome->username, "System");
    strcpy(welcome->content, "Welcome to the chat! This is the beginning of the conversation.");
    welcome->timestamp = time(NULL);
    welcome->channel_id = channel_id; // Set the channel_id for the welcome message
    welcome->user_id = 0;             // System user ID

    if (out_messages != NULL)
    {
      *out_messages = result->messages;
    }

    return result;
  }

  char line[BUFFER_SIZE];
  Message messages[limit];
  int count = 0;

  // Read lines from the file and parse messages for the specified channel
  while (fgets(line, sizeof(line), read_file) && count < limit)
  {
    int msg_channel_id, msg_user_id, msg_type;
    char msg_username[MAX_USERNAME_LEN], msg_content[BUFFER_SIZE], msg_timestamp[32];

    // Parse the line to extract message data
    if (strstr(line, "STORE_MESSAGE") &&
        sscanf(line, "[%[^]]] STORE_MESSAGE: channel_id=%d, user_id=%d, username=%[^,], type=%d, content=%[^\n]",
               msg_timestamp, &msg_channel_id, &msg_user_id, msg_username, &msg_type, msg_content) == 6)
    {
      // Only include messages for the requested channel
      if (msg_channel_id == channel_id)
      {
        // Convert the stored timestamp to time_t
        struct tm tm_time = {0};
        strptime(msg_timestamp, "%Y-%m-%d %H:%M:%S", &tm_time);

        // Fill in the message structure
        messages[count].type = (MessageType)msg_type;
        strcpy(messages[count].username, msg_username);
        strcpy(messages[count].content, msg_content);
        messages[count].timestamp = mktime(&tm_time);
        messages[count].channel_id = msg_channel_id; // Set the channel_id for the message
        messages[count].user_id = msg_user_id;       // Set the user_id for the message

        count++;
      }
    }
  }

  fclose(read_file);
  pthread_mutex_unlock(&db->db_mutex);

  // If no messages were found, provide a welcome message
  if (count == 0)
  {
    result->count = 1;
    result->messages[0].type = MSG_STATUS;
    strcpy(result->messages[0].username, "System");
    strcpy(result->messages[0].content, "Welcome to the chat! This is the beginning of the conversation.");
    result->messages[0].timestamp = time(NULL);
    result->messages[0].channel_id = channel_id; // Set the channel_id for the welcome message
    result->messages[0].user_id = 0;             // System user ID
  }
  else
  {
    // Copy the messages to the result
    result->count = count;
    memcpy(result->messages, messages, count * sizeof(Message));
  }

  // If out_messages pointer is provided, set it to point to the messages array
  if (out_messages != NULL)
  {
    *out_messages = result->messages;
  }

  return result;
}