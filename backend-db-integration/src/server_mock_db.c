#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include <libpq-fe.h>

// Database connection settings
#define DB_HOST "localhost"
#define DB_PORT "5432"
#define DB_NAME "mydiscord30"
#define DB_USER "postgres"
#define DB_PASSWORD "postgres"

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

// Global database connection
PGconn *db_conn = NULL;
bool use_database = false;

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
  int id;
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

// Function prototypes for database operations
int db_find_user_by_name(const char *username);
int db_find_channel_by_name(const char *channel_name);
bool db_mute_user_internal(PGconn *conn, int user_id, int channel_id, int minutes);
bool db_is_user_muted_internal(PGconn *conn, int user_id, int channel_id);
bool db_mute_user(const char *username, const char *channel_name, int minutes);
bool db_is_user_muted(const char *username, const char *channel_name);

// Helper function prototypes
void send_system_message(int client_socket, const char *message);

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

// Helper function to send system messages to a client
void send_system_message(int client_socket, const char *message)
{
  send(client_socket, message, strlen(message), 0);
}

// Database functions
bool db_connect()
{
  printf("[LOG] db_connect: Connecting to PostgreSQL database...\n");

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
  return true;
}

void db_disconnect()
{
  if (db_conn)
  {
    printf("[LOG] db_disconnect: Disconnecting from database...\n");
    PQfinish(db_conn);
    db_conn = NULL;
    printf("[LOG] Disconnected from database\n");
  }
}

bool db_init()
{
  printf("[LOG] db_init: Initializing database tables...\n");

  if (!db_conn)
  {
    printf("[ERROR] db_init: No database connection\n");
    return false;
  }

  PGresult *res;

  // Create users table
  printf("[LOG] db_init: Creating users table...\n");
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
  PQclear(res);

  // Create channels table
  printf("[LOG] db_init: Creating channels table...\n");
  res = PQexec(db_conn,
               "CREATE TABLE IF NOT EXISTS channels ("
               "id SERIAL PRIMARY KEY,"
               "name VARCHAR(32) UNIQUE NOT NULL,"
               "type VARCHAR(16) NOT NULL,"
               "created_by INTEGER REFERENCES users(id),"
               "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
               ")");

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "[ERROR] db_init: Failed to create channels table: %s\n", PQerrorMessage(db_conn));
    PQclear(res);
    return false;
  }
  printf("[LOG] db_init: Channels table created or already exists\n");
  PQclear(res);

  // Create messages table
  printf("[LOG] db_init: Creating messages table...\n");
  res = PQexec(db_conn,
               "CREATE TABLE IF NOT EXISTS messages ("
               "id SERIAL PRIMARY KEY,"
               "user_id INTEGER REFERENCES users(id),"
               "channel_id INTEGER REFERENCES channels(id),"
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
  PQclear(res);

  // Create reactions table
  printf("[LOG] db_init: Creating reactions table...\n");
  res = PQexec(db_conn,
               "CREATE TABLE IF NOT EXISTS reactions ("
               "id SERIAL PRIMARY KEY,"
               "message_id INTEGER REFERENCES messages(id),"
               "user_id INTEGER REFERENCES users(id),"
               "emoji VARCHAR(8) NOT NULL,"
               "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
               ")");

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "[ERROR] db_init: Failed to create reactions table: %s\n", PQerrorMessage(db_conn));
    PQclear(res);
    return false;
  }
  printf("[LOG] db_init: Reactions table created or already exists\n");
  PQclear(res);

  // Create muted_users table - no foreign keys for now to debug
  printf("[LOG] db_init: Creating muted_users table...\n");
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
  PQclear(res);

  // Add foreign keys to muted_users table
  printf("[LOG] db_init: Adding foreign keys to muted_users table...\n");
  res = PQexec(db_conn,
               "ALTER TABLE muted_users "
               "ADD CONSTRAINT fk_muted_user_id FOREIGN KEY (user_id) REFERENCES users(id),"
               "ADD CONSTRAINT fk_muted_channel_id FOREIGN KEY (channel_id) REFERENCES channels(id)");

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "[ERROR] db_init: Failed to add foreign keys to muted_users table: %s\n", PQerrorMessage(db_conn));
    PQclear(res);
    // Don't fail on foreign key constraint error
    printf("[WARN] db_init: Continuing despite foreign key error\n");
  }
  else
  {
    printf("[LOG] db_init: Foreign keys added to muted_users table\n");
  }
  PQclear(res);

  printf("[LOG] db_init: Database tables verified successfully\n");
  return true;
}

bool db_create_user(const char *username, int role)
{
  char query[256];
  snprintf(query, sizeof(query),
           "INSERT INTO users (username, role) VALUES ('%s', %d) ON CONFLICT (username) DO NOTHING",
           username, role);

  PGresult *res = PQexec(db_conn, query);
  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "Failed to create user in DB: %s", PQerrorMessage(db_conn));
    PQclear(res);
    return false;
  }

  PQclear(res);
  return true;
}

bool db_update_user_role(const char *username, int role)
{
  char query[256];
  snprintf(query, sizeof(query),
           "UPDATE users SET role = %d WHERE username = '%s'",
           role, username);

  PGresult *res = PQexec(db_conn, query);
  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "Failed to update user role in DB: %s", PQerrorMessage(db_conn));
    PQclear(res);
    return false;
  }

  PQclear(res);
  return true;
}

bool db_create_channel(const char *name, const char *type, int creator_id)
{
  char query[256];

  if (creator_id >= 0)
  {
    snprintf(query, sizeof(query),
             "INSERT INTO channels (name, type, created_by) VALUES ('%s', '%s', %d) ON CONFLICT (name) DO NOTHING",
             name, type, creator_id);
  }
  else
  {
    snprintf(query, sizeof(query),
             "INSERT INTO channels (name, type) VALUES ('%s', '%s') ON CONFLICT (name) DO NOTHING",
             name, type);
  }

  PGresult *res = PQexec(db_conn, query);
  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "Failed to create channel in DB: %s", PQerrorMessage(db_conn));
    PQclear(res);
    return false;
  }

  PQclear(res);
  return true;
}

bool db_delete_channel(const char *name)
{
  char query[256];
  snprintf(query, sizeof(query),
           "DELETE FROM channels WHERE name = '%s'",
           name);

  PGresult *res = PQexec(db_conn, query);
  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "Failed to delete channel in DB: %s", PQerrorMessage(db_conn));
    PQclear(res);
    return false;
  }

  PQclear(res);
  return true;
}

