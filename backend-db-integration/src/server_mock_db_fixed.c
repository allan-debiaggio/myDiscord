#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#ifdef USE_DATABASE
#include <libpq-fe.h>
#endif

#define BUFFER_SIZE 4096
#define MAX_CLIENTS 50
#define MAX_USERS 100
#define MAX_CHANNELS 20
#define MAX_MESSAGES 1000
#define MAX_REACTIONS 500
#define MAX_MUTED_USERS 100

// Global database connection
#ifdef USE_DATABASE
PGconn *db_conn = NULL;
bool use_database = false;
#endif

// Forward declarations
void cleanup_server();
void *handle_client(void *arg);

// Trap SIGINT to ensure proper cleanup
void handle_sigint(int sig)
{
  printf("\nCaught signal %d, cleaning up and exiting...\n", sig);
  cleanup_server();
  exit(0);
}

// Data structures
typedef struct
{
  int id;
  int user_id;
  int channel_id;
  char content[BUFFER_SIZE];
  time_t timestamp;
  bool is_deleted;
} Message;

typedef struct
{
  int message_id;
  int user_id;
  char emoji[8];
} Reaction;

typedef struct
{
  int user_id;
  int channel_id;
  time_t end_time;
} MutedUser;

typedef struct
{
  int id;
  int socket;
  char username[64];
  bool is_online;
  char current_channel[32];
  int role; // 0: guest, 1: member, 2: moderator, 3: admin
} User;

typedef struct
{
  char name[32];
  char type[10]; // "public" or "private"
  int creator_id;
} Channel;

// Linked list for clients
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

// Helper function to send system messages to a client
void send_system_message(int client_socket, const char *message)
{
  send(client_socket, message, strlen(message), 0);
}

// Database functions
#ifdef USE_DATABASE
bool db_connect()
{
  printf("[LOG] db_connect: Connecting to PostgreSQL database...\n");
  fflush(stdout);

  const char *conninfo = "dbname=mydiscord30 user=postgres password=postgres host=localhost";

  db_conn = PQconnectdb(conninfo);

  if (PQstatus(db_conn) != CONNECTION_OK)
  {
    fprintf(stderr, "[ERROR] db_connect: Connection to database failed: %s\n", PQerrorMessage(db_conn));
    PQfinish(db_conn);
    db_conn = NULL;
    return false;
  }

  printf("[LOG] Connected to database successfully\n");
  fflush(stdout);
  return true;
}

void db_disconnect()
{
  if (db_conn)
  {
    printf("[LOG] db_disconnect: Disconnecting from database...\n");
    fflush(stdout);
    PQfinish(db_conn);
    db_conn = NULL;
    printf("[LOG] Disconnected from database\n");
    fflush(stdout);
  }
}

bool db_init()
{
  printf("[LOG] db_init: Initializing database tables...\n");
  fflush(stdout);

  if (!db_conn)
  {
    printf("[ERROR] db_init: No database connection\n");
    return false;
  }

  PGresult *res;

  // Create users table
  printf("[LOG] db_init: Creating users table...\n");
  fflush(stdout);

  res = PQexec(db_conn,
               "CREATE TABLE IF NOT EXISTS users ("
               "id SERIAL PRIMARY KEY,"
               "username VARCHAR(64) UNIQUE NOT NULL,"
               "password VARCHAR(64) NOT NULL,"
               "role INTEGER NOT NULL DEFAULT 1,"
               "is_online BOOLEAN DEFAULT FALSE,"
               "current_channel VARCHAR(32) DEFAULT 'general'"
               ")");

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "[ERROR] db_init: Failed to create users table: %s\n", PQerrorMessage(db_conn));
    PQclear(res);
    return false;
  }
  printf("[LOG] db_init: Users table created or already exists\n");
  fflush(stdout);
  PQclear(res);

  // Create channels table
  printf("[LOG] db_init: Creating channels table...\n");
  fflush(stdout);

  res = PQexec(db_conn,
               "CREATE TABLE IF NOT EXISTS channels ("
               "id SERIAL PRIMARY KEY,"
               "name VARCHAR(32) UNIQUE NOT NULL,"
               "type VARCHAR(16) NOT NULL,"
               "created_by INTEGER,"
               "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
               ")");

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "[ERROR] db_init: Failed to create channels table: %s\n", PQerrorMessage(db_conn));
    PQclear(res);
    return false;
  }
  printf("[LOG] db_init: Channels table created or already exists\n");
  fflush(stdout);
  PQclear(res);

  // Create messages table
  printf("[LOG] db_init: Creating messages table...\n");
  fflush(stdout);

  res = PQexec(db_conn,
               "CREATE TABLE IF NOT EXISTS messages ("
               "id SERIAL PRIMARY KEY,"
               "user_id INTEGER,"
               "channel_id INTEGER,"
               "content TEXT NOT NULL,"
               "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
               ")");

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "[ERROR] db_init: Failed to create messages table: %s\n", PQerrorMessage(db_conn));
    PQclear(res);
    return false;
  }
  printf("[LOG] db_init: Messages table created or already exists\n");
  fflush(stdout);
  PQclear(res);

  // Create muted_users table - simple version without foreign keys for debugging
  printf("[LOG] db_init: Creating muted_users table...\n");
  fflush(stdout);

  res = PQexec(db_conn,
               "CREATE TABLE IF NOT EXISTS muted_users ("
               "user_id INTEGER NOT NULL,"
               "channel_id INTEGER NOT NULL,"
               "end_time TIMESTAMP NOT NULL,"
               "PRIMARY KEY(user_id, channel_id)"
               ")");

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "[ERROR] db_init: Failed to create muted_users table: %s\n", PQerrorMessage(db_conn));
    PQclear(res);
    return false;
  }
  printf("[LOG] db_init: Muted_users table created or already exists\n");
  fflush(stdout);
  PQclear(res);

  // Create default general channel if it doesn't exist
  printf("[LOG] db_init: Ensuring general channel exists...\n");
  fflush(stdout);

  res = PQexec(db_conn,
               "INSERT INTO channels (name, type, created_by) "
               "VALUES ('general', 'public', 1) "
               "ON CONFLICT (name) DO NOTHING");

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "[ERROR] db_init: Failed to create general channel: %s\n", PQerrorMessage(db_conn));
    PQclear(res);
    // Continue anyway - channel might already exist
  }
  else
  {
    printf("[LOG] db_init: General channel created or already exists\n");
    fflush(stdout);
  }
  PQclear(res);

  // Create test users if needed
  printf("[LOG] db_init: Creating test users if they don't exist...\n");
  fflush(stdout);

  const char *test_users[] = {"admin", "mod", "user1", "user2"};
  const int roles[] = {3, 2, 1, 1}; // Admin, Mod, Member, Member

  for (int i = 0; i < 4; i++)
  {
    const char *paramValues[3];
    paramValues[0] = test_users[i];
    paramValues[1] = "password"; // Simple password for testing

    char role_str[2];
    sprintf(role_str, "%d", roles[i]);
    paramValues[2] = role_str;

    res = PQexecParams(db_conn,
                       "INSERT INTO users (username, password, role) "
                       "VALUES ($1, $2, $3) "
                       "ON CONFLICT (username) DO NOTHING",
                       3, NULL, paramValues, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
      fprintf(stderr, "[WARN] db_init: Failed to create test user %s: %s\n",
              test_users[i], PQerrorMessage(db_conn));
      // Continue anyway - user might already exist
    }
    PQclear(res);
  }

  printf("[LOG] db_init: Database tables verified successfully\n");
  fflush(stdout);
  return true;
}

