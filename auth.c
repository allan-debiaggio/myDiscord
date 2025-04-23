#include "auth.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/sha.h>
#include <openssl/rand.h>

// Helper function to generate a random salt
static void generate_salt(char *salt, size_t salt_size)
{
  unsigned char random_bytes[salt_size];
  RAND_bytes(random_bytes, salt_size);

  for (size_t i = 0; i < salt_size; i++)
  {
    sprintf(salt + (i * 2), "%02x", random_bytes[i]);
  }
  salt[salt_size * 2] = '\0';
}

// Hash a password with SHA-256 and salt
char *hash_password(const char *password)
{
  char salt[33]; // 16 bytes of random data in hex
  generate_salt(salt, 16);

  // Combine password and salt
  char salted_password[256];
  snprintf(salted_password, sizeof(salted_password), "%s%s", password, salt);

  // Calculate SHA-256 hash
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256((unsigned char *)salted_password, strlen(salted_password), hash);

  // Convert hash to hex string
  char *result = malloc(97); // 64 for hash + 32 for salt + 1 for null terminator
  char *ptr = result;

  // Add hash
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
  {
    sprintf(ptr, "%02x", hash[i]);
    ptr += 2;
  }

  // Add salt
  strcpy(ptr, salt);

  return result;
}

// Verify a password against a stored hash
bool verify_password(const char *password, const char *hash)
{
  // Extract salt from the stored hash (last 32 characters)
  char salt[33];
  strncpy(salt, hash + 64, 32);
  salt[32] = '\0';

  // Combine password and salt
  char salted_password[256];
  snprintf(salted_password, sizeof(salted_password), "%s%s", password, salt);

  // Calculate SHA-256 hash
  unsigned char new_hash[SHA256_DIGEST_LENGTH];
  SHA256((unsigned char *)salted_password, strlen(salted_password), new_hash);

  // Convert new hash to hex string
  char new_hash_str[65];
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
  {
    sprintf(new_hash_str + (i * 2), "%02x", new_hash[i]);
  }
  new_hash_str[64] = '\0';

  // Compare hashes
  return strncmp(new_hash_str, hash, 64) == 0;
}

// Register a new user
AuthResult register_user(PGconn *conn, const char *first_name, const char *last_name,
                         const char *email, const char *password)
{
  AuthResult result = {false, -1, ""};

  // Check if email already exists
  const char *check_query = "SELECT user_id FROM USERS WHERE email = $1";
  const char *check_values[1] = {email};
  int check_lengths[1] = {strlen(email)};
  int check_formats[1] = {0};

  PGresult *check_res = PQexecParams(conn, check_query, 1, NULL, check_values,
                                     check_lengths, check_formats, 0);

  if (PQresultStatus(check_res) == PGRES_TUPLES_OK && PQntuples(check_res) > 0)
  {
    strcpy(result.error_message, "Email already exists");
    PQclear(check_res);
    return result;
  }
  PQclear(check_res);

  // Hash the password
  char *password_hash = hash_password(password);

  // Create the user
  const char *query = "INSERT INTO USERS (first_name, last_name, email, password_hash) "
                      "VALUES ($1, $2, $3, $4) RETURNING user_id";
  const char *values[4] = {first_name, last_name, email, password_hash};
  int lengths[4] = {strlen(first_name), strlen(last_name), strlen(email), strlen(password_hash)};
  int formats[4] = {0, 0, 0, 0};

  PGresult *res = PQexecParams(conn, query, 4, NULL, values, lengths, formats, 0);

  if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0)
  {
    result.success = true;
    result.user_id = atoi(PQgetvalue(res, 0, 0));
  }
  else
  {
    strcpy(result.error_message, PQerrorMessage(conn));
  }

  free(password_hash);
  PQclear(res);
  return result;
}

// Update user's last login time
static void update_user_last_login(PGconn *conn, int user_id)
{
  const char *query = "UPDATE USERS SET last_login = CURRENT_TIMESTAMP WHERE user_id = $1";
  const char *values[1] = {NULL};
  char user_id_str[20];
  snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
  values[0] = user_id_str;
  int lengths[1] = {strlen(user_id_str)};
  int formats[1] = {0};

  PGresult *res = PQexecParams(conn, query, 1, NULL, values, lengths, formats, 0);
  PQclear(res);
}

// Login a user
AuthResult login_user(PGconn *conn, const char *email, const char *password)
{
  AuthResult result = {false, -1, ""};

  // Get user's password hash
  const char *query = "SELECT user_id, password_hash FROM USERS WHERE email = $1";
  const char *values[1] = {email};
  int lengths[1] = {strlen(email)};
  int formats[1] = {0};

  PGresult *res = PQexecParams(conn, query, 1, NULL, values, lengths, formats, 0);

  if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0)
  {
    int user_id = atoi(PQgetvalue(res, 0, 0));
    const char *stored_hash = PQgetvalue(res, 0, 1);

    if (verify_password(password, stored_hash))
    {
      result.success = true;
      result.user_id = user_id;

      // Update last login time
      update_user_last_login(conn, user_id);
    }
    else
    {
      strcpy(result.error_message, "Invalid password");
    }
  }
  else
  {
    strcpy(result.error_message, "User not found");
  }

  PQclear(res);
  return result;
}