int db_add_message(const char *username, const char *channel, const char *content)
{
  char query[512];
  snprintf(query, sizeof(query),
           "INSERT INTO messages (user_id, channel_id, content) "
           "SELECT u.id, c.id, '%s' FROM users u, channels c "
           "WHERE u.username = '%s' AND c.name = '%s' "
           "RETURNING id",
           content, username, channel);

  PGresult *res = PQexec(db_conn, query);
  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "Failed to add message in DB: %s", PQerrorMessage(db_conn));
    PQclear(res);
    return -1;
  }

  int message_id = -1;
  if (PQntuples(res) > 0)
  {
    message_id = atoi(PQgetvalue(res, 0, 0));
  }

  PQclear(res);
  return message_id;
}

bool db_delete_message(int message_id)
{
  char query[256];
  snprintf(query, sizeof(query),
           "DELETE FROM messages WHERE id = %d",
           message_id);

  PGresult *res = PQexec(db_conn, query);
  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "Failed to delete message in DB: %s", PQerrorMessage(db_conn));
    PQclear(res);
    return false;
  }

  PQclear(res);
  return true;
}

bool db_add_reaction(const char *username, int message_id, const char *emoji)
{
  if (!db_conn || !use_database)
    return false;

  int user_id = db_find_user_by_name(username);

  if (user_id == -1)
    return false;

  return true; // Placeholder, implement actual functionality to add reaction
}

// Implementation of database mute user internal function
bool db_mute_user_internal(PGconn *conn, int user_id, int channel_id, int minutes)
{
  if (!conn)
    return false;

  time_t now = time(NULL);
  time_t expiry = now + (minutes * 60);

  struct tm *tm_info = localtime(&expiry);
  char expiry_str[20];
  strftime(expiry_str, sizeof(expiry_str), "%Y-%m-%d %H:%M:%S", tm_info);

  const char *paramValues[3];
  char user_id_str[12], channel_id_str[12];

  snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
  snprintf(channel_id_str, sizeof(channel_id_str), "%d", channel_id);

  paramValues[0] = user_id_str;
  paramValues[1] = channel_id_str;
  paramValues[2] = expiry_str;

  PGresult *res = PQexecParams(conn,
                               "INSERT INTO muted_users (user_id, channel_id, end_time) "
                               "VALUES ($1, $2, $3) "
                               "ON CONFLICT (user_id, channel_id) DO UPDATE SET end_time = $3",
                               3, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "Mute user failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    return false;
  }

  PQclear(res);
  return true;
}

// Implementation of database check if user is muted
bool db_is_user_muted_internal(PGconn *conn, int user_id, int channel_id)
{
  if (!conn)
    return false;

  const char *paramValues[2];
  char user_id_str[12], channel_id_str[12];

  snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
  snprintf(channel_id_str, sizeof(channel_id_str), "%d", channel_id);

  paramValues[0] = user_id_str;
  paramValues[1] = channel_id_str;

  PGresult *res = PQexecParams(conn,
                               "SELECT end_time FROM muted_users "
                               "WHERE user_id = $1 AND channel_id = $2 AND end_time > NOW()",
                               2, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "Mute check failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    return false;
  }

  bool is_muted = (PQntuples(res) > 0);
  PQclear(res);

  return is_muted;
}

// User-facing mute user function
bool db_mute_user(const char *username, const char *channel_name, int minutes)
{
  if (!db_conn || !use_database)
    return false;

  int user_id = db_find_user_by_name(username);
  int channel_id = db_find_channel_by_name(channel_name);

  if (user_id == -1 || channel_id == -1)
    return false;

  return db_mute_user_internal(db_conn, user_id, channel_id, minutes);
}

// User-facing check if user is muted function
bool db_is_user_muted(const char *username, const char *channel_name)
{
  if (!db_conn || !use_database)
    return false;

  int user_id = db_find_user_by_name(username);
  int channel_id = db_find_channel_by_name(channel_name);

  if (user_id == -1 || channel_id == -1)
    return false;

  return db_is_user_muted_internal(db_conn, user_id, channel_id);
}

void load_users_from_db()
{
  if (!db_conn || !use_database)
  {
    return;
  }

  printf("[LOG] Loading users from database...\n");

  PGresult *res = PQexec(db_conn, "SELECT id, username, role, is_online, current_channel FROM users ORDER BY id");

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "[ERROR] Failed to load users: %s\n", PQerrorMessage(db_conn));
    PQclear(res);
    return;
  }

  int rows = PQntuples(res);

  pthread_mutex_lock(&users_mutex);

  user_count = 0;

  for (int i = 0; i < rows && i < MAX_USERS; i++)
  {
    int id = atoi(PQgetvalue(res, i, 0));
    const char *username = PQgetvalue(res, i, 1);
    int role = atoi(PQgetvalue(res, i, 2));
    bool is_online = strcmp(PQgetvalue(res, i, 3), "t") == 0;
    const char *current_channel = PQgetvalue(res, i, 4);

    users[user_count].id = id;
    strncpy(users[user_count].username, username, sizeof(users[user_count].username) - 1);
    users[user_count].role = role;
    users[user_count].is_online = is_online;
    strncpy(users[user_count].current_channel, current_channel, sizeof(users[user_count].current_channel) - 1);

    user_count++;
  }

  pthread_mutex_unlock(&users_mutex);

  PQclear(res);

  printf("[LOG] Loaded %d users from database\n", user_count);
}

void load_channels_from_db()
{
  if (!db_conn || !use_database)
  {
    return;
  }

  printf("[LOG] Loading channels from database...\n");

  PGresult *res = PQexec(db_conn, "SELECT name, type, created_by FROM channels ORDER BY id");

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "[ERROR] Failed to load channels: %s\n", PQerrorMessage(db_conn));
    PQclear(res);
    return;
  }

  int rows = PQntuples(res);

  pthread_mutex_lock(&channels_mutex);

  channel_count = 0;

  for (int i = 0; i < rows && i < MAX_CHANNELS; i++)
  {
    const char *name = PQgetvalue(res, i, 0);
    const char *type = PQgetvalue(res, i, 1);
    int creator_id = atoi(PQgetvalue(res, i, 2));

    strncpy(channels[channel_count].name, name, sizeof(channels[channel_count].name) - 1);
    strncpy(channels[channel_count].type, type, sizeof(channels[channel_count].type) - 1);
    channels[channel_count].creator_id = creator_id;

    channel_count++;
  }

  pthread_mutex_unlock(&channels_mutex);

  PQclear(res);

  printf("[LOG] Loaded %d channels from database\n", channel_count);
}