// Simplified database functions for testing
int db_add_message(const char *username, const char *channel, const char *content)
{
  if (!db_conn || !use_database)
  {
    return -1;
  }

  // Get user ID
  const char *paramValues[1];
  paramValues[0] = username;

  PGresult *res = PQexecParams(db_conn,
                               "SELECT id FROM users WHERE username = $1",
                               1, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
  {
    fprintf(stderr, "User %s not found in database\n", username);
    PQclear(res);
    return -1;
  }

  int user_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  // Get channel ID
  paramValues[0] = channel;
  res = PQexecParams(db_conn,
                     "SELECT id FROM channels WHERE name = $1",
                     1, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
  {
    fprintf(stderr, "Channel %s not found in database\n", channel);
    PQclear(res);
    return -1;
  }

  int channel_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  // Insert message
  const char *paramValues2[3];
  char user_id_str[16], channel_id_str[16];
  sprintf(user_id_str, "%d", user_id);
  sprintf(channel_id_str, "%d", channel_id);

  paramValues2[0] = user_id_str;
  paramValues2[1] = channel_id_str;
  paramValues2[2] = content;

  res = PQexecParams(db_conn,
                     "INSERT INTO messages (user_id, channel_id, content) VALUES ($1, $2, $3) RETURNING id",
                     3, NULL, paramValues2, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "Failed to add message in DB: %s", PQerrorMessage(db_conn));
    PQclear(res);
    return -1;
  }

  int message_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  return message_id;
}

// Load users from database
void load_users_from_db()
{
  if (!db_conn || !use_database)
  {
    return;
  }

  printf("[LOG] load_users_from_db: Loading users from database...\n");
  fflush(stdout);

  PGresult *res = PQexec(db_conn, "SELECT id, username, role, is_online, current_channel FROM users ORDER BY id LIMIT 100");

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "[ERROR] load_users_from_db: Failed to load users: %s\n", PQerrorMessage(db_conn));
    PQclear(res);
    return;
  }

  int rows = PQntuples(res);
  printf("[LOG] load_users_from_db: Found %d users in database\n", rows);
  fflush(stdout);

  pthread_mutex_lock(&users_mutex);
  user_count = 0;

  for (int i = 0; i < rows && i < MAX_USERS; i++)
  {
    users[user_count].id = atoi(PQgetvalue(res, i, 0));
    strncpy(users[user_count].username, PQgetvalue(res, i, 1), sizeof(users[user_count].username) - 1);
    users[user_count].username[sizeof(users[user_count].username) - 1] = '\0';
    users[user_count].role = atoi(PQgetvalue(res, i, 2));
    users[user_count].is_online = (strcmp(PQgetvalue(res, i, 3), "t") == 0);
    strncpy(users[user_count].current_channel, PQgetvalue(res, i, 4), sizeof(users[user_count].current_channel) - 1);
    users[user_count].current_channel[sizeof(users[user_count].current_channel) - 1] = '\0';

    printf("[LOG] Loaded user: %s (ID: %d, Role: %d)\n",
           users[user_count].username,
           users[user_count].id,
           users[user_count].role);
    fflush(stdout);

    user_count++;
  }

  pthread_mutex_unlock(&users_mutex);
  PQclear(res);

  printf("[LOG] Loaded %d users from database\n", user_count);
  fflush(stdout);
}

