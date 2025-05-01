#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libpq-fe.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10
#define PORT 8081

PGconn *db_conn = NULL;

// Helper function to process commands
int process_command(char *buffer, int client_sock)
{
  char response[BUFFER_SIZE];
  char cmd[32] = {0};
  char arg1[64] = {0};
  char arg2[64] = {0};

  // Parse command format: /command arg1 arg2
  if (buffer[0] == '/')
  {
    sscanf(buffer, "/%s %s %s", cmd, arg1, arg2);

    if (strcmp(cmd, "mute") == 0 && strlen(arg1) > 0)
    {
      // Format: /mute username [channel]
      const char *paramValues[3];
      char channel[64] = "general"; // Default channel

      if (strlen(arg2) > 0)
      {
        strncpy(channel, arg2, sizeof(channel) - 1);
      }

      printf("Executing /mute for user: %s in channel: %s\n", arg1, channel);

      // Get user_id for username
      paramValues[0] = arg1;
      PGresult *res = PQexecParams(db_conn,
                                   "SELECT id FROM users WHERE username = $1",
                                   1, NULL, paramValues, NULL, NULL, 0);

      if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
      {
        sprintf(response, "Error: User '%s' not found", arg1);
        send(client_sock, response, strlen(response), 0);
        PQclear(res);
        return 0;
      }

      const char *user_id = PQgetvalue(res, 0, 0);
      printf("Found user ID: %s\n", user_id);
      PQclear(res);

      // Get channel_id
      paramValues[0] = channel;
      res = PQexecParams(db_conn,
                         "SELECT id FROM channels WHERE name = $1",
                         1, NULL, paramValues, NULL, NULL, 0);

      if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
      {
        sprintf(response, "Error: Channel '%s' not found", channel);
        send(client_sock, response, strlen(response), 0);
        PQclear(res);
        return 0;
      }

      const char *channel_id = PQgetvalue(res, 0, 0);
      printf("Found channel ID: %s\n", channel_id);
      PQclear(res);

      // Make sure muted_users table exists with proper structure
      printf("Ensuring muted_users table exists with proper schema\n");
      res = PQexec(db_conn,
                   "CREATE TABLE IF NOT EXISTS muted_users ("
                   "id SERIAL PRIMARY KEY,"
                   "user_id INTEGER NOT NULL,"
                   "channel_id INTEGER NOT NULL,"
                   "muted_by INTEGER,"
                   "muted_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                   "UNIQUE(user_id, channel_id)"
                   ")");

      if (PQresultStatus(res) != PGRES_COMMAND_OK)
      {
        printf("Failed to create muted_users table: %s\n", PQerrorMessage(db_conn));
        sprintf(response, "Error: Database error while setting up muted users");
        send(client_sock, response, strlen(response), 0);
        PQclear(res);
        return 0;
      }
      PQclear(res);

      // First check if the user is already muted
      paramValues[0] = user_id;
      paramValues[1] = channel_id;

      res = PQexecParams(db_conn,
                         "SELECT id FROM muted_users WHERE user_id = $1 AND channel_id = $2",
                         2, NULL, paramValues, NULL, NULL, 0);

      if (PQresultStatus(res) != PGRES_TUPLES_OK)
      {
        printf("Error checking if user is already muted: %s\n", PQerrorMessage(db_conn));
        sprintf(response, "Error checking mute status");
        send(client_sock, response, strlen(response), 0);
        PQclear(res);
        return 0;
      }

      // If user is already muted, just return that info
      if (PQntuples(res) > 0)
      {
        printf("User is already muted\n");
        sprintf(response, "User '%s' is already muted in channel '%s'", arg1, channel);
        send(client_sock, response, strlen(response), 0);
        PQclear(res);
        return 1;
      }
      PQclear(res);

      // Add to muted_users table - using a simplified approach without foreign keys to ensure it works
      paramValues[0] = user_id;
      paramValues[1] = channel_id;
      paramValues[2] = "1"; // Muted by admin (ID 1)

      printf("Inserting muted user: user_id=%s, channel_id=%s, muted_by=1\n", user_id, channel_id);

      // Use INSERT without RETURNING since some PostgreSQL versions may not support it
      res = PQexecParams(db_conn,
                         "INSERT INTO muted_users (user_id, channel_id, muted_by) VALUES ($1, $2, $3)",
                         3, NULL, paramValues, NULL, NULL, 0);

      if (PQresultStatus(res) != PGRES_COMMAND_OK)
      {
        printf("Error muting user: %s\n", PQerrorMessage(db_conn));
        sprintf(response, "Error muting user: %s", PQerrorMessage(db_conn));
        send(client_sock, response, strlen(response), 0);
        PQclear(res);
        return 0;
      }

      printf("User successfully muted\n");
      sprintf(response, "User '%s' has been muted in channel '%s'", arg1, channel);

      PQclear(res);
      send(client_sock, response, strlen(response), 0);
      return 1;
    }
    else if (strcmp(cmd, "unmute") == 0 && strlen(arg1) > 0)
    {
      // Format: /unmute username [channel]
      const char *paramValues[2];
      char channel[64] = "general"; // Default channel

      if (strlen(arg2) > 0)
      {
        strncpy(channel, arg2, sizeof(channel) - 1);
      }

      printf("Executing /unmute for user: %s in channel: %s\n", arg1, channel);

      // Get user_id for username
      paramValues[0] = arg1;
      PGresult *res = PQexecParams(db_conn,
                                   "SELECT id FROM users WHERE username = $1",
                                   1, NULL, paramValues, NULL, NULL, 0);

      if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
      {
        sprintf(response, "Error: User '%s' not found", arg1);
        send(client_sock, response, strlen(response), 0);
        PQclear(res);
        return 0;
      }

      const char *user_id = PQgetvalue(res, 0, 0);
      printf("Found user ID: %s\n", user_id);
      PQclear(res);

      // Get channel_id
      paramValues[0] = channel;
      res = PQexecParams(db_conn,
                         "SELECT id FROM channels WHERE name = $1",
                         1, NULL, paramValues, NULL, NULL, 0);

      if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
      {
        sprintf(response, "Error: Channel '%s' not found", channel);
        send(client_sock, response, strlen(response), 0);
        PQclear(res);
        return 0;
      }

      const char *channel_id = PQgetvalue(res, 0, 0);
      printf("Found channel ID: %s\n", channel_id);
      PQclear(res);

      // Check if the user is currently muted
      paramValues[0] = user_id;
      paramValues[1] = channel_id;

      res = PQexecParams(db_conn,
                         "SELECT id FROM muted_users WHERE user_id = $1 AND channel_id = $2",
                         2, NULL, paramValues, NULL, NULL, 0);

      if (PQresultStatus(res) != PGRES_TUPLES_OK)
      {
        printf("Error checking mute status: %s\n", PQerrorMessage(db_conn));
        sprintf(response, "Error checking mute status");
        send(client_sock, response, strlen(response), 0);
        PQclear(res);
        return 0;
      }

      if (PQntuples(res) == 0)
      {
        printf("User is not muted\n");
        sprintf(response, "User '%s' is not muted in channel '%s'", arg1, channel);
        send(client_sock, response, strlen(response), 0);
        PQclear(res);
        return 1;
      }
      PQclear(res);

      // Remove from muted_users table
      printf("Removing user %s (ID: %s) from muted list in channel %s (ID: %s)\n",
             arg1, user_id, channel, channel_id);

      res = PQexecParams(db_conn,
                         "DELETE FROM muted_users WHERE user_id = $1 AND channel_id = $2",
                         2, NULL, paramValues, NULL, NULL, 0);

      if (PQresultStatus(res) != PGRES_COMMAND_OK)
      {
        printf("Error unmuting user: %s\n", PQerrorMessage(db_conn));
        sprintf(response, "Error unmuting user: %s", PQerrorMessage(db_conn));
        send(client_sock, response, strlen(response), 0);
        PQclear(res);
        return 0;
      }

      printf("User successfully unmuted\n");
      sprintf(response, "User '%s' has been unmuted in channel '%s'", arg1, channel);

      send(client_sock, response, strlen(response), 0);
      PQclear(res);
      return 1;
    }
    else if (strcmp(cmd, "list") == 0 && strcmp(arg1, "muted") == 0)
    {
      // Format: /list muted [channel]
      const char *paramValues[1];
      char channel[64] = "general"; // Default channel
      char list_buffer[BUFFER_SIZE] = "Muted users:\n";

      if (strlen(arg2) > 0)
      {
        strncpy(channel, arg2, sizeof(channel) - 1);
      }

      printf("Executing /list muted for channel: %s\n", channel);

      // Get channel_id
      paramValues[0] = channel;
      PGresult *res = PQexecParams(db_conn,
                                   "SELECT id FROM channels WHERE name = $1",
                                   1, NULL, paramValues, NULL, NULL, 0);

      if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0)
      {
        sprintf(response, "Error: Channel '%s' not found", channel);
        send(client_sock, response, strlen(response), 0);
        PQclear(res);
        return 0;
      }

      const char *channel_id = PQgetvalue(res, 0, 0);
      printf("Found channel ID: %s\n", channel_id);
      PQclear(res);

      // Simple query - just check if any muted users exist
      paramValues[0] = channel_id;
      res = PQexecParams(db_conn,
                         "SELECT COUNT(*) FROM muted_users WHERE channel_id = $1",
                         1, NULL, paramValues, NULL, NULL, 0);

      if (PQresultStatus(res) != PGRES_TUPLES_OK)
      {
        // There was an actual error with the query
        printf("Error checking muted users count: %s\n", PQerrorMessage(db_conn));
        strcat(list_buffer, "Error checking muted users.");
        PQclear(res);
        send(client_sock, list_buffer, strlen(list_buffer), 0);
        return 1;
      }

      int count = 0;
      if (PQntuples(res) > 0)
      {
        count = atoi(PQgetvalue(res, 0, 0));
      }
      printf("Found %d muted users in channel %s\n", count, channel);
      PQclear(res);

      if (count == 0)
      {
        // No muted users - return empty list message
        strcat(list_buffer, "No users are muted in this channel.");
        send(client_sock, list_buffer, strlen(list_buffer), 0);
        return 1;
      }

      // If we get here, there are muted users - try to get them
      paramValues[0] = channel_id;
      res = PQexecParams(db_conn,
                         "SELECT mu.user_id, u.username, mu.muted_at "
                         "FROM muted_users mu "
                         "JOIN users u ON mu.user_id = u.id "
                         "WHERE mu.channel_id = $1",
                         1, NULL, paramValues, NULL, NULL, 0);

      if (PQresultStatus(res) != PGRES_TUPLES_OK)
      {
        // Join failed for some reason
        printf("Join query failed: %s\n", PQerrorMessage(db_conn));

        // Fall back to simpler approach
        PQclear(res);
        res = PQexecParams(db_conn,
                           "SELECT user_id FROM muted_users WHERE channel_id = $1",
                           1, NULL, paramValues, NULL, NULL, 0);

        if (PQresultStatus(res) != PGRES_TUPLES_OK)
        {
          strcat(list_buffer, "Error retrieving muted users list.");
          PQclear(res);
          send(client_sock, list_buffer, strlen(list_buffer), 0);
          return 1;
        }

        int nrows = PQntuples(res);
        if (nrows == 0)
        {
          strcat(list_buffer, "No users are muted in this channel.");
        }
        else
        {
          for (int i = 0; i < nrows; i++)
          {
            const char *user_id = PQgetvalue(res, i, 0);

            // Look up username for this user_id
            const char *user_params[1];
            user_params[0] = user_id;

            PGresult *user_res = PQexecParams(db_conn,
                                              "SELECT username FROM users WHERE id = $1",
                                              1, NULL, user_params, NULL, NULL, 0);

            if (PQresultStatus(user_res) == PGRES_TUPLES_OK && PQntuples(user_res) > 0)
            {
              char line[256];
              sprintf(line, "- %s\n", PQgetvalue(user_res, 0, 0));
              strcat(list_buffer, line);
            }
            else
            {
              char line[256];
              sprintf(line, "- User ID %s\n", user_id);
              strcat(list_buffer, line);
            }

            PQclear(user_res);
          }
        }
      }
      else
      {
        int nrows = PQntuples(res);

        if (nrows == 0)
        {
          strcat(list_buffer, "No users are muted in this channel.");
        }
        else
        {
          for (int i = 0; i < nrows; i++)
          {
            char line[256];
            sprintf(line, "- %s\n", PQgetvalue(res, i, 1));
            strcat(list_buffer, line);
          }
        }
      }

      PQclear(res);
      send(client_sock, list_buffer, strlen(list_buffer), 0);
      return 1;
    }

    // Unknown command
    sprintf(response, "Unknown command: %s\nAvailable commands: /mute, /unmute, /list muted", cmd);
    send(client_sock, response, strlen(response), 0);
    return 1;
  }

  return 0; // Not a command
}