// Client list utility functions
void add_client(int socket, const char *username)
{
  pthread_mutex_lock(&clients_mutex);

  client_node *new_node = (client_node *)malloc(sizeof(client_node));
  new_node->socket = socket;
  strncpy(new_node->username, username, sizeof(new_node->username) - 1);
  new_node->username[sizeof(new_node->username) - 1] = '\0';
  strcpy(new_node->current_channel, "general");
  new_node->next = clients_head;
  clients_head = new_node;

  pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int socket)
{
  pthread_mutex_lock(&clients_mutex);

  client_node *temp = clients_head;
  client_node *prev = NULL;

  if (temp != NULL && temp->socket == socket)
  {
    clients_head = temp->next;
    free(temp);
    pthread_mutex_unlock(&clients_mutex);
    return;
  }

  while (temp != NULL && temp->socket != socket)
  {
    prev = temp;
    temp = temp->next;
  }

  if (temp == NULL)
  {
    pthread_mutex_unlock(&clients_mutex);
    return;
  }

  prev->next = temp->next;
  free(temp);

  pthread_mutex_unlock(&clients_mutex);
}

client_node *find_client_by_socket(int socket)
{
  pthread_mutex_lock(&clients_mutex);
  client_node *current = clients_head;

  while (current != NULL)
  {
    if (current->socket == socket)
    {
      pthread_mutex_unlock(&clients_mutex);
      return current;
    }
    current = current->next;
  }

  pthread_mutex_unlock(&clients_mutex);
  return NULL;
}

client_node *find_client_by_username(const char *username)
{
  pthread_mutex_lock(&clients_mutex);
  client_node *current = clients_head;

  while (current != NULL)
  {
    if (strcmp(current->username, username) == 0)
    {
      pthread_mutex_unlock(&clients_mutex);
      return current;
    }
    current = current->next;
  }

  pthread_mutex_unlock(&clients_mutex);
  return NULL;
}

void set_client_channel(int socket, const char *channel)
{
  pthread_mutex_lock(&clients_mutex);
  client_node *current = clients_head;
  while (current != NULL)
  {
    if (current->socket == socket)
    {
      strncpy(current->current_channel, channel, sizeof(current->current_channel) - 1);
      current->current_channel[sizeof(current->current_channel) - 1] = '\0';

      // Update user's channel in memory
      for (int i = 0; i < user_count; i++)
      {
        if (strcmp(current->username, users[i].username) == 0)
        {
          strncpy(users[i].current_channel, channel, sizeof(users[i].current_channel) - 1);
          users[i].current_channel[sizeof(users[i].current_channel) - 1] = '\0';

          // Update in database if enabled
          if (db_conn && use_database)
          {
            int db_user_id = db_find_user_by_name(users[i].username);
            if (db_user_id > 0)
            {
              const char *paramValues[2];
              char user_id_str[12];

              snprintf(user_id_str, sizeof(user_id_str), "%d", db_user_id);

              paramValues[0] = channel;
              paramValues[1] = user_id_str;

              PGresult *res = PQexecParams(db_conn,
                                           "UPDATE users SET current_channel = $1 WHERE id = $2",
                                           2, NULL, paramValues, NULL, NULL, 0);

              if (PQresultStatus(res) != PGRES_COMMAND_OK)
              {
                fprintf(stderr, "[ERROR] Failed to update user channel in database: %s\n", PQerrorMessage(db_conn));
              }
              else
              {
                printf("[LOG] Updated user %s channel to %s in database\n", users[i].username, channel);
              }

              PQclear(res);
            }
          }

          break;
        }
      }

      break;
    }
    current = current->next;
  }
  pthread_mutex_unlock(&clients_mutex);
}

// User management functions
int find_user_by_name(const char *username)
{
  pthread_mutex_lock(&users_mutex);
  for (int i = 0; i < user_count; i++)
  {
    if (strcmp(users[i].username, username) == 0)
    {
      pthread_mutex_unlock(&users_mutex);
      return i;
    }
  }
  pthread_mutex_unlock(&users_mutex);
  return -1;
}

int get_user_role(const char *username)
{
  int user_index = find_user_by_name(username);
  if (user_index == -1)
  {
    return -1;
  }
  return users[user_index].role;
}

bool update_user_role(const char *username, int new_role)
{
  int user_index = find_user_by_name(username);
  if (user_index == -1)
  {
    return false;
  }

  pthread_mutex_lock(&users_mutex);
  users[user_index].role = new_role;
  pthread_mutex_unlock(&users_mutex);

  // Update in database if enabled
  if (use_database)
  {
    int user_id = db_find_user_by_name(username);
    if (user_id != -1)
    {
      db_update_user_role(username, new_role);
    }
  }

  return true;
}

// Channel management functions
int find_channel_by_name(const char *channel_name)
{
  pthread_mutex_lock(&channels_mutex);
  for (int i = 0; i < channel_count; i++)
  {
    if (strcmp(channels[i].name, channel_name) == 0)
    {
      pthread_mutex_unlock(&channels_mutex);
      return i;
    }
  }
  pthread_mutex_unlock(&channels_mutex);
  return -1;
}

bool create_channel(const char *name, const char *type, int creator_user_index)
{
  // Check if channel already exists
  if (find_channel_by_name(name) != -1)
  {
    return false;
  }

  pthread_mutex_lock(&channels_mutex);
  if (channel_count >= MAX_CHANNELS)
  {
    pthread_mutex_unlock(&channels_mutex);
    return false;
  }

  strncpy(channels[channel_count].name, name, sizeof(channels[channel_count].name) - 1);
  channels[channel_count].name[sizeof(channels[channel_count].name) - 1] = '\0';

  strncpy(channels[channel_count].type, type, sizeof(channels[channel_count].type) - 1);
  channels[channel_count].type[sizeof(channels[channel_count].type) - 1] = '\0';

  channels[channel_count].creator_id = creator_user_index;
  channel_count++;
  pthread_mutex_unlock(&channels_mutex);

  // Store in database if enabled
  if (use_database)
  {
    int db_user_id = -1;
    if (creator_user_index >= 0)
    {
      db_user_id = db_find_user_by_name(users[creator_user_index].username);
    }
    db_create_channel(name, type, db_user_id);
  }

  return true;
}