// Load channels from database
void load_channels_from_db()
{
  if (!db_conn || !use_database)
  {
    return;
  }

  printf("[LOG] load_channels_from_db: Loading channels from database...\n");
  fflush(stdout);

  PGresult *res = PQexec(db_conn, "SELECT name, type, created_by FROM channels ORDER BY id LIMIT 20");

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "[ERROR] load_channels_from_db: Failed to load channels: %s\n", PQerrorMessage(db_conn));
    PQclear(res);
    return;
  }

  int rows = PQntuples(res);
  printf("[LOG] load_channels_from_db: Found %d channels in database\n", rows);
  fflush(stdout);

  pthread_mutex_lock(&channels_mutex);
  channel_count = 0;

  for (int i = 0; i < rows && i < MAX_CHANNELS; i++)
  {
    strncpy(channels[channel_count].name, PQgetvalue(res, i, 0), sizeof(channels[channel_count].name) - 1);
    channels[channel_count].name[sizeof(channels[channel_count].name) - 1] = '\0';

    strncpy(channels[channel_count].type, PQgetvalue(res, i, 1), sizeof(channels[channel_count].type) - 1);
    channels[channel_count].type[sizeof(channels[channel_count].type) - 1] = '\0';

    channels[channel_count].creator_id = atoi(PQgetvalue(res, i, 2));

    printf("[LOG] Loaded channel: %s (Type: %s, Creator: %d)\n",
           channels[channel_count].name,
           channels[channel_count].type,
           channels[channel_count].creator_id);
    fflush(stdout);

    channel_count++;
  }

  pthread_mutex_unlock(&channels_mutex);
  PQclear(res);

  printf("[LOG] Loaded %d channels from database\n", channel_count);
  fflush(stdout);
}

bool authenticate_user(const char *username, const char *password, int *role)
{
  if (!db_conn || !use_database)
  {
    // Fallback to in-memory authentication
    for (int i = 0; i < user_count; i++)
    {
      if (strcmp(users[i].username, username) == 0)
      {
        // For simplicity, accept any password in non-DB mode
        *role = users[i].role;
        return true;
      }
    }
    return false;
  }

  const char *paramValues[2];
  paramValues[0] = username;
  paramValues[1] = password;

  PGresult *res = PQexecParams(db_conn,
                               "SELECT id, role FROM users WHERE username = $1 AND password = $2",
                               2, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "Authentication query failed: %s\n", PQerrorMessage(db_conn));
    PQclear(res);
    return false;
  }

  if (PQntuples(res) == 0)
  {
    PQclear(res);
    return false;
  }

  *role = atoi(PQgetvalue(res, 0, 1));
  PQclear(res);
  return true;
}

// Add this function to handle mute and unmute commands
bool db_mute_user(const char *username, const char *channel, int duration, bool is_mute)
{
  if (!db_conn || !use_database)
  {
    return false;
  }

  printf("[LOG] db_mute_user: %s user %s in channel %s (duration: %d)\n",
         is_mute ? "Muting" : "Unmuting", username, channel, duration);
  fflush(stdout);

  // Get user ID
  const char *paramValues[1];
  paramValues[0] = username;

  PGresult *res = PQexecParams(db_conn,
                               "SELECT id FROM users WHERE username = $1",
                               1, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
  {
    fprintf(stderr, "[ERROR] User %s not found in database\n", username);
    PQclear(res);
    return false;
  }

  int user_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  // Get channel ID
  paramValues[0] = channel;
  res = PQexecParams(db_conn,
                     "SELECT id FROM channels WHERE name = $1",
                     1, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
  {
    fprintf(stderr, "[ERROR] Channel %s not found in database\n", channel);
    PQclear(res);
    return false;
  }

  int channel_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  if (is_mute)
  {
    // Mute the user
    const char *paramValues2[3];
    char user_id_str[16], channel_id_str[16], duration_str[16];
    sprintf(user_id_str, "%d", user_id);
    sprintf(channel_id_str, "%d", channel_id);
    sprintf(duration_str, "%d", duration);

    paramValues2[0] = user_id_str;
    paramValues2[1] = channel_id_str;
    paramValues2[2] = duration_str;

    // Fix: Use SQL directly to calculate the timestamp in the server
    res = PQexecParams(db_conn,
                       "INSERT INTO muted_users (user_id, channel_id, end_time) "
                       "VALUES ($1, $2, NOW() + ($3 || ' minutes')::interval) "
                       "ON CONFLICT (user_id, channel_id) DO UPDATE "
                       "SET end_time = NOW() + ($3 || ' minutes')::interval",
                       3, NULL, paramValues2, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
      fprintf(stderr, "[ERROR] Failed to mute user in DB: %s\n", PQerrorMessage(db_conn));
      PQclear(res);
      return false;
    }
    PQclear(res);
    return true;
  }
  else
  {
    // Unmute the user
    const char *paramValues2[2];
    char user_id_str[16], channel_id_str[16];
    sprintf(user_id_str, "%d", user_id);
    sprintf(channel_id_str, "%d", channel_id);

    paramValues2[0] = user_id_str;
    paramValues2[1] = channel_id_str;

    res = PQexecParams(db_conn,
                       "DELETE FROM muted_users WHERE user_id = $1 AND channel_id = $2",
                       2, NULL, paramValues2, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
      fprintf(stderr, "[ERROR] Failed to unmute user in DB: %s\n", PQerrorMessage(db_conn));
      PQclear(res);
      return false;
    }
    PQclear(res);
    return true;
  }
}

// Function to list muted users in a channel
char *db_list_muted_users(const char *channel)
{
  if (!db_conn || !use_database)
  {
    return strdup("Database not connected");
  }

  printf("[LOG] db_list_muted_users: Listing muted users in channel %s\n", channel);
  fflush(stdout);

  // Get channel ID
  const char *paramValues[1];
  paramValues[0] = channel;

  PGresult *res = PQexecParams(db_conn,
                               "SELECT id FROM channels WHERE name = $1",
                               1, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
  {
    fprintf(stderr, "[ERROR] Channel %s not found in database\n", channel);
    PQclear(res);
    return strdup("Channel not found");
  }

  int channel_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  // Get muted users
  char channel_id_str[16];
  sprintf(channel_id_str, "%d", channel_id);
  paramValues[0] = channel_id_str;

  res = PQexecParams(db_conn,
                     "SELECT u.username, m.end_time FROM muted_users m "
                     "JOIN users u ON m.user_id = u.id "
                     "WHERE m.channel_id = $1 AND m.end_time > NOW()",
                     1, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "[ERROR] Failed to query muted users: %s\n", PQerrorMessage(db_conn));
    PQclear(res);
    return strdup("Error querying muted users");
  }

  int rows = PQntuples(res);

  // Prepare response
  char *response;
  if (rows == 0)
  {
    response = strdup("No muted users in this channel");
  }
  else
  {
    // Allocate generous buffer for response
    response = (char *)malloc(1024);
    sprintf(response, "Muted users in channel %s:\n", channel);

    for (int i = 0; i < rows; i++)
    {
      char *username = PQgetvalue(res, i, 0);
      char *end_time = PQgetvalue(res, i, 1);

      char entry[256];
      sprintf(entry, "%d. %s (until %s)\n", i + 1, username, end_time);
      strcat(response, entry);
    }
  }

  PQclear(res);
  return response;
}

// Function to create a new channel (admin only)
bool db_create_channel(const char *name, const char *type, int creator_id)
{
  if (!db_conn || !use_database)
  {
    return false;
  }

  printf("[LOG] db_create_channel: Creating %s channel '%s' by user ID %d\n",
         type, name, creator_id);
  fflush(stdout);

  const char *paramValues[3];
  paramValues[0] = name;
  paramValues[1] = type;

  char creator_id_str[16];
  sprintf(creator_id_str, "%d", creator_id);
  paramValues[2] = creator_id_str;

  PGresult *res = PQexecParams(db_conn,
                               "INSERT INTO channels (name, type, created_by) "
                               "VALUES ($1, $2, $3) "
                               "ON CONFLICT (name) DO NOTHING RETURNING id",
                               3, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "[ERROR] Failed to create channel: %s\n", PQerrorMessage(db_conn));
    PQclear(res);
    return false;
  }

  int rows = PQntuples(res);
  bool success = (rows > 0);

  if (success)
  {
    printf("[LOG] Channel '%s' created successfully\n", name);
  }
  else
  {
    printf("[LOG] Channel '%s' already exists\n", name);
  }

  PQclear(res);

  // Reload channels from database
  load_channels_from_db();

  return success;
}

// Function to delete a channel (admin only)
bool db_delete_channel(const char *name)
{
  if (!db_conn || !use_database)
  {
    return false;
  }

  printf("[LOG] db_delete_channel: Deleting channel '%s'\n", name);
  fflush(stdout);

  // Don't allow deleting the general channel
  if (strcmp(name, "general") == 0)
  {
    printf("[ERROR] Cannot delete the general channel\n");
    return false;
  }

  const char *paramValues[1];
  paramValues[0] = name;

  PGresult *res = PQexecParams(db_conn,
                               "DELETE FROM channels WHERE name = $1 RETURNING id",
                               1, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "[ERROR] Failed to delete channel: %s\n", PQerrorMessage(db_conn));
    PQclear(res);
    return false;
  }

  int rows = PQntuples(res);
  bool success = (rows > 0);

  if (success)
  {
    printf("[LOG] Channel '%s' deleted successfully\n", name);
  }
  else
  {
    printf("[LOG] Channel '%s' not found\n", name);
  }

  PQclear(res);

  // Reload channels from database
  load_channels_from_db();

  return success;
}

