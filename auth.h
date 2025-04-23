#ifndef AUTH_H
#define AUTH_H

#include <stdbool.h>
#include "db_connection.h"

// Authentication result structure
typedef struct
{
  bool success;
  int user_id;
  char error_message[256];
} AuthResult;

// Function declarations
AuthResult register_user(PGconn *conn, const char *first_name, const char *last_name,
                         const char *email, const char *password);
AuthResult login_user(PGconn *conn, const char *email, const char *password);
bool verify_password(const char *password, const char *hash);
char *hash_password(const char *password);

#endif // AUTH_H