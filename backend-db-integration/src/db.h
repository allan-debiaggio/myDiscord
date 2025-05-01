#ifndef DB_H
#define DB_H

#include <stdbool.h>
#include <time.h>
#include <libpq-fe.h>

// Database connection settings
#define DB_HOST "localhost"
#define DB_PORT "5432"
#define DB_NAME "mydiscord30"
#define DB_USER "postgres"
#define DB_PASSWORD "postgres"

// Avoid redefining structures if they are already defined in server_mock.c
#ifndef MESSAGE_STRUCT_DEFINED
#define MESSAGE_STRUCT_DEFINED
// Message structure
typedef struct
{
  int id;
  int user_id;
  int channel_id;
  char content[2048];
  time_t timestamp;
  bool is_deleted;
} Message;
#endif

#ifndef REACTION_STRUCT_DEFINED
#define REACTION_STRUCT_DEFINED
// Reaction structure
typedef struct
{
  int message_id;
  int user_id;
  char emoji[8];
} Reaction;
#endif

#ifndef MUTED_USER_STRUCT_DEFINED
#define MUTED_USER_STRUCT_DEFINED
// Muted user structure
typedef struct
{
  int user_id;
  int channel_id;
  time_t end_time;
} MutedUser;
#endif

#ifndef USER_STRUCT_DEFINED
#define USER_STRUCT_DEFINED
// User structure
typedef struct
{
  int id;
  int socket;
  char username[64];
  bool is_online;
  char current_channel[32];
  int role;
} User;
#endif

#ifndef CHANNEL_STRUCT_DEFINED
#define CHANNEL_STRUCT_DEFINED
// Channel structure
typedef struct
{
  int id;
  char name[32];
  char type[10];
  int creator_id;
} Channel;
#endif

// Function declarations
PGconn *db_connect(void);
void db_disconnect(PGconn *conn);
bool db_init(void);

// User operations
int db_find_user_by_name(PGconn *conn, const char *username);
int db_create_user(PGconn *conn, const char *username, const char *password, int role);
bool db_set_user_online_status(PGconn *conn, int user_id, bool is_online);
bool db_update_user_channel(PGconn *conn, int user_id, const char *channel);
bool db_set_user_role(PGconn *conn, int user_id, int role);
User db_get_user(PGconn *conn, int user_id);
User db_get_user_by_name(PGconn *conn, const char *username);

// Channel operations
int db_find_channel_by_name(PGconn *conn, const char *channel_name);
int db_create_channel(PGconn *conn, const char *name, const char *type, int creator_id);
bool db_delete_channel(PGconn *conn, const char *name);
Channel *db_get_all_channels(PGconn *conn, int *count);

// Message operations
int db_store_message(PGconn *conn, int user_id, int channel_id, const char *content);
Message *db_get_channel_messages(PGconn *conn, int channel_id, int *count);
bool db_delete_message(PGconn *conn, int message_id);

// Reaction operations
bool db_add_reaction(PGconn *conn, int user_id, int message_id, const char *emoji);
Reaction *db_get_message_reactions(PGconn *conn, int message_id, int *count);

// Mute operations
bool db_mute_user(PGconn *conn, int user_id, int channel_id, int minutes);
bool db_is_user_muted(PGconn *conn, int user_id, int channel_id);

#endif // DB_H