bool delete_channel(const char *name)
{
  // Don't allow deletion of general channel
  if (strcmp(name, "general") == 0)
  {
    return false;
  }

  int channel_index = find_channel_by_name(name);
  if (channel_index == -1)
  {
    return false;
  }

  pthread_mutex_lock(&channels_mutex);

  for (int i = channel_index; i < channel_count - 1; i++)
  {
    memcpy(&channels[i], &channels[i + 1], sizeof(Channel));
  }

  channel_count--;
  pthread_mutex_unlock(&channels_mutex);

  // Delete from database if enabled
  if (use_database)
  {
    db_delete_channel(name);
  }

  return true;
}

// Message management functions
int add_message(const char *username, const char *channel, const char *content)
{
  printf("[DEBUG] add_message called with: username=%s, channel=%s, content=%s\n",
         username, channel, content);

  pthread_mutex_lock(&messages_mutex);

  int user_index = find_user_by_name(username);
  int channel_index = find_channel_by_name(channel);

  if (user_index == -1 || channel_index == -1)
  {
    printf("[ERROR] add_message: Could not find user (%d) or channel (%d)\n", user_index, channel_index);
    pthread_mutex_unlock(&messages_mutex);
    return -1;
  }

  int message_id = message_count;

  // Check if we're connected to the database and should use it
  if (db_conn && use_database)
  {
    printf("[DEBUG] add_message: Using database to store message\n");
    // Use database to store message
    int db_user_id = db_find_user_by_name(username);
    int db_channel_id = db_find_channel_by_name(channel);

    printf("[DEBUG] add_message: Found db_user_id=%d, db_channel_id=%d\n", db_user_id, db_channel_id);

    if (db_user_id > 0 && db_channel_id > 0)
    {
      const char *paramValues[3];
      char user_id_str[12];
      char channel_id_str[12];

      snprintf(user_id_str, sizeof(user_id_str), "%d", db_user_id);
      snprintf(channel_id_str, sizeof(channel_id_str), "%d", db_channel_id);

      paramValues[0] = user_id_str;
      paramValues[1] = channel_id_str;
      paramValues[2] = content;

      printf("[DEBUG] add_message: Executing SQL with params: user_id=%s, channel_id=%s, content=%s\n",
             user_id_str, channel_id_str, content);

      PGresult *res = PQexecParams(db_conn,
                                   "INSERT INTO messages (user_id, channel_id, content) VALUES ($1, $2, $3) RETURNING id",
                                   3, NULL, paramValues, NULL, NULL, 0);

      if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0)
      {
        message_id = atoi(PQgetvalue(res, 0, 0));
        printf("[LOG] Message stored in database with ID: %d\n", message_id);
      }
      else
      {
        fprintf(stderr, "[ERROR] Failed to store message in database: %s\n", PQerrorMessage(db_conn));
      }

      PQclear(res);
    }
    else
    {
      fprintf(stderr, "[ERROR] Failed to find user or channel in database for message\n");
    }
  }
  else
  {
    printf("[DEBUG] add_message: Skipping database storage, db_conn=%p, use_database=%d\n",
           (void *)db_conn, use_database);
  }

  // Also store in memory structure for local lookups
  if (message_count < MAX_MESSAGES)
  {
    messages[message_count].id = message_id;
    messages[message_count].user_id = user_index;
    messages[message_count].channel_id = channel_index;
    strncpy(messages[message_count].content, content, sizeof(messages[message_count].content) - 1);
    messages[message_count].timestamp = time(NULL);
    messages[message_count].is_deleted = false;
    message_count++;
    printf("[DEBUG] add_message: Message stored in memory at index %d\n", message_count - 1);
  }

  pthread_mutex_unlock(&messages_mutex);
  return message_id;
}

bool delete_message(int message_id)
{
  pthread_mutex_lock(&messages_mutex);
  for (int i = 0; i < message_count; i++)
  {
    if (messages[i].id == message_id)
    {
      messages[i].is_deleted = true;
      pthread_mutex_unlock(&messages_mutex);
      return true;
    }
  }
  pthread_mutex_unlock(&messages_mutex);
  return false;
}

bool add_reaction(int user_index, int message_id, const char *emoji)
{
  if (reaction_count >= MAX_REACTIONS)
  {
    return false;
  }

  // Check if reaction already exists
  for (int i = 0; i < reaction_count; i++)
  {
    if (reactions[i].user_id == user_index &&
        reactions[i].message_id == message_id &&
        strcmp(reactions[i].emoji, emoji) == 0)
    {
      return true; // Already exists
    }
  }

  // Add new reaction
  reactions[reaction_count].user_id = user_index;
  reactions[reaction_count].message_id = message_id;
  strncpy(reactions[reaction_count].emoji, emoji, sizeof(reactions[reaction_count].emoji) - 1);
  reactions[reaction_count].emoji[sizeof(reactions[reaction_count].emoji) - 1] = '\0';
  reaction_count++;

  // Store in database if enabled
  if (use_database)
  {
    int db_user_id = -1;
    if (user_index >= 0 && user_index < user_count)
    {
      db_user_id = db_find_user_by_name(users[user_index].username);
    }
    if (db_user_id != -1)
    {
      db_add_reaction(users[user_index].username, message_id, emoji);
    }
  }

  return true;
}

// Mute management functions
bool mute_user(const char *username, const char *channel, int minutes)
{
  int user_index = find_user_by_name(username);
  if (user_index == -1)
  {
    return false;
  }

  int channel_index = find_channel_by_name(channel);
  if (channel_index == -1)
  {
    return false;
  }

  // Check if we have space for another muted user
  if (muted_user_count >= MAX_MUTED_USERS)
  {
    // Check for expired mutes first
    time_t now = time(NULL);
    for (int i = 0; i < muted_user_count; i++)
    {
      if (muted_users[i].end_time <= now)
      {
        // Remove expired mute by moving the last one to this position
        muted_users[i] = muted_users[muted_user_count - 1];
        muted_user_count--;
        break;
      }
    }

    // If still full, we can't mute
    if (muted_user_count >= MAX_MUTED_USERS)
    {
      return false;
    }
  }

  // Add to muted users
  muted_users[muted_user_count].user_id = user_index;
  muted_users[muted_user_count].channel_id = channel_index;
  muted_users[muted_user_count].end_time = time(NULL) + (minutes * 60);
  muted_user_count++;

  // Store in database if enabled
  if (use_database)
  {
    int db_user_id = db_find_user_by_name(username);
    int db_channel_id = db_find_channel_by_name(channel);
    if (db_user_id != -1 && db_channel_id != -1)
    {
      db_mute_user_internal(db_conn, db_user_id, db_channel_id, minutes);
    }
  }

  return true;
}