void cleanup()
{
  if (db_conn)
  {
    PQfinish(db_conn);
    db_conn = NULL;
  }
}

// Function to handle client connections
void *handle_client(void *socket_desc)
{
  int client_sock = *(int *)socket_desc;
  char buffer[BUFFER_SIZE];
  int read_size;

  // Get client message
  while ((read_size = recv(client_sock, buffer, sizeof(buffer), 0)) > 0)
  {
    buffer[read_size] = '\0';

    printf("Received message from client: %s\n", buffer);

    // Check if it's a command
    if (process_command(buffer, client_sock))
    {
      continue;
    }

    // Store message in database
    if (db_conn)
    {
      const char *paramValues[1];
      paramValues[0] = buffer;

      PGresult *res = PQexecParams(db_conn,
                                   "INSERT INTO messages (user_id, channel_id, content) VALUES (1, 1, $1) RETURNING id",
                                   1, NULL, paramValues, NULL, NULL, 0);

      if (PQresultStatus(res) == PGRES_TUPLES_OK)
      {
        char stored_msg[BUFFER_SIZE + 100];
        sprintf(stored_msg, "Successfully stored message (ID: %s): %s",
                PQgetvalue(res, 0, 0), buffer);
        send(client_sock, stored_msg, strlen(stored_msg), 0);
      }
      else
      {
        char error_msg[BUFFER_SIZE];
        sprintf(error_msg, "Database error: %s", PQerrorMessage(db_conn));
        send(client_sock, error_msg, strlen(error_msg), 0);
      }

      PQclear(res);
    }
    else
    {
      // Send generic response
      send(client_sock, "Message received (no database)", 29, 0);
    }
  }

  // Client disconnected
  if (read_size == 0)
  {
    printf("Client disconnected\n");
  }
  else if (read_size == -1)
  {
    perror("recv failed");
  }

  free(socket_desc);
  close(client_sock);

  return NULL;
}

