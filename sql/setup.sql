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

-- Create a function to get recent messages from a channel
CREATE OR REPLACE FUNCTION get_recent_messages(
    channel_id_param INTEGER, 
    limit_param INTEGER DEFAULT 50
) 
RETURNS TABLE (
    message_id INTEGER,
    username VARCHAR(32),
    content TEXT,
    sent_at TIMESTAMP,
    edited_at TIMESTAMP,
    is_deleted BOOLEAN
) 
AS $$
BEGIN
    RETURN QUERY
    SELECT 
        m.message_id,
        u.username,
        m.content,
        m.sent_at,
        m.edited_at,
        m.is_deleted
    FROM 
        messages m
    JOIN 
        users u ON m.user_id = u.user_id
    WHERE 
        m.channel_id = channel_id_param
    ORDER BY 
        m.sent_at DESC
    LIMIT 
        limit_param;
END;
$$ LANGUAGE plpgsql; 