bool is_user_muted(const char *username, const char *channel)
{
  int user_index = find_user_by_name(username);
  if (user_index == -1)
  {
    return false;
  }

  int channel_index = find_channel_by_name(channel);
  if (channel_index == -1)
  {
    return false;
  }

  time_t now = time(NULL);

  for (int i = 0; i < muted_user_count; i++)
  {
    if (muted_users[i].user_id == user_index &&
        muted_users[i].channel_id == channel_index &&
        muted_users[i].end_time > now)
    {
      return true;
    }
  }

  // Check database if enabled
  if (use_database)
  {
    int db_user_id = db_find_user_by_name(username);
    int db_channel_id = db_find_channel_by_name(channel);
    if (db_user_id != -1 && db_channel_id != -1)
    {
      return db_is_user_muted_internal(db_conn, db_user_id, db_channel_id);
    }
  }

  return false;
}

// Permission verification
bool has_permission(const char *username, const char *operation, const char *target)
{
  int role = get_user_role(username);

  if (role == ROLE_ADMIN)
  {
    return true; // Admins can do anything
  }

  if (strcmp(operation, "create_channel") == 0)
  {
    return role >= ROLE_MODERATOR; // Moderators and admins can create channels
  }
  else if (strcmp(operation, "delete_channel") == 0)
  {
    if (strcmp(target, "general") == 0)
    {
      return false; // Nobody can delete general
    }
    return role >= ROLE_MODERATOR; // Moderators and admins can delete channels
  }
  else if (strcmp(operation, "change_role") == 0)
  {
    return role >= ROLE_ADMIN; // Only admins can change roles
  }
  else if (strcmp(operation, "mute") == 0)
  {
    return role >= ROLE_MODERATOR; // Moderators and admins can mute users
  }
  else if (strcmp(operation, "delete_message") == 0)
  {
    return role >= ROLE_MODERATOR; // Moderators and admins can delete messages
  }

  return false;
}

// Broadcast message to all clients in a channel
void broadcast_message(const char *message, const char *channel)
{
  pthread_mutex_lock(&clients_mutex);
  client_node *current = clients_head;

  while (current != NULL)
  {
    if (strcmp(current->current_channel, channel) == 0)
    {
      send(current->socket, message, strlen(message), 0);
    }
    current = current->next;
  }

  pthread_mutex_unlock(&clients_mutex);
}

// Build and broadcast a formatted system message
void broadcast_system_message(const char *format, const char *channel, ...)
{
  char buffer[BUFFER_SIZE];
  va_list args;
  va_start(args, channel);

  // Format the message
  vsnprintf(buffer, sizeof(buffer), format, args);

  // Build the full message with [SYSTEM] prefix
  char full_message[BUFFER_SIZE];
  snprintf(full_message, sizeof(full_message), "[SYSTEM] %s\n", buffer);

  // Broadcast to the channel
  broadcast_message(full_message, channel);

  va_end(args);
}

// Server initialization
bool initialize_server(bool use_db)
{
  printf("[LOG] initialize_server: Initializing server...\n");

  // Set the use_database flag
  use_database = use_db;

  // Initialize mutexes
  pthread_mutex_init(&clients_mutex, NULL);
  pthread_mutex_init(&users_mutex, NULL);
  pthread_mutex_init(&channels_mutex, NULL);
  pthread_mutex_init(&messages_mutex, NULL);

  // Create default "general" channel if it doesn't exist
  if (channel_count == 0)
  {
    strncpy(channels[0].name, "general", sizeof(channels[0].name) - 1);
    strncpy(channels[0].type, "public", sizeof(channels[0].type) - 1);
    channels[0].creator_id = 0; // System
    channel_count++;
    printf("[LOG] initialize_server: Created default general channel\n");
  }

  // Initialize database connection if enabled
  if (use_database)
  {
    printf("[LOG] initialize_server: Connecting to database...\n");
    if (!db_connect())
    {
      fprintf(stderr, "[ERROR] initialize_server: Failed to connect to database\n");
      return false;
    }

    if (!db_init())
    {
      fprintf(stderr, "[ERROR] initialize_server: Failed to initialize database tables\n");
      // Don't call db_disconnect here since db_init already does it on error
      return false;
    }

    // Load data from database
    printf("[LOG] initialize_server: Loading data from database...\n");
    load_users_from_db();
    load_channels_from_db();
  }

  printf("[LOG] initialize_server: Server initialized successfully\n");
  return true;
}

// Clean up on server shutdown
void cleanup_server()
{
  // Close all client connections
  pthread_mutex_lock(&clients_mutex);
  client_node *current = clients_head;
  while (current != NULL)
  {
    client_node *next = current->next;
    close(current->socket);
    free(current);
    current = next;
  }
  clients_head = NULL;
  pthread_mutex_unlock(&clients_mutex);

  // Disconnect from database if connected
  if (use_database && db_conn)
  {
    db_disconnect();
  }

  // Destroy mutexes
  pthread_mutex_destroy(&clients_mutex);
  pthread_mutex_destroy(&users_mutex);
  pthread_mutex_destroy(&channels_mutex);
  pthread_mutex_destroy(&messages_mutex);
}

