#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Connect to the PostgreSQL database
PGconn *db_connect(void)
{
  char conninfo[256];
  snprintf(conninfo, sizeof(conninfo),
           "host=%s port=%s dbname=%s user=%s password=%s",
           DB_HOST, DB_PORT, DB_NAME, DB_USER, DB_PASSWORD);

  PGconn *conn = PQconnectdb(conninfo);

  if (PQstatus(conn) != CONNECTION_OK)
  {
    fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(conn));
    PQfinish(conn);
    return NULL;
  }

  return conn;
}

// Disconnect from the PostgreSQL database
void db_disconnect(PGconn *conn)
{
  if (conn)
  {
    PQfinish(conn);
  }
}

// Initialize database connection
bool db_init(void)
{
  PGconn *conn = db_connect();
  if (!conn)
  {
    return false;
  }

  printf("[LOG] Successfully connected to PostgreSQL database: %s\n", DB_NAME);

  // Test query to verify connection
  PGresult *res = PQexec(conn, "SELECT 1");
  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    printf("[ERROR] Database connection test failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    db_disconnect(conn);
    return false;
  }

  PQclear(res);
  db_disconnect(conn);
  return true;
}

// Find user by username
int db_find_user_by_name(PGconn *conn, const char *username)
{
  char query[256];
  snprintf(query, sizeof(query), "SELECT id FROM users WHERE username = $1");

  const char *paramValues[1];
  paramValues[0] = username;

  PGresult *res = PQexecParams(conn, query, 1, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "User search failed: %s\n", PQerrorMessage(conn));
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

// Create a new user
int db_create_user(PGconn *conn, const char *username, const char *password, int role)
{
  char query[512];
  snprintf(query, sizeof(query),
           "INSERT INTO users (username, password, role, is_online, current_channel) VALUES ($1, $2, $3, false, 'general') RETURNING id");

  const char *paramValues[3];
  paramValues[0] = username;
  paramValues[1] = password;

  char role_str[2];
  snprintf(role_str, sizeof(role_str), "%d", role);
  paramValues[2] = role_str;

  PGresult *res = PQexecParams(conn, query, 3, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "User creation failed: %s\n", PQerrorMessage(conn));
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

// Set user online status
bool db_set_user_online_status(PGconn *conn, int user_id, bool is_online)
{
  char query[256];
  snprintf(query, sizeof(query),
           "UPDATE users SET is_online = $1 WHERE id = $2");

  const char *paramValues[2];
  paramValues[0] = is_online ? "TRUE" : "FALSE";

  char id_str[12];
  snprintf(id_str, sizeof(id_str), "%d", user_id);
  paramValues[1] = id_str;

  PGresult *res = PQexecParams(conn, query, 2, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "User status update failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    return false;
  }

  PQclear(res);
  return true;
}

// Update user's current channel
bool db_update_user_channel(PGconn *conn, int user_id, const char *channel)
{
  char query[256];
  snprintf(query, sizeof(query),
           "UPDATE users SET current_channel = $1 WHERE id = $2");

  const char *paramValues[2];
  paramValues[0] = channel;

  char id_str[12];
  snprintf(id_str, sizeof(id_str), "%d", user_id);
  paramValues[1] = id_str;

  PGresult *res = PQexecParams(conn, query, 2, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "User channel update failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    return false;
  }

  PQclear(res);
  return true;
}

// Set user role
bool db_set_user_role(PGconn *conn, int user_id, int role)
{
  char query[256];
  snprintf(query, sizeof(query),
           "UPDATE users SET role = $1 WHERE id = $2");

  const char *paramValues[2];

  char role_str[2];
  snprintf(role_str, sizeof(role_str), "%d", role);
  paramValues[0] = role_str;

  char id_str[12];
  snprintf(id_str, sizeof(id_str), "%d", user_id);
  paramValues[1] = id_str;

  PGresult *res = PQexecParams(conn, query, 2, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "User role update failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    return false;
  }

  PQclear(res);
  return true;
}

// Get user by ID
User db_get_user(PGconn *conn, int user_id)
{
  User user;
  memset(&user, 0, sizeof(User));
  user.id = -1; // Default to invalid ID
  user.is_online = false;

  char query[256];
  snprintf(query, sizeof(query),
           "SELECT id, username, role, is_online, current_channel FROM users WHERE id = $1");

  const char *paramValues[1];
  char id_str[12];
  snprintf(id_str, sizeof(id_str), "%d", user_id);
  paramValues[0] = id_str;

  PGresult *res = PQexecParams(conn, query, 1, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "User query failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    return user;
  }

  if (PQntuples(res) > 0)
  {
    user.id = atoi(PQgetvalue(res, 0, 0));
    strncpy(user.username, PQgetvalue(res, 0, 1), sizeof(user.username) - 1);
    user.role = atoi(PQgetvalue(res, 0, 2));
    user.is_online = strcmp(PQgetvalue(res, 0, 3), "t") == 0;
    strncpy(user.current_channel, PQgetvalue(res, 0, 4), sizeof(user.current_channel) - 1);
  }

  PQclear(res);
  return user;
}

// Get user by username
User db_get_user_by_name(PGconn *conn, const char *username)
{
  User user;
  memset(&user, 0, sizeof(User));
  user.id = -1; // Default to invalid ID
  user.is_online = false;

  char query[256];
  snprintf(query, sizeof(query),
           "SELECT id, username, role, is_online, current_channel FROM users WHERE username = $1");

  const char *paramValues[1];
  paramValues[0] = username;

  PGresult *res = PQexecParams(conn, query, 1, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "User query failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    return user;
  }

  if (PQntuples(res) > 0)
  {
    user.id = atoi(PQgetvalue(res, 0, 0));
    strncpy(user.username, PQgetvalue(res, 0, 1), sizeof(user.username) - 1);
    user.role = atoi(PQgetvalue(res, 0, 2));
    user.is_online = strcmp(PQgetvalue(res, 0, 3), "t") == 0;
    strncpy(user.current_channel, PQgetvalue(res, 0, 4), sizeof(user.current_channel) - 1);
  }

  PQclear(res);
  return user;
}

// Find channel by name
int db_find_channel_by_name(PGconn *conn, const char *channel_name)
{
  char query[256];
  snprintf(query, sizeof(query), "SELECT id FROM channels WHERE name = $1");

  const char *paramValues[1];
  paramValues[0] = channel_name;

  PGresult *res = PQexecParams(conn, query, 1, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "Channel search failed: %s\n", PQerrorMessage(conn));
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

// Create a new channel
int db_create_channel(PGconn *conn, const char *name, const char *type, int creator_id)
{
  char query[512];
  snprintf(query, sizeof(query),
           "INSERT INTO channels (name, type, creator_id) VALUES ($1, $2, $3) RETURNING id");

  const char *paramValues[3];
  paramValues[0] = name;
  paramValues[1] = type;

  char creator_id_str[12];
  snprintf(creator_id_str, sizeof(creator_id_str), "%d", creator_id);
  paramValues[2] = creator_id_str;

  PGresult *res = PQexecParams(conn, query, 3, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "Channel creation failed: %s\n", PQerrorMessage(conn));
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

// Delete a channel
bool db_delete_channel(PGconn *conn, const char *name)
{
  // Don't allow deletion of general channel
  if (strcmp(name, "general") == 0)
  {
    return false;
  }

  char query[256];
  snprintf(query, sizeof(query),
           "DELETE FROM channels WHERE name = $1 AND name != 'general'");

  const char *paramValues[1];
  paramValues[0] = name;

  PGresult *res = PQexecParams(conn, query, 1, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "Channel deletion failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    return false;
  }

  // Check if any rows were affected
  if (strcmp(PQcmdTuples(res), "0") == 0)
  {
    PQclear(res);
    return false;
  }

  PQclear(res);
  return true;
}

// Get all channels
Channel *db_get_all_channels(PGconn *conn, int *count)
{
  char query[256];
  snprintf(query, sizeof(query),
           "SELECT id, name, type, creator_id FROM channels ORDER BY name");

  PGresult *res = PQexec(conn, query);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "Channel list fetch failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    *count = 0;
    return NULL;
  }

  int num_rows = PQntuples(res);
  *count = num_rows;

  if (num_rows == 0)
  {
    PQclear(res);
    return NULL;
  }

  Channel *channels = malloc(num_rows * sizeof(Channel));
  if (!channels)
  {
    PQclear(res);
    *count = 0;
    return NULL;
  }

  for (int i = 0; i < num_rows; i++)
  {
    channels[i].id = atoi(PQgetvalue(res, i, 0));
    strncpy(channels[i].name, PQgetvalue(res, i, 1), sizeof(channels[i].name) - 1);
    strncpy(channels[i].type, PQgetvalue(res, i, 2), sizeof(channels[i].type) - 1);
    channels[i].creator_id = atoi(PQgetvalue(res, i, 3));
  }

  PQclear(res);
  return channels;
}

// Store a new message
int db_store_message(PGconn *conn, int user_id, int channel_id, const char *content)
{
  char query[512];
  snprintf(query, sizeof(query),
           "INSERT INTO messages (user_id, channel_id, content) VALUES ($1, $2, $3) RETURNING id");

  const char *paramValues[3];

  char user_id_str[12];
  snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
  paramValues[0] = user_id_str;

  char channel_id_str[12];
  snprintf(channel_id_str, sizeof(channel_id_str), "%d", channel_id);
  paramValues[1] = channel_id_str;

  paramValues[2] = content;

  PGresult *res = PQexecParams(conn, query, 3, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "Message storage failed: %s\n", PQerrorMessage(conn));
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

// Get messages for a channel
Message *db_get_channel_messages(PGconn *conn, int channel_id, int *count)
{
  char query[256];
  snprintf(query, sizeof(query),
           "SELECT id, user_id, channel_id, content, extract(epoch from timestamp), is_deleted "
           "FROM messages WHERE channel_id = $1 AND is_deleted = FALSE "
           "ORDER BY timestamp DESC LIMIT 50");

  const char *paramValues[1];
  char channel_id_str[12];
  snprintf(channel_id_str, sizeof(channel_id_str), "%d", channel_id);
  paramValues[0] = channel_id_str;

  PGresult *res = PQexecParams(conn, query, 1, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "Message fetch failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    *count = 0;
    return NULL;
  }

  int num_rows = PQntuples(res);
  *count = num_rows;

  if (num_rows == 0)
  {
    PQclear(res);
    return NULL;
  }

  Message *messages = malloc(num_rows * sizeof(Message));
  if (!messages)
  {
    PQclear(res);
    *count = 0;
    return NULL;
  }

  for (int i = 0; i < num_rows; i++)
  {
    messages[i].id = atoi(PQgetvalue(res, i, 0));
    messages[i].user_id = atoi(PQgetvalue(res, i, 1));
    messages[i].channel_id = atoi(PQgetvalue(res, i, 2));
    strncpy(messages[i].content, PQgetvalue(res, i, 3), sizeof(messages[i].content) - 1);
    messages[i].timestamp = (time_t)atol(PQgetvalue(res, i, 4));
    messages[i].is_deleted = strcmp(PQgetvalue(res, i, 5), "t") == 0;
  }

  PQclear(res);
  return messages;
}

// Mark a message as deleted
bool db_delete_message(PGconn *conn, int message_id)
{
  char query[256];
  snprintf(query, sizeof(query),
           "UPDATE messages SET is_deleted = TRUE WHERE id = $1");

  const char *paramValues[1];
  char id_str[12];
  snprintf(id_str, sizeof(id_str), "%d", message_id);
  paramValues[0] = id_str;

  PGresult *res = PQexecParams(conn, query, 1, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "Message deletion failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    return false;
  }

  PQclear(res);
  return true;
}

// Add reaction to a message
bool db_add_reaction(PGconn *conn, int user_id, int message_id, const char *emoji)
{
  // First, check if this user already reacted with this emoji to this message
  char check_query[256];
  snprintf(check_query, sizeof(check_query),
           "SELECT id FROM reactions WHERE user_id = $1 AND message_id = $2 AND emoji = $3");

  const char *checkParams[3];

  char user_id_str[12];
  snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
  checkParams[0] = user_id_str;

  char message_id_str[12];
  snprintf(message_id_str, sizeof(message_id_str), "%d", message_id);
  checkParams[1] = message_id_str;

  checkParams[2] = emoji;

  PGresult *check_res = PQexecParams(conn, check_query, 3, NULL, checkParams, NULL, NULL, 0);

  if (PQresultStatus(check_res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "Reaction check failed: %s\n", PQerrorMessage(conn));
    PQclear(check_res);
    return false;
  }

  // If reaction already exists, don't add a duplicate
  if (PQntuples(check_res) > 0)
  {
    PQclear(check_res);
    return true; // Already exists, consider it a success
  }

  PQclear(check_res);

  // Add the new reaction
  char query[512];
  snprintf(query, sizeof(query),
           "INSERT INTO reactions (user_id, message_id, emoji) VALUES ($1, $2, $3)");

  const char *paramValues[3];
  paramValues[0] = user_id_str;
  paramValues[1] = message_id_str;
  paramValues[2] = emoji;

  PGresult *res = PQexecParams(conn, query, 3, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "Reaction creation failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    return false;
  }

  PQclear(res);
  return true;
}

// Get reactions for a message
Reaction *db_get_message_reactions(PGconn *conn, int message_id, int *count)
{
  char query[256];
  snprintf(query, sizeof(query),
           "SELECT message_id, user_id, emoji FROM reactions WHERE message_id = $1");

  const char *paramValues[1];
  char message_id_str[12];
  snprintf(message_id_str, sizeof(message_id_str), "%d", message_id);
  paramValues[0] = message_id_str;

  PGresult *res = PQexecParams(conn, query, 1, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "Reactions fetch failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    *count = 0;
    return NULL;
  }

  int num_rows = PQntuples(res);
  *count = num_rows;

  if (num_rows == 0)
  {
    PQclear(res);
    return NULL;
  }

  Reaction *reactions = malloc(num_rows * sizeof(Reaction));
  if (!reactions)
  {
    PQclear(res);
    *count = 0;
    return NULL;
  }

  for (int i = 0; i < num_rows; i++)
  {
    reactions[i].message_id = atoi(PQgetvalue(res, i, 0));
    reactions[i].user_id = atoi(PQgetvalue(res, i, 1));
    strncpy(reactions[i].emoji, PQgetvalue(res, i, 2), sizeof(reactions[i].emoji) - 1);
  }

  PQclear(res);
  return reactions;
}

// Mute a user in a channel
bool db_mute_user(PGconn *conn, int user_id, int channel_id, int minutes)
{
  char query[512];
  snprintf(query, sizeof(query),
           "INSERT INTO muted_users (user_id, channel_id, end_time) "
           "VALUES ($1, $2, NOW() + INTERVAL '%d minutes')",
           minutes);

  const char *paramValues[2];

  char user_id_str[12];
  snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
  paramValues[0] = user_id_str;

  char channel_id_str[12];
  snprintf(channel_id_str, sizeof(channel_id_str), "%d", channel_id);
  paramValues[1] = channel_id_str;

  PGresult *res = PQexecParams(conn, query, 2, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "User mute failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    return false;
  }

  PQclear(res);
  return true;
}

// Check if a user is muted in a channel
bool db_is_user_muted(PGconn *conn, int user_id, int channel_id)
{
  char query[256];
  snprintf(query, sizeof(query),
           "SELECT COUNT(*) FROM muted_users "
           "WHERE user_id = $1 AND channel_id = $2 AND end_time > NOW()");

  const char *paramValues[2];

  char user_id_str[12];
  snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
  paramValues[0] = user_id_str;

  char channel_id_str[12];
  snprintf(channel_id_str, sizeof(channel_id_str), "%d", channel_id);
  paramValues[1] = channel_id_str;

  PGresult *res = PQexecParams(conn, query, 2, NULL, paramValues, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "User mute check failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    return false;
  }

  bool is_muted = false;
  if (PQntuples(res) > 0)
  {
    int count = atoi(PQgetvalue(res, 0, 0));
    is_muted = (count > 0);
  }

  PQclear(res);
  return is_muted;
}