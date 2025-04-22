#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "db_connection.h"
#include "db_operations.h"

// Global variables to store IDs for testing
int g_user_id = -1;
int g_channel_id = -1;
int g_message_id = -1;
int g_role_id = -1;

// Function to generate a unique email address
char *generate_unique_email(const char *base_name)
{
  static char email[256];
  time_t now = time(NULL);
  snprintf(email, sizeof(email), "%s.%ld@example.com", base_name, now);
  return email;
}

void test_user_operations(PGconn *conn)
{
  printf("\nTesting user operations...\n");

  // Test creating a user
  const char *first_name = "John";
  const char *last_name = "Doe";
  const char *email = generate_unique_email("john.doe");
  const char *password_hash = "hashed_password_123";

  bool success = create_user(conn, first_name, last_name, email, password_hash);
  printf("Create user: %s\n", success ? "SUCCESS" : "FAILED");

  if (success)
  {
    // Get the user_id of the created user
    const char *query = "SELECT user_id FROM USERS WHERE email = $1";
    const char *values[1] = {email};
    int lengths[1] = {strlen(email)};
    int formats[1] = {0};

    PGresult *res = PQexecParams(conn, query, 1, NULL, values, lengths, formats, 0);
    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0)
    {
      g_user_id = atoi(PQgetvalue(res, 0, 0));
      printf("Retrieved user_id: %d\n", g_user_id);

      // Test updating user status
      success = update_user_status(conn, g_user_id, "online");
      printf("Update user status: %s\n", success ? "SUCCESS" : "FAILED");

      // Test updating last login
      success = update_user_last_login(conn, g_user_id);
      printf("Update last login: %s\n", success ? "SUCCESS" : "FAILED");
    }
    PQclear(res);
  }
}

void test_channel_operations(PGconn *conn)
{
  printf("\nTesting channel operations...\n");

  if (g_user_id != -1)
  {
    // Test creating a channel
    const char *name = "general";
    const char *description = "General discussion channel";
    const char *channel_type = "text";
    bool is_private = false;

    g_channel_id = create_channel(conn, name, description, channel_type, g_user_id, is_private);
    printf("Create channel: %s (ID: %d)\n", g_channel_id != -1 ? "SUCCESS" : "FAILED", g_channel_id);

    if (g_channel_id != -1)
    {
      // Test adding channel member
      bool success = add_channel_member(conn, g_user_id, g_channel_id);
      printf("Add channel member: %s\n", success ? "SUCCESS" : "FAILED");

      // Test removing channel member
      success = remove_channel_member(conn, g_user_id, g_channel_id);
      printf("Remove channel member: %s\n", success ? "SUCCESS" : "FAILED");
    }
  }
  else
  {
    printf("Skipping channel operations: No valid user_id available\n");
  }
}

void test_message_operations(PGconn *conn)
{
  printf("\nTesting message operations...\n");

  if (g_user_id != -1 && g_channel_id != -1)
  {
    // Test creating a message
    const char *content = "Hello, this is a test message!";
    g_message_id = create_message(conn, g_user_id, g_channel_id, content);
    printf("Create message: %s (ID: %d)\n", g_message_id != -1 ? "SUCCESS" : "FAILED", g_message_id);

    if (g_message_id != -1)
    {
      // Test editing message
      const char *new_content = "This message has been edited.";
      bool success = edit_message(conn, g_message_id, new_content);
      printf("Edit message: %s\n", success ? "SUCCESS" : "FAILED");

      // Don't delete the message yet as we need it for reactions and attachments
    }
  }
  else
  {
    printf("Skipping message operations: No valid user_id or channel_id available\n");
  }
}