int main()
{
  int server_sock, client_sock, *new_sock;
  struct sockaddr_in server, client;

  printf("Starting server with PostgreSQL integration...\n");
  fflush(stdout);

  // Connect to PostgreSQL
  db_conn = PQconnectdb("dbname=mydiscord30 user=postgres password=postgres host=localhost");

  if (PQstatus(db_conn) != CONNECTION_OK)
  {
    fprintf(stderr, "Database connection failed: %s\n", PQerrorMessage(db_conn));
    fflush(stderr);
    cleanup();
    return 1;
  }

  printf("Connected to database successfully!\n");
  fflush(stdout);

  // Set up required tables
  PGresult *res = PQexec(db_conn,
                         "CREATE TABLE IF NOT EXISTS users ("
                         "id SERIAL PRIMARY KEY,"
                         "username VARCHAR(64) UNIQUE NOT NULL,"
                         "password VARCHAR(64) NOT NULL DEFAULT 'password',"
                         "role INTEGER NOT NULL DEFAULT 1,"
                         "is_online BOOLEAN DEFAULT FALSE,"
                         "current_channel VARCHAR(32) DEFAULT 'general'"
                         ")");

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "Users table creation failed: %s\n", PQerrorMessage(db_conn));
    PQclear(res);
    cleanup();
    return 1;
  }

  PQclear(res);
  printf("Users table initialized\n");

  // Create channels table
  res = PQexec(db_conn,
               "CREATE TABLE IF NOT EXISTS channels ("
               "id SERIAL PRIMARY KEY,"
               "name VARCHAR(32) UNIQUE NOT NULL,"
               "type VARCHAR(10) CHECK (type IN ('public', 'private')), "
               "created_by INTEGER"
               ")");

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "Channels table creation failed: %s\n", PQerrorMessage(db_conn));
    PQclear(res);
    cleanup();
    return 1;
  }

  PQclear(res);
  printf("Channels table initialized\n");

  // Create messages table
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
    fprintf(stderr, "Messages table creation failed: %s\n", PQerrorMessage(db_conn));
    PQclear(res);
    cleanup();
    return 1;
  }

  PQclear(res);
  printf("Messages table initialized\n");

  // Create muted_users table
  res = PQexec(db_conn,
               "CREATE TABLE IF NOT EXISTS muted_users ("
               "id SERIAL PRIMARY KEY,"
               "user_id INTEGER REFERENCES users(id) ON DELETE CASCADE,"
               "channel_id INTEGER REFERENCES channels(id) ON DELETE CASCADE,"
               "muted_by INTEGER REFERENCES users(id) ON DELETE SET NULL,"
               "muted_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
               "UNIQUE(user_id, channel_id)"
               ")");

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "Muted_users table creation failed: %s\n", PQerrorMessage(db_conn));
    PQclear(res);
    cleanup();
    return 1;
  }

  PQclear(res);
  printf("Muted_users table initialized\n");

  // Make sure we have at least one user and channel for testing
  res = PQexec(db_conn,
               "INSERT INTO users (username, password) "
               "VALUES ('testuser', 'password') "
               "ON CONFLICT (username) DO NOTHING");
  PQclear(res);

  // Add more test users
  res = PQexec(db_conn,
               "INSERT INTO users (username, password) "
               "VALUES ('alice', 'password') "
               "ON CONFLICT (username) DO NOTHING");
  PQclear(res);

  res = PQexec(db_conn,
               "INSERT INTO users (username, password) "
               "VALUES ('bob', 'password') "
               "ON CONFLICT (username) DO NOTHING");
  PQclear(res);

  res = PQexec(db_conn,
               "INSERT INTO users (username, password) "
               "VALUES ('charlie', 'password') "
               "ON CONFLICT (username) DO NOTHING");
  PQclear(res);

  // Add more test channels
  res = PQexec(db_conn,
               "INSERT INTO channels (name, type, created_by) "
               "VALUES ('general', 'public', 1) "
               "ON CONFLICT (name) DO NOTHING");
  PQclear(res);

  res = PQexec(db_conn,
               "INSERT INTO channels (name, type, created_by) "
               "VALUES ('random', 'public', 1) "
               "ON CONFLICT (name) DO NOTHING");
  PQclear(res);

  res = PQexec(db_conn,
               "INSERT INTO channels (name, type, created_by) "
               "VALUES ('private', 'private', 1) "
               "ON CONFLICT (name) DO NOTHING");
  PQclear(res);

  // Create socket
  server_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (server_sock == -1)
  {
    perror("Socket creation failed");
    cleanup();
    return 1;
  }

  printf("Socket created successfully\n");
  fflush(stdout);

  // Prepare sockaddr_in structure
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(PORT);

  // Bind
  if (bind(server_sock, (struct sockaddr *)&server, sizeof(server)) < 0)
  {
    perror("Bind failed");
    cleanup();
    return 1;
  }

  printf("Socket bound to port %d\n", PORT);
  fflush(stdout);

  // Listen
  listen(server_sock, MAX_CLIENTS);

  printf("Server started on port %d, waiting for connections...\n", PORT);
  fflush(stdout);

  // Accept incoming connections
  socklen_t c = sizeof(struct sockaddr_in);
  while ((client_sock = accept(server_sock, (struct sockaddr *)&client, &c)))
  {
    printf("New connection accepted from %s:%d\n",
           inet_ntoa(client.sin_addr), ntohs(client.sin_port));

    // Create new thread to handle client
    new_sock = malloc(sizeof(int));
    *new_sock = client_sock;

    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, handle_client, (void *)new_sock) < 0)
    {
      perror("Thread creation failed");
      free(new_sock);
      continue;
    }

    // Detach thread
    pthread_detach(thread_id);
  }

  if (client_sock < 0)
  {
    perror("Accept failed");
    cleanup();
    return 1;
  }

  // Clean up
  cleanup();
  return 0;
}