// Process client messages
void process_message(int client_socket, const char *message)
{
  char buffer[BUFFER_SIZE];
  char username[64] = {0};
  char password[64] = {0}; // Add password variable
  char channel[32] = {0};
  char content[BUFFER_SIZE] = {0};

  // Check if it's a command (starts with /)
  if (message[0] == '/')
  {
    // Parse the command
    char cmd[32];
    char args[BUFFER_SIZE - 32];

    sscanf(message, "/%31s %[^\n]", cmd, args);

    // Handle different commands
    if (strcmp(cmd, "join") == 0)
    {
      int channel_index = find_channel_by_name(args);
      if (channel_index == -1)
      {
        snprintf(buffer, sizeof(buffer), "[ERROR] Channel #%s does not exist\n", args);
        send(client_socket, buffer, strlen(buffer), 0);
      }
      else
      {
        // Leave current channel
        snprintf(buffer, sizeof(buffer), "[SYSTEM] %s has left #%s\n", username, channel);
        broadcast_message(buffer, channel);

        // Join new channel
        set_client_channel(client_socket, args);

        snprintf(buffer, sizeof(buffer), "[SYSTEM] %s has joined #%s\n", username, args);
        broadcast_message(buffer, args);

        snprintf(buffer, sizeof(buffer), "[SUCCESS] You have joined #%s\n", args);
        send(client_socket, buffer, strlen(buffer), 0);
      }
    }
    else if (strcmp(cmd, "create") == 0)
    {
      // Extract channel name and type
      char channel_name[32];
      char channel_type[16] = "public"; // Default to public

      sscanf(args, "%31s %15s", channel_name, channel_type);

      // Check permission
      if (!has_permission(username, "create_channel", channel_name))
      {
        snprintf(buffer, sizeof(buffer), "[ERROR] You don't have permission to create channels\n");
        send(client_socket, buffer, strlen(buffer), 0);
        return;
      }

      // Check if channel exists
      if (find_channel_by_name(channel_name) != -1)
      {
        snprintf(buffer, sizeof(buffer), "[ERROR] Channel #%s already exists\n", channel_name);
        send(client_socket, buffer, strlen(buffer), 0);
      }
      else
      {
        int user_index = find_user_by_name(username);
        if (create_channel(channel_name, channel_type, user_index))
        {
          snprintf(buffer, sizeof(buffer), "[SUCCESS] Channel #%s created\n", channel_name);
          send(client_socket, buffer, strlen(buffer), 0);

          // Broadcast to all users
          snprintf(buffer, sizeof(buffer), "[SYSTEM] Channel #%s has been created by %s\n",
                   channel_name, username);
          broadcast_message(buffer, "general");
        }
        else
        {
          snprintf(buffer, sizeof(buffer), "[ERROR] Failed to create channel #%s\n", channel_name);
          send(client_socket, buffer, strlen(buffer), 0);
        }
      }
    }
    else if (strcmp(cmd, "delete") == 0)
    {
      // Check permission
      if (!has_permission(username, "delete_channel", args))
      {
        snprintf(buffer, sizeof(buffer), "[ERROR] You don't have permission to delete channels\n");
        send(client_socket, buffer, strlen(buffer), 0);
        return;
      }

      if (delete_channel(args))
      {
        snprintf(buffer, sizeof(buffer), "[SUCCESS] Channel #%s deleted\n", args);
        send(client_socket, buffer, strlen(buffer), 0);

        // Move all users from deleted channel to general
        pthread_mutex_lock(&clients_mutex);
        client_node *curr = clients_head;
        while (curr != NULL)
        {
          if (strcmp(curr->current_channel, args) == 0)
          {
            strcpy(curr->current_channel, "general");

            // Notify user they were moved
            snprintf(buffer, sizeof(buffer),
                     "[SYSTEM] Channel #%s was deleted. You've been moved to #general\n", args);
            send(curr->socket, buffer, strlen(buffer), 0);
          }
          curr = curr->next;
        }
        pthread_mutex_unlock(&clients_mutex);

        // Broadcast to all users
        snprintf(buffer, sizeof(buffer), "[SYSTEM] Channel #%s has been deleted by %s\n",
                 args, username);
        broadcast_message(buffer, "general");
      }
      else
      {
        snprintf(buffer, sizeof(buffer), "[ERROR] Failed to delete channel #%s\n", args);
        send(client_socket, buffer, strlen(buffer), 0);
      }
    }
    else if (strcmp(cmd, "role") == 0)
    {
      // Check permission
      if (!has_permission(username, "change_role", NULL))
      {
        snprintf(buffer, sizeof(buffer), "[ERROR] You don't have permission to change roles\n");
        send(client_socket, buffer, strlen(buffer), 0);
        return;
      }

      char target_user[32];
      char role_str[32];
      sscanf(args, "%31s %31s", target_user, role_str);

      int role_val = ROLE_MEMBER;
      if (strcmp(role_str, "admin") == 0)
        role_val = ROLE_ADMIN;
      else if (strcmp(role_str, "moderator") == 0)
        role_val = ROLE_MODERATOR;

      if (update_user_role(target_user, role_val))
      {
        snprintf(buffer, sizeof(buffer), "[SUCCESS] %s's role updated to %s\n",
                 target_user, role_str);
        send(client_socket, buffer, strlen(buffer), 0);
      }
      else
      {
        snprintf(buffer, sizeof(buffer), "[ERROR] Failed to update %s's role\n", target_user);
        send(client_socket, buffer, strlen(buffer), 0);
      }
    }
    else if (strcmp(cmd, "mute") == 0)
    {
      // Check permission
      if (!has_permission(username, "mute", NULL))
      {
        snprintf(buffer, sizeof(buffer), "[ERROR] You don't have permission to mute users\n");
        send(client_socket, buffer, strlen(buffer), 0);
        return;
      }

      char target_user[32];
      char channel_name[32];
      int minutes = 5; // Default mute time

      sscanf(args, "%31s %31s %d", target_user, channel_name, &minutes);

      if (mute_user(target_user, channel_name, minutes))
      {
        snprintf(buffer, sizeof(buffer), "[SUCCESS] %s has been muted in #%s for %d minutes\n",
                 target_user, channel_name, minutes);
        send(client_socket, buffer, strlen(buffer), 0);

        // Notify the channel
        snprintf(buffer, sizeof(buffer), "[SYSTEM] %s has been muted by %s for %d minutes\n",
                 target_user, username, minutes);
        broadcast_message(buffer, channel_name);
      }
      else
      {
        snprintf(buffer, sizeof(buffer), "[ERROR] Failed to mute %s in #%s\n",
                 target_user, channel_name);
        send(client_socket, buffer, strlen(buffer), 0);
      }
    }
    else if (strcmp(cmd, "react") == 0)
    {
      int message_id;
      char emoji[16];
      sscanf(args, "%d %15s", &message_id, emoji);

      int user_index = find_user_by_name(username);

      if (add_reaction(user_index, message_id, emoji))
      {
        // Find which channel the message is in
        char channel[32] = "general";
        for (int i = 0; i < message_count; i++)
        {
          if (messages[i].id == message_id)
          {
            strcpy(channel, channels[messages[i].channel_id].name);
            break;
          }
        }

        snprintf(buffer, sizeof(buffer), "[REACTION] %s reacted with %s to message #%d\n",
                 username, emoji, message_id);
        broadcast_message(buffer, channel);
      }
      else
      {
        snprintf(buffer, sizeof(buffer), "[ERROR] Failed to add reaction\n");
        send(client_socket, buffer, strlen(buffer), 0);
      }
    }
    else if (strcmp(cmd, "list") == 0)
    {
      if (strcmp(args, "channels") == 0)
      {
        snprintf(buffer, sizeof(buffer), "=== Channel List ===\n");
        send(client_socket, buffer, strlen(buffer), 0);

        pthread_mutex_lock(&channels_mutex);
        for (int i = 0; i < channel_count; i++)
        {
          snprintf(buffer, sizeof(buffer), "#%s (%s)\n",
                   channels[i].name, channels[i].type);
          send(client_socket, buffer, strlen(buffer), 0);
        }
        pthread_mutex_unlock(&channels_mutex);
      }
      else if (strcmp(args, "users") == 0)
      {
        snprintf(buffer, sizeof(buffer), "=== User List ===\n");
        send(client_socket, buffer, strlen(buffer), 0);

        pthread_mutex_lock(&users_mutex);
        for (int i = 0; i < user_count; i++)
        {
          const char *role_str = "member";
          if (users[i].role == ROLE_ADMIN)
            role_str = "admin";
          else if (users[i].role == ROLE_MODERATOR)
            role_str = "moderator";

          snprintf(buffer, sizeof(buffer), "%s (%s) - %s\n",
                   users[i].username, role_str,
                   users[i].is_online ? "online" : "offline");
          send(client_socket, buffer, strlen(buffer), 0);
        }
        pthread_mutex_unlock(&users_mutex);
      }
      else
      {
        snprintf(buffer, sizeof(buffer), "[ERROR] Unknown list command. Try 'channels' or 'users'\n");
        send(client_socket, buffer, strlen(buffer), 0);
      }
    }
    else if (strcmp(cmd, "help") == 0)
    {
      snprintf(buffer, sizeof(buffer),
               "=== Command Help ===\n"
               "/join <channel> - Join a channel\n"
               "/create <channel> [public|private] - Create a new channel\n"
               "/delete <channel> - Delete a channel\n"
               "/role <username> [admin|moderator|member] - Change user role\n"
               "/mute <username> <channel> [minutes] - Mute a user\n"
               "/react <message_id> <emoji> - React to a message\n"
               "/list [channels|users] - List channels or users\n"
               "/help - Display this help message\n");
      send(client_socket, buffer, strlen(buffer), 0);
    }
    else
    {
      snprintf(buffer, sizeof(buffer), "[ERROR] Unknown command: %s\n", cmd);
      send(client_socket, buffer, strlen(buffer), 0);
    }
  }
  else if (strncmp(message, "AUTH ", 5) == 0)
  {
    // Extract username and password from message
    char *auth_msg = strdup(message);
    char *username_part = strchr(auth_msg, ' ');

    if (username_part)
    {
      username_part++; // Skip the space
      char *password_part = strchr(username_part, '\n');

      if (password_part)
      {
        *password_part = '\0';
        password_part++;

        // Remove trailing newline from password if it exists
        char *password_end = strchr(password_part, '\n');
        if (password_end)
        {
          *password_end = '\0';
        }

        strncpy(username, username_part, sizeof(username) - 1);
        strncpy(password, password_part, sizeof(password) - 1);
      }
    }

    free(auth_msg);

    // Skip the "AUTH " part to get the username
    const char *username_start = message + 5;
    strncpy(username, username_start, sizeof(username) - 1);

    // Check if username is already taken
    int user_index = find_user_by_name(username);

    if (user_index != -1)
    {
      // User already exists, perform authentication
      // For simplicity in this mock, we just accept any password
      // In a real system, compare hashed passwords

      // Update user's online status
      pthread_mutex_lock(&users_mutex);
      users[user_index].socket = client_socket;
      users[user_index].is_online = true;
      int role = users[user_index].role;
      pthread_mutex_unlock(&users_mutex);

      // Add to active clients
      add_client(client_socket, username);

      // Send welcome message
      char welcome[BUFFER_SIZE];
      snprintf(welcome, sizeof(welcome), "Welcome to MyDiscord! You are now connected.\n");
      send(client_socket, welcome, strlen(welcome), 0);

      // Update user's role based on database info
      char role_msg[BUFFER_SIZE];
      const char *role_name = "Guest";

      if (role == ROLE_MEMBER)
        role_name = "Member";
      else if (role == ROLE_MODERATOR)
        role_name = "Mod";
      else if (role == ROLE_ADMIN)
        role_name = "Admin";

      snprintf(role_msg, sizeof(role_msg), "\n[SYSTEM] Your role has been updated to: %s\n", role_name);
      send(client_socket, role_msg, strlen(role_msg), 0);

      // Simulate joining the general channel
      broadcast_system_message("\n[SYSTEM] %s has joined the channel!\n", "general", username);
      send_system_message(client_socket, "\n[SYSTEM] Successfully logged in. You're now in #general channel.\n");
    }
    else
    {
      // New user registration
      pthread_mutex_lock(&users_mutex);
      if (user_count < MAX_USERS)
      {
        // Create new user with default role (member)
        int new_role = ROLE_MEMBER;

        // Store in database if enabled
        if (db_conn && use_database)
        {
          const char *paramValues[3];
          char role_str[2];

          paramValues[0] = username;
          paramValues[1] = password;

          snprintf(role_str, sizeof(role_str), "%d", new_role);
          paramValues[2] = role_str;

          PGresult *res = PQexecParams(db_conn,
                                       "INSERT INTO users (username, password, role, is_online, current_channel) VALUES ($1, $2, $3, true, 'general') RETURNING id",
                                       3, NULL, paramValues, NULL, NULL, 0);

          if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0)
          {
            users[user_count].id = atoi(PQgetvalue(res, 0, 0));
            printf("[LOG] User created in database with ID: %d\n", users[user_count].id);
          }
          else
          {
            fprintf(stderr, "[ERROR] Failed to create user in database: %s\n", PQerrorMessage(db_conn));
          }

          PQclear(res);
        }

        // Also store in memory
        strncpy(users[user_count].username, username, sizeof(users[user_count].username) - 1);
        users[user_count].username[sizeof(users[user_count].username) - 1] = '\0';
        users[user_count].socket = client_socket;
        users[user_count].is_online = true;
        strcpy(users[user_count].current_channel, "general");
        users[user_count].role = new_role;
        user_count++;

        // Add to active clients
        add_client(client_socket, username);

        // Send welcome message for new user
        char welcome[BUFFER_SIZE];
        snprintf(welcome, sizeof(welcome), "Welcome to MyDiscord! You have been registered and are now connected.\n");
        send(client_socket, welcome, strlen(welcome), 0);

        // Notify about role
        char role_msg[BUFFER_SIZE];
        snprintf(role_msg, sizeof(role_msg), "\n[SYSTEM] Your role has been set to: Member\n");
        send(client_socket, role_msg, strlen(role_msg), 0);

        // Simulate joining the general channel
        broadcast_system_message("\n[SYSTEM] %s has joined the channel!\n", "general", username);
        send_system_message(client_socket, "\n[SYSTEM] Successfully registered. You're now in #general channel.\n");
      }
      else
      {
        send_system_message(client_socket, "[ERROR] Maximum number of users reached. Registration failed.\n");
      }
      pthread_mutex_unlock(&users_mutex);
    }
  }
  else
  {
    // Regular message - broadcast to current channel if user is not muted
    if (is_user_muted(username, channel))
    {
      snprintf(buffer, sizeof(buffer), "[ERROR] You are muted in #%s\n", channel);
      send(client_socket, buffer, strlen(buffer), 0);
      return;
    }

    // Add message to storage
    int msg_id = add_message(username, channel, message);

    if (msg_id >= 0)
    {
      // Format as a chat message and broadcast to channel
      snprintf(buffer, sizeof(buffer), "[%s] %s: %s\n",
               channel, username, message);
      broadcast_message(buffer, channel);
    }
  }
}

