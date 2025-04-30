#include "../../include/server.h"
#include "../../include/database.h" // Re-enabling database integration
#include "../../include/common.h"   // For SERVER_PORT constant

int main(int argc, char *argv[])
{
  // Initialize server context
  ServerContext server;
  DatabaseContext db;
  int port = SERVER_PORT;

  // Process command line arguments if any
  if (argc > 1)
  {
    port = atoi(argv[1]);
    if (port <= 0 || port > 65535)
    {
      printf("Invalid port number. Using default port %d\n", SERVER_PORT);
      port = SERVER_PORT;
    }
  }

  // Initialize server with mutex, channels, and clients
  memset(&server, 0, sizeof(ServerContext));
  pthread_mutex_init(&server.clients_mutex, NULL);
  pthread_mutex_init(&server.channels_mutex, NULL);

  // Initialize client array
  for (int i = 0; i < MAX_CLIENTS; i++)
  {
    server.clients[i].socket = 0;
    server.clients[i].active = false;
    server.clients[i].current_channel_id = 1; // Default to General channel (ID 1)
  }

  // Initialize channels array
  for (int i = 0; i < MAX_CHANNELS; i++)
  {
    server.channels[i].active = false;
  }
  server.channel_count = 0;

  // Connect to database
  if (db_connect(&db))
  {
    log_message("Successfully connected to PostgreSQL database");

    // Set database context in server
    server.db = &db;

    // Create the tables if they don't exist yet
    bool tables_created = db_execute(&db,
                                     "CREATE TABLE IF NOT EXISTS users ("
                                     "user_id SERIAL PRIMARY KEY,"
                                     "username VARCHAR(32) UNIQUE NOT NULL,"
                                     "password_hash VARCHAR(255) NOT NULL,"
                                     "email VARCHAR(100),"
                                     "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                                     "last_login TIMESTAMP,"
                                     "is_active BOOLEAN DEFAULT TRUE"
                                     ");"

                                     "CREATE TABLE IF NOT EXISTS channels ("
                                     "channel_id SERIAL PRIMARY KEY,"
                                     "name VARCHAR(50) NOT NULL,"
                                     "description TEXT,"
                                     "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                                     "created_by INTEGER REFERENCES users(user_id),"
                                     "is_public BOOLEAN DEFAULT TRUE"
                                     ");"

                                     "CREATE TABLE IF NOT EXISTS messages ("
                                     "message_id SERIAL PRIMARY KEY,"
                                     "channel_id INTEGER REFERENCES channels(channel_id) ON DELETE CASCADE,"
                                     "user_id INTEGER REFERENCES users(user_id) ON DELETE SET NULL,"
                                     "content TEXT NOT NULL,"
                                     "sent_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                                     "edited_at TIMESTAMP,"
                                     "is_deleted BOOLEAN DEFAULT FALSE"
                                     ");"

                                     "INSERT INTO channels (name, description, is_public) "
                                     "VALUES ('general', 'General discussion channel', TRUE) "
                                     "ON CONFLICT DO NOTHING;");

    if (tables_created)
    {
      log_message("Database tables created successfully");
    }
    else
    {
      log_message("Failed to create database tables");
    }
  }
  else
  {
    log_message("Failed to connect to database. Continuing without database functionality.");
    server.db = NULL;
  }

  // Initialize server socket
  if (initialize_server(&server, port) != 0)
  {
    log_message("Failed to initialize server");
    return 1;
  }

  // Create default "General" channel
  create_channel(&server, "General", "Default public channel", true);

  // Run server (will block until server is stopped)
  run_server(&server);

  // Clean up server resources
  cleanup_server(&server);

  // Disconnect from database
  if (db.connected)
  {
    db_disconnect(&db);
  }

  return 0;
}