#!/bin/bash

# PostgreSQL connection info
PG_USER="postgres"
PG_PASS="postgres"
DB_NAME="mydispute"

# Check if psql is installed
if ! command -v psql &> /dev/null; then
    echo "Error: PostgreSQL client (psql) is not installed."
    exit 1
fi

# Create database if it doesn't exist
echo "Creating database $DB_NAME if it doesn't exist..."
PGPASSWORD="$PG_PASS" psql -U "$PG_USER" -h localhost -c "CREATE DATABASE $DB_NAME;" 2>/dev/null

if [ $? -eq 0 ]; then
    echo "Database created successfully."
else
    echo "Database already exists or couldn't be created."
fi

# Import schema
echo "Creating database tables..."
PGPASSWORD="$PG_PASS" psql -U "$PG_USER" -h localhost -d "$DB_NAME" -c "
-- Create tables for the chat messenger

-- Users table
CREATE TABLE IF NOT EXISTS users (
    user_id SERIAL PRIMARY KEY,
    username VARCHAR(32) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    email VARCHAR(100),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_login TIMESTAMP,
    is_active BOOLEAN DEFAULT TRUE
);

-- Channels table
CREATE TABLE IF NOT EXISTS channels (
    channel_id SERIAL PRIMARY KEY,
    name VARCHAR(50) NOT NULL,
    description TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    created_by INTEGER REFERENCES users(user_id),
    is_public BOOLEAN DEFAULT TRUE
);

-- Channel members table
CREATE TABLE IF NOT EXISTS channel_members (
    channel_id INTEGER REFERENCES channels(channel_id) ON DELETE CASCADE,
    user_id INTEGER REFERENCES users(user_id) ON DELETE CASCADE,
    joined_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    is_admin BOOLEAN DEFAULT FALSE,
    PRIMARY KEY (channel_id, user_id)
);

-- Messages table
CREATE TABLE IF NOT EXISTS messages (
    message_id SERIAL PRIMARY KEY,
    channel_id INTEGER REFERENCES channels(channel_id) ON DELETE CASCADE,
    user_id INTEGER REFERENCES users(user_id) ON DELETE SET NULL,
    content TEXT NOT NULL,
    sent_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    edited_at TIMESTAMP,
    is_deleted BOOLEAN DEFAULT FALSE
);

-- Create a default general channel
INSERT INTO channels (name, description, is_public)
VALUES ('general', 'General discussion channel', TRUE)
ON CONFLICT DO NOTHING;

-- Create indexes
CREATE INDEX IF NOT EXISTS idx_messages_channel_id ON messages(channel_id);
CREATE INDEX IF NOT EXISTS idx_messages_user_id ON messages(user_id);
CREATE INDEX IF NOT EXISTS idx_messages_sent_at ON messages(sent_at);
CREATE INDEX IF NOT EXISTS idx_channel_members_user_id ON channel_members(user_id);
"

if [ $? -eq 0 ]; then
    echo "Schema created successfully."
else
    echo "Error creating schema."
    exit 1
fi

# Create a default admin user (username: admin, password: admin)
# The hash is a simple one since we're using a simplified hashing function
echo "Creating default admin user..."
PGPASSWORD="$PG_PASS" psql -U "$PG_USER" -h localhost -d "$DB_NAME" -c "
INSERT INTO users (username, password_hash, email, is_active) 
VALUES ('admin', '92cfceb39d57d914ed8b14d0e37643de0797ae56', 'admin@example.com', true)
ON CONFLICT (username) DO NOTHING;
"

if [ $? -eq 0 ]; then
    echo "Default user created successfully (username: admin, password: admin)."
else
    echo "Error creating default user."
    exit 1
fi

echo "Database setup completed successfully." 