// Function to set a user's role (admin only)
bool db_set_user_role(const char *username, int role)
{
  if (!db_conn || !use_database)
  {
    return false;
  }

  if (role < 0 || role > 3)
  {
    printf("[ERROR] Invalid role: %d (must be 0-3)\n", role);
    return false;
  }

  printf("[LOG] db_set_user_role: Setting user '%s' to role %d\n", username, role);
  fflush(stdout);

  const char *paramValues[2];
  paramValues[0] = username;

  char role_str[2];
  sprintf(role_str, "%d", role);
  paramValues[1] = role_str;

  PGresult *res = PQexecParams(db_conn,
                               "UPDATE users SET role = $2 WHERE username = $1 RETURNING id",
                               2, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "[ERROR] Failed to set user role: %s\n", PQerrorMessage(db_conn));
    PQclear(res);
    return false;
  }

  int rows = PQntuples(res);
  bool success = (rows > 0);

  if (success)
  {
    printf("[LOG] User '%s' role set to %d successfully\n", username, role);
  }
  else
  {
    printf("[LOG] User '%s' not found\n", username);
  }

  PQclear(res);

  // Reload users from database
  load_users_from_db();

  return success;
}

// Function to list all channels
char *db_list_channels()
{
  if (!db_conn || !use_database)
  {
    return strdup("Database not connected");
  }

  printf("[LOG] db_list_channels: Listing all channels\n");
  fflush(stdout);

  PGresult *res = PQexec(db_conn,
                         "SELECT name, type FROM channels ORDER BY name");

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "[ERROR] Failed to query channels: %s\n", PQerrorMessage(db_conn));
    PQclear(res);
    return strdup("Error querying channels");
  }

  int rows = PQntuples(res);

  // Prepare response
  char *response;
  if (rows == 0)
  {
    response = strdup("No channels found");
  }
  else
  {
    // Allocate generous buffer for response
    response = (char *)malloc(2048);
    sprintf(response, "Available channels (%d):\n", rows);

    for (int i = 0; i < rows; i++)
    {
      char *name = PQgetvalue(res, i, 0);
      char *type = PQgetvalue(res, i, 1);

      char entry[128];
      sprintf(entry, "%d. %s (%s)\n", i + 1, name, type);
      strcat(response, entry);
    }
  }

  PQclear(res);
  return response;
}

