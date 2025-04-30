#ifndef DATABASE_H
#define DATABASE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include "common.h"

// Database connection parameters
#define DB_FILE "chat_db.dat"

// Database connection context
typedef struct
{
  FILE *file;
  bool connected;
  pthread_mutex_t db_mutex;
} DatabaseContext;

// User structure
typedef struct
{
  int user_id;
  char username[MAX_USERNAME_LEN];
  char password_hash[64];
  char email[100];
  bool is_active;
} User;

// Function prototypes
bool db_connect(DatabaseContext *db);
bool db_disconnect(DatabaseContext *db);
bool db_execute(DatabaseContext *db, const char *query);
void *db_query(DatabaseContext *db, const char *query);

// User related functions
bool db_create_user(DatabaseContext *db, const char *username, const char *password_hash, const char *email);
bool db_authenticate_user(DatabaseContext *db, const char *username, const char *password_hash);
bool db_update_user_last_login(DatabaseContext *db, const char *username);

// Channel related functions
bool db_create_channel(DatabaseContext *db, const char *name, const char *description);

// Message related functions
bool db_store_message(DatabaseContext *db, int channel_id, int user_id, const char *username, time_t timestamp, MessageType type, const char *content);
void *db_get_recent_messages(DatabaseContext *db, int channel_id, int limit, Message **out_messages);

// Message list return structure
typedef struct
{
  int count;
  Message messages[]; // Flexible array member
} MessageList;

#endif /* DATABASE_H */