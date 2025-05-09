# My Dispute - Discord-like Terminal Interface

This is a terminal-based Discord-like interface implemented in C using ncurses. It provides a UI for channel-based messaging, private messaging, user roles, and moderation features.

## Features

- Multi-user chat with channels
- Private messaging
- User authentication and registration
- Roles-based permission system
- Moderation tools (mute users)
- Channel management

## Project Structure

```
.
├── src/
│   ├── client/      # Client-side source files
│   ├── server/      # Server-side source files
│   └── shared/      # Shared source files
├── include/         # Header files
├── assets/          # ASCII art and other assets
├── build/           # Compiled binaries and object files
├── logs/            # Log files (optional)
├── .env             # Environment variables
├── .gitignore
├── Dockerfile
├── Makefile
└── README.md
```

## Environment Variables

You can configure the server IP and port using a `.env` file:

```
SERVER_PORT=8888
SERVER_IP=127.0.0.1
```

## Building the Application

To build both the client and server:

```
make all
```

Binaries will be placed in the `build/` directory:
- Client: `build/my_dispute`
- Server: `build/my_dispute_server`

## Running the Application

First, start the server:

```
./build/my_dispute_server
```

Then, in another terminal, start the client:

```
./build/my_dispute
```

You can start multiple clients in separate terminals to simulate multiple users.

## Running with Docker

Build the Docker image:

```
docker build -t my_dispute .
```

Run the server in a container:

```
docker run -it --rm -p 8888:8888 my_dispute
```

You can then connect clients from your host or other containers to the server at `localhost:8888`.

## Default Channels and Users

The server creates the following default channels:
- general
- random
- help

It also creates an admin user:
- Username: admin
- Password: admin123

## Client Usage

- Arrow keys to navigate
- Enter to select
- Tab to switch focus
- /command syntax for special commands

## Server Commands

The server supports the following commands:
- /create [channel] - Create a new channel (admin only)
- /delete [channel] - Delete a channel (admin only)
- /mute [user] [minutes] - Mute a user (admin/mod only)
- /setrole [user] [1-3] - Set user role (admin only)

## Roles
1. User
2. Moderator
3. Admin

## License

MIT