// Function to deliver a private message
bool db_send_private_message(const char *sender, const char *recipient, const char *message)
{
  if (!db_conn || !use_database)
  {
    return false;
  }

  printf("[LOG] db_send_private_message: From '%s' to '%s'\n", sender, recipient);
  fflush(stdout);

  // First, verify both users exist
  int sender_id = -1, recipient_id = -1;

  // Get sender ID
  const char *paramValues[1];
  paramValues[0] = sender;

  PGresult *res = PQexecParams(db_conn,
                               "SELECT id FROM users WHERE username = $1",
                               1, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
  {
    fprintf(stderr, "[ERROR] Sender '%s' not found\n", sender);
    PQclear(res);
    return false;
  }

  sender_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  // Get recipient ID
  paramValues[0] = recipient;

  res = PQexecParams(db_conn,
                     "SELECT id FROM users WHERE username = $1",
                     1, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
  {
    fprintf(stderr, "[ERROR] Recipient '%s' not found\n", recipient);
    PQclear(res);
    return false;
  }

  recipient_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  // Create dedicated private_messages table if it doesn't exist
  res = PQexec(db_conn,
               "CREATE TABLE IF NOT EXISTS private_messages ("
               "id SERIAL PRIMARY KEY,"
               "sender_id INTEGER NOT NULL,"
               "recipient_id INTEGER NOT NULL,"
               "content TEXT NOT NULL,"
               "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
               ")");

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "[ERROR] Failed to create private_messages table: %s\n", PQerrorMessage(db_conn));
    PQclear(res);
    return false;
  }
  PQclear(res);

  // Store the private message in the dedicated table
  const char *paramValues2[3];
  char sender_id_str[16], recipient_id_str[16];
  sprintf(sender_id_str, "%d", sender_id);
  sprintf(recipient_id_str, "%d", recipient_id);

  paramValues2[0] = sender_id_str;
  paramValues2[1] = recipient_id_str;
  paramValues2[2] = message;

  res = PQexecParams(db_conn,
                     "INSERT INTO private_messages (sender_id, recipient_id, content) "
                     "VALUES ($1, $2, $3) RETURNING id",
                     3, NULL, paramValues2, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "[ERROR] Failed to save private message: %s\n", PQerrorMessage(db_conn));
    PQclear(res);
    return false;
  }

  int message_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  printf("[LOG] Private message from '%s' to '%s' stored with ID %d\n",
         sender, recipient, message_id);

  return true;
}

bool db_is_user_muted(const char *username, const char *channel)
{
#ifdef USE_DATABASE
  if (!db_conn || !use_database)
  {
    return false;
  }

  printf("[LOG] db_is_user_muted: Checking if user %s is muted in channel %s\n", username, channel);
  fflush(stdout);

  // Get user ID
  const char *paramValues[1];
  paramValues[0] = username;

  PGresult *res = PQexecParams(db_conn,
                               "SELECT id FROM users WHERE username = $1",
                               1, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
  {
    fprintf(stderr, "[ERROR] User %s not found in database\n", username);
    PQclear(res);
    return false;
  }

  int user_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  // Get channel ID
  paramValues[0] = channel;
  res = PQexecParams(db_conn,
                     "SELECT id FROM channels WHERE name = $1",
                     1, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
  {
    fprintf(stderr, "[ERROR] Channel %s not found in database\n", channel);
    PQclear(res);
    return false;
  }

  int channel_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  // Check if user is muted
  const char *paramValues2[2];
  char user_id_str[16], channel_id_str[16];
  sprintf(user_id_str, "%d", user_id);
  sprintf(channel_id_str, "%d", channel_id);

  paramValues2[0] = user_id_str;
  paramValues2[1] = channel_id_str;

  res = PQexecParams(db_conn,
                     "SELECT 1 FROM muted_users "
                     "WHERE user_id = $1 AND channel_id = $2 AND end_time > NOW()",
                     2, NULL, paramValues2, NULL, NULL, 0);

  bool is_muted = (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0);

  if (is_muted)
  {
    printf("[LOG] User %s is muted in channel %s\n", username, channel);
  }

  PQclear(res);
  return is_muted;
#else
  return false;
#endif
}

// Client list utility functions
void add_client(int socket, const char *username)
{
  pthread_mutex_lock(&clients_mutex);

  client_node *new_node = (client_node *)malloc(sizeof(client_node));
  new_node->socket = socket;
  strncpy(new_node->username, username, sizeof(new_node->username) - 1);
  new_node->username[sizeof(new_node->username) - 1] = '\0';

  strncpy(new_node->current_channel, "general", sizeof(new_node->current_channel) - 1);
  new_node->current_channel[sizeof(new_node->current_channel) - 1] = '\0';

  new_node->next = clients_head;
  clients_head = new_node;

  pthread_mutex_unlock(&clients_mutex);
  printf("[LOG] Added client %s (socket: %d)\n", username, socket);
  fflush(stdout);
}

void cleanup_server()
{
  printf("[LOG] Cleaning up server resources...\n");
  fflush(stdout);

  // Free client list
  client_node *current = clients_head;
  client_node *temp;

  while (current != NULL)
  {
    close(current->socket);
    temp = current;
    current = current->next;
    free(temp);
  }

  clients_head = NULL;

#ifdef USE_DATABASE
  // Close database connection
  db_disconnect();
#endif

  printf("[LOG] Server cleanup complete\n");
  fflush(stdout);
}

// Very simple server main function for testing
int main(int argc, char *argv[])
{
  // Set up signal handlers
  signal(SIGINT, handle_sigint);
  signal(SIGTERM, handle_sigint);

  // Default settings
  bool use_db = false;
  int port = 8080;

  // Parse command line arguments
  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "--use-database") == 0)
    {
      use_db = true;
      printf("Database integration enabled\n");
      fflush(stdout);
    }
    else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc)
    {
      port = atoi(argv[i + 1]);
      printf("Using port %d\n", port);
      fflush(stdout);
      i++; // Skip the port value
    }
  }

  printf("[LOG] Starting server on port %d...\n", port);
  printf("[LOG] Database integration: %s\n", use_db ? "ENABLED" : "DISABLED");
  fflush(stdout);

