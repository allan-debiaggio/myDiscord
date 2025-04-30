#include "../../include/auth.h"

// Salt for password hashing (ideally this would be different per user)
static const char *SALT = "cXBWc3RQNjY0YWpYeHNLeg==";

// Simple hash function (not secure, just for testing)
void hash_password(const char *password, char *hash_output, size_t output_size)
{
  char salted_password[BUFFER_SIZE];
  unsigned int hash = 0;

  // Create salted password
  snprintf(salted_password, sizeof(salted_password), "%s%s", password, SALT);

  // Simple hash computation
  for (size_t i = 0; i < strlen(salted_password); i++)
  {
    hash = ((hash << 5) + hash) + salted_password[i]; // djb2 hash algorithm
  }

  // Convert to hex string
  snprintf(hash_output, output_size, "%08x", hash);
}

// Verify a password against a hash
bool verify_password(const char *password, const char *hash)
{
  char calculated_hash[9]; // 8 hex chars + null terminator
  hash_password(password, calculated_hash, sizeof(calculated_hash));
  return strcmp(calculated_hash, hash) == 0;
}