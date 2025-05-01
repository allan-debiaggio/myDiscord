-- Insert roles
INSERT INTO roles (name, permissions) VALUES ('member', B'00000001');
INSERT INTO roles (name, permissions) VALUES ('moderator', B'00000011');
INSERT INTO roles (name, permissions) VALUES ('administrator', B'00000111');

-- Insert test users
INSERT INTO users (username, email, password_hash) VALUES 
('testuser', 'test@example.com', '$2a$12$tT0x5GiSSh.fVWBZ9iGIo.VH8E/orylLJXvFcRlyJSnS2M.9PjWCu');
-- Password is 'password'

-- Insert some channels
INSERT INTO channels (name, type, created_by) VALUES 
('general', 'public', 1),
('random', 'public', 1);

-- Give the test user admin role
INSERT INTO user_roles (user_id, role_id) VALUES (1, 3);

-- Insert some test messages
INSERT INTO messages (channel_id, user_id, content) VALUES 
(1, 1, 'Welcome to the general channel!'),
(1, 1, 'This is a test message'),
(2, 1, 'Welcome to the random channel!');

-- Insert message reactions
INSERT INTO reactions (message_id, user_id, emoji) VALUES 
(1, 1, '��'),
(2, 1, '😂'); 