#ifdef USE_DATABASE
  // Initialize the database if needed
  if (use_db)
  {
    use_database = true;
    printf("[LOG] Starting server with database integration...\n");
    fflush(stdout);

    // Connect to database
    if (!db_connect())
    {
      fprintf(stderr, "[ERROR] Failed to connect to database, exiting\n");
      fflush(stderr);
      return 1;
    }

    // Initialize database tables
    if (!db_init())
    {
      fprintf(stderr, "[ERROR] Failed to initialize database tables, exiting\n");
      fflush(stderr);
      db_disconnect();
      return 1;
    }

    printf("[LOG] Database connection successful and tables verified\n");
    fflush(stdout);

    // Load existing data from database
    load_users_from_db();
    load_channels_from_db();
  }
#endif

  // Create a default general channel if needed
  pthread_mutex_lock(&channels_mutex);
  if (channel_count == 0)
  {
    strncpy(channels[0].name, "general", sizeof(channels[0].name) - 1);
    strncpy(channels[0].type, "public", sizeof(channels[0].type) - 1);
    channels[0].creator_id = 0; // System
    channel_count++;
    printf("[LOG] Created default general channel\n");
    fflush(stdout);
  }
  pthread_mutex_unlock(&channels_mutex);

  // Create socket
  int server_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (server_sock < 0)
  {
    perror("Failed to create socket");
    cleanup_server();
    return 1;
  }

  // Set socket options
  int opt = 1;
  if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
  {
    perror("setsockopt failed");
    close(server_sock);
    cleanup_server();
    return 1;
  }

  // Prepare sockaddr_in structure
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  // Bind
  if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    perror("Bind failed");
    close(server_sock);
    cleanup_server();
    return 1;
  }

  // Listen
  if (listen(server_sock, 5) < 0)
  {
    perror("Listen failed");
    close(server_sock);
    cleanup_server();
    return 1;
  }

  printf("[LOG] Server initialized successfully. Listening on port %d\n", port);
  printf("[LOG] Type Ctrl+C to stop the server\n");
  fflush(stdout);

  // Basic functionality test
  printf("\n[TEST] Running basic functionality test...\n");

#ifdef USE_DATABASE
  if (use_database)
  {
    printf("[TEST] Database connected and tables initialized\n");

    // Test adding a message
    int msg_id = db_add_message("admin", "general", "Test message from startup");
    if (msg_id >= 0)
    {
      printf("[TEST] Successfully stored test message (ID: %d)\n", msg_id);
    }
    else
    {
      printf("[TEST] Failed to store test message\n");
    }
  }
#endif

  printf("[LOG] Server ready and waiting for connections\n");
  fflush(stdout);

  // Main server loop
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);
  int client_sock;

  while ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len)))
  {
    printf("[LOG] New connection accepted\n");
    fflush(stdout);

    // Create new thread to handle client
    pthread_t thread_id;
    int *new_sock = malloc(sizeof(int));
    *new_sock = client_sock;

    if (pthread_create(&thread_id, NULL, handle_client, (void *)new_sock) < 0)
    {
      perror("Thread creation failed");
      free(new_sock);
      continue;
    }

    // Detach the thread
    pthread_detach(thread_id);
  }

  close(server_sock);
  cleanup_server();
  return 0;
}

