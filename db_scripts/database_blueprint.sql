CREATE TABLE USERS (
    user_id SERIAL PRIMARY KEY,
    first_name VARCHAR(100),
    last_name VARCHAR(100),
    email VARCHAR(150) UNIQUE,
    password_hash VARCHAR(255),
    status VARCHAR(100),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_login TIMESTAMP,
    is_active BOOLEAN DEFAULT true
);

CREATE TABLE CHANNELS (
    channel_id SERIAL PRIMARY KEY,
    name VARCHAR(100),
    description VARCHAR(255),
    channel_type VARCHAR(50),
    created_by INT REFERENCES USERS(user_id),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    is_private BOOLEAN DEFAULT false
);

CREATE TABLE MESSAGES (
    message_id SERIAL PRIMARY KEY,
    user_id INT REFERENCES USERS(user_id),
    channel_id INT REFERENCES CHANNELS(channel_id),
    content TEXT,
    sent_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    is_edited BOOLEAN DEFAULT false,
    is_encrypted BOOLEAN DEFAULT false,
    edited_at TIMESTAMP
);

CREATE TABLE REACTIONS (
    reaction_id SERIAL PRIMARY KEY,
    message_id INT REFERENCES MESSAGES(message_id),
    user_id INT REFERENCES USERS(user_id),
    emoji VARCHAR(50),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE ATTACHMENTS (
    attachment_id SERIAL PRIMARY KEY,
    message_id INT REFERENCES MESSAGES(message_id),
    file_name VARCHAR(255),
    file_path VARCHAR(255),
    file_size INT,
    mime_type VARCHAR(100),
    uploaded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE ROLES (
    role_id SERIAL PRIMARY KEY,
    name VARCHAR(100),
    color VARCHAR(50),
    permission_level INT
);

CREATE TABLE USER_ROLES (
    user_role_id SERIAL PRIMARY KEY,
    user_id INT REFERENCES USERS(user_id),
    role_id INT REFERENCES ROLES(role_id),
    channel_id INT REFERENCES CHANNELS(channel_id),
    assigned_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE CHANNEL_MEMBERS (
    member_id SERIAL PRIMARY KEY,
    user_id INT REFERENCES USERS(user_id),
    channel_id INT REFERENCES CHANNELS(channel_id),
    joined_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    notifications_enabled BOOLEAN DEFAULT true
);

CREATE TABLE DIRECT_MESSAGES (
    dm_id SERIAL PRIMARY KEY,
    sender_id INT REFERENCES USERS(user_id),
    recipient_id INT REFERENCES USERS(user_id),
    content TEXT,
    sent_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    is_read BOOLEAN DEFAULT false,
    is_encrypted BOOLEAN DEFAULT false
);