// Handle client connection thread function
void *handle_client(void *arg)
{
  int client_socket = *((int *)arg);
  free(arg);

  char buffer[BUFFER_SIZE];
  int recv_size;

  // Authenticate the user
  if ((recv_size = recv(client_socket, buffer, sizeof(buffer) - 1, 0)) > 0)
  {
    buffer[recv_size] = '\0';

    process_message(client_socket, buffer);
  }

  close(client_socket);
  return NULL;
}

// Start the server
int start_server(int port, bool use_db)
{
  // Save the use_database flag globally
  use_database = use_db;

  // Setup logging
  FILE *log_file = fopen("server_output.log", "w");
  if (log_file != NULL)
  {
    // Redirect stdout to the log file
    dup2(fileno(log_file), STDOUT_FILENO);
    // Redirect stderr to the log file
    dup2(fileno(log_file), STDERR_FILENO);
    fclose(log_file);
  }

  printf("[LOG] Starting server on port %d...\n", port);
  printf("[LOG] Database integration: %s\n", use_database ? "ENABLED" : "DISABLED");

  // Initialize the database if needed
  if (use_database)
  {
    printf("[LOG] Starting server with database integration...\n");

    // Connect to database
    if (!db_connect())
    {
      fprintf(stderr, "[ERROR] Failed to connect to database, exiting\n");
      return 1;
    }

    // Initialize database tables
    if (!db_init())
    {
      fprintf(stderr, "[ERROR] Failed to initialize database tables, exiting\n");
      db_disconnect();
      return 1;
    }

    printf("[LOG] Database connection successful and tables verified\n");

    // Load existing data from database
    load_users_from_db();
    load_channels_from_db();
  }
  else
  {
    printf("[LOG] Starting server in memory-only mode (no database)\n");
  }

  // If we don't have any channels, create a default one
  pthread_mutex_lock(&channels_mutex);
  if (channel_count == 0)
  {
    strncpy(channels[0].name, "general", sizeof(channels[0].name) - 1);
    strncpy(channels[0].type, "public", sizeof(channels[0].type) - 1);
    channels[0].creator_id = 0; // System
    channel_count++;
    printf("[LOG] Created default general channel\n");
  }
  pthread_mutex_unlock(&channels_mutex);

  // Basic server setup only to verify database functionality
  printf("[LOG] Server initialized in test mode. Database verification complete.\n");

  // Sleep to allow test client to connect
  sleep(30);

  return 0;
}