// Handle client connection thread function - placeholder
void *handle_client(void *arg)
{
  int client_socket = *((int *)arg);
  free(arg);

  char buffer[BUFFER_SIZE];
  int recv_size;

  // Send welcome message
  send(client_socket, "Welcome to MyDiscord Fixed Server!\n", 34, 0);

  // Receive messages in a loop
  while ((recv_size = recv(client_socket, buffer, sizeof(buffer) - 1, 0)) > 0)
  {
    buffer[recv_size] = '\0';
    printf("[LOG] Received message from client: %s\n", buffer);
    fflush(stdout);

    // Check if this is a login request
    if (strncmp(buffer, "LOGIN:", 6) == 0)
    {
      char username[64] = {0};
      char password[64] = {0};

      // Parse LOGIN:username:password
      sscanf(buffer + 6, "%63[^:]:%63s", username, password);

      int role = 0;
#ifdef USE_DATABASE
      bool auth_success = authenticate_user(username, password, &role);
#else
      bool auth_success = true;
      role = 1;
#endif

      if (auth_success)
      {
        printf("[LOG] User %s authenticated successfully with role %d\n", username, role);
        fflush(stdout);

        add_client(client_socket, username);

        char response[256];
        sprintf(response, "LOGIN_SUCCESS\nROLE_UPDATE:%d", role);
        send(client_socket, response, strlen(response), 0);
      }
      else
      {
        printf("[LOG] Authentication failed for user %s\n", username);
        fflush(stdout);

        send(client_socket, "LOGIN_FAILED", 12, 0);
      }
      continue;
    }

    // Process other messages
#ifdef USE_DATABASE
    if (use_database && strncmp(buffer, "MSG:", 4) == 0)
    {
      char channel[32] = {0};
      char message[BUFFER_SIZE] = {0};
      char *username = "unknown";

      // Find client to get username
      pthread_mutex_lock(&clients_mutex);
      client_node *current = clients_head;
      while (current != NULL)
      {
        if (current->socket == client_socket)
        {
          username = current->username;
          break;
        }
        current = current->next;
      }
      pthread_mutex_unlock(&clients_mutex);

      // Parse MSG:channel:message
      sscanf(buffer + 4, "%31[^:]:%[^\n]", channel, message);

      // Check if user is muted before allowing them to send a message
      if (db_is_user_muted(username, channel))
      {
        char response[BUFFER_SIZE];
        sprintf(response, "You are currently muted in channel %s and cannot send messages", channel);
        send(client_socket, response, strlen(response), 0);
        continue;
      }

      int msg_id = db_add_message(username, channel, message);
      if (msg_id >= 0)
      {
        char response[BUFFER_SIZE];
        sprintf(response, "Successfully stored message (ID: %d): %s", msg_id, message);
        send(client_socket, response, strlen(response), 0);

        // Broadcast the message to all clients in this channel
        char broadcast_message[BUFFER_SIZE];
        sprintf(broadcast_message, "[%s@%s]: %s", username, channel, message);

        pthread_mutex_lock(&clients_mutex);
        client_node *broadcast_target = clients_head;
        while (broadcast_target != NULL)
        {
          // Send to all clients in the same channel except the sender
          if (broadcast_target->socket != client_socket &&
              strcmp(broadcast_target->current_channel, channel) == 0)
          {
            send(broadcast_target->socket, broadcast_message, strlen(broadcast_message), 0);
          }
          broadcast_target = broadcast_target->next;
        }
        pthread_mutex_unlock(&clients_mutex);

        printf("[LOG] Broadcasted message from %s to channel %s\n", username, channel);
        fflush(stdout);
      }
      else
      {
        send(client_socket, "Failed to store message", 23, 0);
      }
      continue;
    }

    // Handle mute command
    if (use_database && strncmp(buffer, "MUTE:", 5) == 0)
    {
      char username[64] = {0};
      char channel[32] = {0};
      int duration = 10; // default 10 minutes

      // Parse MUTE:username:channel:duration
      sscanf(buffer + 5, "%63[^:]:%31[^:]:%d", username, channel, &duration);

      bool success = db_mute_user(username, channel, duration, true);
      if (success)
      {
        char response[256];
        sprintf(response, "User %s has been muted in channel %s for %d minutes",
                username, channel, duration);
        send(client_socket, response, strlen(response), 0);
      }
      else
      {
        char response[256];
        sprintf(response, "Failed to mute user %s in channel %s", username, channel);
        send(client_socket, response, strlen(response), 0);
      }
      continue;
    }

    // Handle unmute command
    if (use_database && strncmp(buffer, "UNMUTE:", 7) == 0)
    {
      char username[64] = {0};
      char channel[32] = {0};

      // Parse UNMUTE:username:channel
      sscanf(buffer + 7, "%63[^:]:%31[^\n]", username, channel);

      bool success = db_mute_user(username, channel, 0, false);
      if (success)
      {
        char response[256];
        sprintf(response, "User %s has been unmuted in channel %s",
                username, channel);
        send(client_socket, response, strlen(response), 0);
      }
      else
      {
        char response[256];
        sprintf(response, "Failed to unmute user %s in channel %s", username, channel);
        send(client_socket, response, strlen(response), 0);
      }
      continue;
    }

    // Handle list muted users command
    if (use_database && strncmp(buffer, "LIST_MUTED:", 11) == 0)
    {
      char channel[32] = {0};

      // Parse LIST_MUTED:channel
      sscanf(buffer + 11, "%31[^\n]", channel);

      char *muted_users = db_list_muted_users(channel);
      send(client_socket, muted_users, strlen(muted_users), 0);
      free(muted_users);
      continue;
    }
#endif

    // Handle private message
    if (use_database && strncmp(buffer, "PRIVMSG:", 8) == 0)
    {
      char recipient[64] = {0};
      char message[BUFFER_SIZE] = {0};
      char *sender = "unknown";

      // Find client to get sender's username
      pthread_mutex_lock(&clients_mutex);
      client_node *current = clients_head;
      while (current != NULL)
      {
        if (current->socket == client_socket)
        {
          sender = current->username;
          break;
        }
        current = current->next;
      }
      pthread_mutex_unlock(&clients_mutex);

      // Parse PRIVMSG:recipient:message
      sscanf(buffer + 8, "%63[^:]:%[^\n]", recipient, message);

      bool success = db_send_private_message(sender, recipient, message);
      if (success)
      {
        char response[BUFFER_SIZE];
        sprintf(response, "Private message sent to %s", recipient);
        send(client_socket, response, strlen(response), 0);

        // Find the recipient's socket and forward the message
        int recipient_socket = -1;
        pthread_mutex_lock(&clients_mutex);
        current = clients_head;
        while (current != NULL)
        {
          if (strcmp(current->username, recipient) == 0)
          {
            recipient_socket = current->socket;
            break;
          }
          current = current->next;
        }
        pthread_mutex_unlock(&clients_mutex);

        if (recipient_socket != -1)
        {
          char pm_notification[BUFFER_SIZE];
          sprintf(pm_notification, "[Private from %s]: %s", sender, message);
          send(recipient_socket, pm_notification, strlen(pm_notification), 0);
          printf("[LOG] Delivered private message from %s to %s\n", sender, recipient);
          fflush(stdout);
        }
        else
        {
          printf("[LOG] Recipient %s is offline, message stored only\n", recipient);
          fflush(stdout);
        }
      }
      else
      {
        char response[256];
        sprintf(response, "Failed to send private message to %s", recipient);
        send(client_socket, response, strlen(response), 0);
      }
      continue;
    }

    // Handle channel switching
    if (strncmp(buffer, "CHANNEL:", 8) == 0)
    {
      char new_channel[32] = {0};
      char *username = "unknown";

      // Parse CHANNEL:channel_name and handle potential newlines
      char *newline = strchr(buffer + 8, '\n');
      if (newline)
        *newline = '\0'; // Terminate at newline if present

      sscanf(buffer + 8, "%31s", new_channel);

      // Find the client to update their channel
      pthread_mutex_lock(&clients_mutex);
      client_node *current = clients_head;
      while (current != NULL)
      {
        if (current->socket == client_socket)
        {
          username = current->username;
          // Update the client's current channel
          strncpy(current->current_channel, new_channel, sizeof(current->current_channel) - 1);
          current->current_channel[sizeof(current->current_channel) - 1] = '\0';
          break;
        }
        current = current->next;
      }
      pthread_mutex_unlock(&clients_mutex);

      printf("[LOG] User %s switched to channel %s\n", username, new_channel);
      fflush(stdout);

      // Confirm channel switch to client
      char response[256];
      sprintf(response, "Switched to channel: %s", new_channel);
      send(client_socket, response, strlen(response), 0);

      continue;
    }

    // Handle create channel
    if (use_database && strncmp(buffer, "CREATE_CHANNEL:", 15) == 0)
    {
      char name[32] = {0};
      char type[10] = "public";
      int creator_id = 0;
      char *username = "unknown";

      // Find client to get username
      pthread_mutex_lock(&clients_mutex);
      client_node *current = clients_head;
      while (current != NULL)
      {
        if (current->socket == client_socket)
        {
          username = current->username;
          break;
        }
        current = current->next;
      }
      pthread_mutex_unlock(&clients_mutex);

      // Get creator_id from username
      for (int i = 0; i < user_count; i++)
      {
        if (strcmp(users[i].username, username) == 0)
        {
          creator_id = users[i].id;
          break;
        }
      }

      // Parse CREATE_CHANNEL:name:type
      sscanf(buffer + 15, "%31[^:]:%9s", name, type);

      // Check if user has admin rights
      bool is_admin = false;
      for (int i = 0; i < user_count; i++)
      {
        if (strcmp(users[i].username, username) == 0 && users[i].role == 3)
        {
          is_admin = true;
          break;
        }
      }

      if (!is_admin)
      {
        char response[256] = "ERROR: Only admins can create channels";
        send(client_socket, response, strlen(response), 0);
      }
      else
      {
        bool success = db_create_channel(name, type, creator_id);
        if (success)
        {
          char response[256];
          sprintf(response, "Channel '%s' created successfully", name);
          send(client_socket, response, strlen(response), 0);
        }
        else
        {
          char response[256];
          sprintf(response, "Failed to create channel '%s'", name);
          send(client_socket, response, strlen(response), 0);
        }
      }
      continue;
    }

    // Handle delete channel
    if (use_database && strncmp(buffer, "DELETE_CHANNEL:", 15) == 0)
    {
      char name[32] = {0};
      char *username = "unknown";

      // Find client to get username
      pthread_mutex_lock(&clients_mutex);
      client_node *current = clients_head;
      while (current != NULL)
      {
        if (current->socket == client_socket)
        {
          username = current->username;
          break;
        }
        current = current->next;
      }
      pthread_mutex_unlock(&clients_mutex);

      // Parse DELETE_CHANNEL:name
      sscanf(buffer + 15, "%31s", name);

      // Check if user has admin rights
      bool is_admin = false;
      for (int i = 0; i < user_count; i++)
      {
        if (strcmp(users[i].username, username) == 0 && users[i].role == 3)
        {
          is_admin = true;
          break;
        }
      }

      if (!is_admin)
      {
        char response[256] = "ERROR: Only admins can delete channels";
        send(client_socket, response, strlen(response), 0);
      }
      else
      {
        bool success = db_delete_channel(name);
        if (success)
        {
          char response[256];
          sprintf(response, "Channel '%s' deleted successfully", name);
          send(client_socket, response, strlen(response), 0);
        }
        else
        {
          char response[256];
          sprintf(response, "Failed to delete channel '%s'", name);
          send(client_socket, response, strlen(response), 0);
        }
      }
      continue;
    }

    // Handle set role
    if (use_database && strncmp(buffer, "SET_ROLE:", 9) == 0)
    {
      char target_username[64] = {0};
      int new_role = 1;
      char *username = "unknown";

      // Find client to get username
      pthread_mutex_lock(&clients_mutex);
      client_node *current = clients_head;
      while (current != NULL)
      {
        if (current->socket == client_socket)
        {
          username = current->username;
          break;
        }
        current = current->next;
      }
      pthread_mutex_unlock(&clients_mutex);

      // Parse SET_ROLE:username:role
      sscanf(buffer + 9, "%63[^:]:%d", target_username, &new_role);

      // Check if user has admin rights
      bool is_admin = false;
      for (int i = 0; i < user_count; i++)
      {
        if (strcmp(users[i].username, username) == 0 && users[i].role == 3)
        {
          is_admin = true;
          break;
        }
      }

      if (!is_admin)
      {
        char response[256] = "ERROR: Only admins can set user roles";
        send(client_socket, response, strlen(response), 0);
      }
      else
      {
        bool success = db_set_user_role(target_username, new_role);
        if (success)
        {
          char response[256];
          sprintf(response, "User '%s' role set to %d successfully", target_username, new_role);
          send(client_socket, response, strlen(response), 0);

          // Find the target user's socket and notify them
          int target_socket = -1;
          pthread_mutex_lock(&clients_mutex);
          current = clients_head;
          while (current != NULL)
          {
            if (strcmp(current->username, target_username) == 0)
            {
              target_socket = current->socket;
              break;
            }
            current = current->next;
          }
          pthread_mutex_unlock(&clients_mutex);

          if (target_socket != -1)
          {
            char role_notification[256];
            sprintf(role_notification, "ROLE_UPDATE:%d", new_role);
            send(target_socket, role_notification, strlen(role_notification), 0);
          }
        }
        else
        {
          char response[256];
          sprintf(response, "Failed to set role for user '%s'", target_username);
          send(client_socket, response, strlen(response), 0);
        }
      }
      continue;
    }

    // Handle list channels
    if (use_database && strcmp(buffer, "LIST_CHANNELS") == 0)
    {
      char *channel_list = db_list_channels();
      send(client_socket, channel_list, strlen(channel_list), 0);
      free(channel_list);
      continue;
    }

    // Echo back message by default
    send(client_socket, buffer, recv_size, 0);
  }

  // Client disconnected
  printf("[LOG] Client disconnected\n");
  fflush(stdout);

  close(client_socket);
  return NULL;
}
#endif