void test_direct_message_operations(PGconn *conn)
{
  printf("\nTesting direct message operations...\n");

  // Create another user for direct messaging
  const char *first_name = "Jane";
  const char *last_name = "Smith";
  const char *email = generate_unique_email("jane.smith");
  const char *password_hash = "hashed_password_456";

  bool success = create_user(conn, first_name, last_name, email, password_hash);
  printf("Create second user: %s\n", success ? "SUCCESS" : "FAILED");

  if (success && g_user_id != -1)
  {
    // Get the second user's ID
    const char *query = "SELECT user_id FROM USERS WHERE email = $1";
    const char *values[1] = {email};
    int lengths[1] = {strlen(email)};
    int formats[1] = {0};

    PGresult *res = PQexecParams(conn, query, 1, NULL, values, lengths, formats, 0);
    int recipient_id = -1;

    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0)
    {
      recipient_id = atoi(PQgetvalue(res, 0, 0));
      printf("Retrieved recipient_id: %d\n", recipient_id);

      // Test creating a direct message
      const char *content = "Hello, this is a private message!";
      int dm_id = create_direct_message(conn, g_user_id, recipient_id, content);
      printf("Create direct message: %s (ID: %d)\n", dm_id != -1 ? "SUCCESS" : "FAILED", dm_id);

      if (dm_id != -1)
      {
        // Test marking message as read
        success = mark_direct_message_read(conn, dm_id);
        printf("Mark direct message as read: %s\n", success ? "SUCCESS" : "FAILED");
      }
    }
    PQclear(res);
  }
  else
  {
    printf("Skipping direct message operations: No valid user_id available\n");
  }
}

void test_role_operations(PGconn *conn)
{
  printf("\nTesting role operations...\n");

  // Test creating a role
  const char *name = "moderator";
  const char *color = "#FF0000";
  int permission_level = 2;

  g_role_id = create_role(conn, name, color, permission_level);
  printf("Create role: %s (ID: %d)\n", g_role_id != -1 ? "SUCCESS" : "FAILED", g_role_id);

  if (g_role_id != -1 && g_user_id != -1 && g_channel_id != -1)
  {
    // Test assigning role to user
    bool success = assign_role_to_user(conn, g_user_id, g_role_id, g_channel_id);
    printf("Assign role to user: %s\n", success ? "SUCCESS" : "FAILED");

    // Get the user_role_id
    const char *query = "SELECT user_role_id FROM USER_ROLES WHERE user_id = $1 AND role_id = $2 AND channel_id = $3";
    const char *values[3] = {NULL, NULL, NULL};
    int lengths[3] = {0, 0, 0};
    int formats[3] = {1, 1, 1};

    char user_id_str[20], role_id_str[20], channel_id_str[20];
    snprintf(user_id_str, sizeof(user_id_str), "%d", g_user_id);
    snprintf(role_id_str, sizeof(role_id_str), "%d", g_role_id);
    snprintf(channel_id_str, sizeof(channel_id_str), "%d", g_channel_id);
    values[0] = user_id_str;
    values[1] = role_id_str;
    values[2] = channel_id_str;
    lengths[0] = strlen(user_id_str);
    lengths[1] = strlen(role_id_str);
    lengths[2] = strlen(channel_id_str);

    PGresult *res = PQexecParams(conn, query, 3, NULL, values, lengths, formats, 0);
    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0)
    {
      int user_role_id = atoi(PQgetvalue(res, 0, 0));
      // Test removing role from user
      success = remove_role_from_user(conn, user_role_id);
      printf("Remove role from user: %s\n", success ? "SUCCESS" : "FAILED");
    }
    PQclear(res);
  }
  else
  {
    printf("Skipping role assignment operations: Missing required IDs\n");
  }
}

