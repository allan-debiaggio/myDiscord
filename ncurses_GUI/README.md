# MY DISPUTE - Discord-like TUI

A terminal-based chat interface with Discord-like functionality, built with ncurses in C.

## Features

- User authentication (registration and login)
- Channel-based messaging
- Private messaging between users
- User roles (regular user, moderator, administrator)
- Message reactions with emojis
- Moderation features (muting users)
- Cyberpunk styling with neon colors

## Requirements

- C compiler (GCC recommended)
- ncurses library

## Building

```
make
```

## Usage

Run the application:

```
./my_dispute
```

### Account Creation

When starting the application, you have two options:
1. Create a new user account
2. Login to an existing account

For new accounts:
- Username must be at least 3 characters
- Password must be at least 8 characters, include 1 uppercase letter and 1 special character

### Commands

Once logged in, the following commands are available:

- `/msg channel_name message_text` - Send a message to a specific channel
- `/pm username message_text` - Send a private message to a user
- `/mute username minutes` - (Moderator+) Mute a user for specified minutes
- `/create channel_name` - (Admin only) Create a new channel
- `/delete channel_name` - (Admin only) Delete a channel
- `/setrole username role` - (Admin only) Set a user's role (1=user, 2=moderator, 3=admin)

### Navigation

- Arrow keys to navigate between channels and users
- F10 to exit the application

## User Roles

1. User - Can send messages and private messages
2. Moderator - Can mute users on channels
3. Administrator - Can create/delete channels and manage user roles

## License

MIT