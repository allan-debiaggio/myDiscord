#!/bin/bash

# Database connection parameters
DB_NAME="mydiscord30"
DB_USER="postgres"
DB_PASSWORD="postgres"
DB_HOST="localhost"

echo "Initializing MyDiscord database..."

# Check if PostgreSQL is running
if ! psql -U $DB_USER -h $DB_HOST -c "SELECT 1" > /dev/null 2>&1; then
  echo "Error: PostgreSQL server is not running or credentials are incorrect."
  exit 1
fi

# Drop database if it exists
echo "Dropping existing database if it exists..."
psql -U $DB_USER -h $DB_HOST -c "DROP DATABASE IF EXISTS $DB_NAME" 

# Create database
echo "Creating database: $DB_NAME..."
psql -U $DB_USER -h $DB_HOST -c "CREATE DATABASE $DB_NAME"

# Create tables
echo "Creating tables..."
psql -U $DB_USER -h $DB_HOST -d $DB_NAME << EOF
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(64) NOT NULL UNIQUE,
    password VARCHAR(64) NOT NULL,
    role INTEGER NOT NULL DEFAULT 1,
    is_online BOOLEAN DEFAULT false,
    current_channel VARCHAR(32) DEFAULT 'general'
);

CREATE TABLE channels (
    id SERIAL PRIMARY KEY,
    name VARCHAR(32) NOT NULL UNIQUE,
    type VARCHAR(10) CHECK (type IN ('public', 'private')) NOT NULL,
    created_by INTEGER REFERENCES users(id)
);

CREATE TABLE messages (
    id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users(id),
    channel_id INTEGER REFERENCES channels(id),
    content TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE reactions (
    id SERIAL PRIMARY KEY,
    message_id INTEGER REFERENCES messages(id),
    user_id INTEGER REFERENCES users(id),
    emoji VARCHAR(8) NOT NULL
);

CREATE TABLE muted_users (
    user_id INTEGER REFERENCES users(id),
    channel_id INTEGER REFERENCES channels(id),
    end_time TIMESTAMP NOT NULL,
    PRIMARY KEY (user_id, channel_id)
);
EOF

# Insert test data
echo "Inserting test data..."
psql -U $DB_USER -h $DB_HOST -d $DB_NAME << EOF
-- Insert admin user
INSERT INTO users (username, password, role, is_online) 
VALUES ('admin', 'adminpass', 3, false);

-- Insert regular users
INSERT INTO users (username, password, role, is_online) 
VALUES ('testuser', 'password123', 1, false);

INSERT INTO users (username, password, role, is_online) 
VALUES ('moderator', 'modpass', 2, false);

-- Create default channels
INSERT INTO channels (name, type, created_by) 
VALUES ('general', 'public', 1);

INSERT INTO channels (name, type, created_by) 
VALUES ('random', 'public', 1);

-- Add test messages
INSERT INTO messages (user_id, channel_id, content)
VALUES (1, 1, 'Welcome to the MyDiscord server!');

INSERT INTO messages (user_id, channel_id, content)
VALUES (2, 1, 'Hello everyone!');
EOF

echo "Database initialization completed successfully!"
echo "Database: $DB_NAME is ready to use." 