int main(int argc, char *argv[])
{
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
    }
    else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc)
    {
      port = atoi(argv[i + 1]);
      printf("Using port %d\n", port);
      i++; // Skip the port value
    }
  }

  // Open log file
  FILE *log_file = fopen("server_output.log", "w");
  fprintf(log_file, "[LOG] Starting MyDiscord server on port %d...\n", port);
  fprintf(log_file, "[LOG] Database integration: %s\n", use_db ? "ENABLED" : "DISABLED");
  fclose(log_file);

  return start_server(port, use_db);
}

// Database interaction functions for find_user and find_channel
int db_find_user_by_name(const char *username)
{
  if (!db_conn || !use_database)
  {
    return -1;
  }

  const char *paramValues[1];
  paramValues[0] = username;

  PGresult *res = PQexecParams(db_conn,
                               "SELECT id FROM users WHERE username = $1",
                               1, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "[ERROR] Failed to find user: %s\n", PQerrorMessage(db_conn));
    PQclear(res);
    return -1;
  }

  int id = -1;
  if (PQntuples(res) > 0)
  {
    id = atoi(PQgetvalue(res, 0, 0));
  }

  PQclear(res);
  return id;
}

int db_find_channel_by_name(const char *channel_name)
{
  if (!db_conn || !use_database)
  {
    return -1;
  }

  const char *paramValues[1];
  paramValues[0] = channel_name;

  PGresult *res = PQexecParams(db_conn,
                               "SELECT id FROM channels WHERE name = $1",
                               1, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "[ERROR] Failed to find channel: %s\n", PQerrorMessage(db_conn));
    PQclear(res);
    return -1;
  }

  int id = -1;
  if (PQntuples(res) > 0)
  {
    id = atoi(PQgetvalue(res, 0, 0));
  }

  PQclear(res);
  return id;
}