void test_reaction_operations(PGconn *conn)
{
  printf("\nTesting reaction operations...\n");

  if (g_message_id != -1 && g_user_id != -1)
  {
    // Test adding a reaction
    const char *emoji = "👍";
    bool success = add_reaction(conn, g_message_id, g_user_id, emoji);
    printf("Add reaction: %s\n", success ? "SUCCESS" : "FAILED");

    if (success)
    {
      // Get the reaction_id
      const char *query = "SELECT reaction_id FROM REACTIONS WHERE message_id = $1 AND user_id = $2 AND emoji = $3";
      const char *values[3] = {NULL, NULL, emoji};
      int lengths[3] = {0, 0, strlen(emoji)};
      int formats[3] = {1, 1, 0};

      char message_id_str[20], user_id_str[20];
      snprintf(message_id_str, sizeof(message_id_str), "%d", g_message_id);
      snprintf(user_id_str, sizeof(user_id_str), "%d", g_user_id);
      values[0] = message_id_str;
      values[1] = user_id_str;
      lengths[0] = strlen(message_id_str);
      lengths[1] = strlen(user_id_str);

      PGresult *res = PQexecParams(conn, query, 3, NULL, values, lengths, formats, 0);
      if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0)
      {
        int reaction_id = atoi(PQgetvalue(res, 0, 0));
        // Test removing reaction
        success = remove_reaction(conn, reaction_id);
        printf("Remove reaction: %s\n", success ? "SUCCESS" : "FAILED");
      }
      PQclear(res);
    }
  }
  else
  {
    printf("Skipping reaction operations: No valid message_id or user_id available\n");
  }
}

void test_attachment_operations(PGconn *conn)
{
  printf("\nTesting attachment operations...\n");

  if (g_message_id != -1)
  {
    // Test adding an attachment
    const char *file_name = "test.txt";
    const char *file_path = "/path/to/test.txt";
    int file_size = 1024;
    const char *mime_type = "text/plain";

    int attachment_id = add_attachment(conn, g_message_id, file_name, file_path, file_size, mime_type);
    printf("Add attachment: %s (ID: %d)\n", attachment_id != -1 ? "SUCCESS" : "FAILED", attachment_id);

    // Clean up attachments
    if (attachment_id != -1)
    {
      const char *cleanup_query = "DELETE FROM ATTACHMENTS WHERE message_id = $1";
      const char *values[1] = {NULL};
      int lengths[1] = {0};
      int formats[1] = {0};

      char message_id_str[20];
      snprintf(message_id_str, sizeof(message_id_str), "%d", g_message_id);
      values[0] = message_id_str;
      lengths[0] = strlen(message_id_str);

      PGresult *res = PQexecParams(conn, cleanup_query, 1, NULL, values, lengths, formats, 0);
      bool cleanup_success = (PQresultStatus(res) == PGRES_COMMAND_OK);
      printf("Clean up attachments: %s\n", cleanup_success ? "SUCCESS" : "FAILED");
      PQclear(res);
    }

    // Clean up reactions
    const char *cleanup_reactions_query = "DELETE FROM REACTIONS WHERE message_id = $1";
    const char *reaction_values[1] = {NULL};
    int reaction_lengths[1] = {0};
    int reaction_formats[1] = {0};

    char message_id_str[20];
    snprintf(message_id_str, sizeof(message_id_str), "%d", g_message_id);
    reaction_values[0] = message_id_str;
    reaction_lengths[0] = strlen(message_id_str);

    PGresult *res = PQexecParams(conn, cleanup_reactions_query, 1, NULL, reaction_values, reaction_lengths, reaction_formats, 0);
    bool cleanup_success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    printf("Clean up reactions: %s\n", cleanup_success ? "SUCCESS" : "FAILED");
    PQclear(res);

    // Now we can safely delete the message
    if (g_message_id != -1)
    {
      bool success = delete_message(conn, g_message_id);
      printf("Delete message: %s\n", success ? "SUCCESS" : "FAILED");
    }
  }
  else
  {
    printf("Skipping attachment operations: No valid message_id available\n");
  }
}

int main()
{
  PGconn *conn = connect_to_database();
  if (conn == NULL)
  {
    printf("Failed to connect to database\n");
    return 1;
  }

  printf("Successfully connected to database\n");

  // Run all tests in sequence, with proper dependencies
  test_user_operations(conn);
  test_channel_operations(conn);
  test_message_operations(conn);
  test_direct_message_operations(conn);
  test_role_operations(conn);
  test_reaction_operations(conn);
  test_attachment_operations(conn);

  close_database_connection(conn);
  printf("\nAll tests completed\n");

  return 0;
}