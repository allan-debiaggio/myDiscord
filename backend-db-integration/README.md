# MyDiscord - C Chat Application with PostgreSQL Integration

MyDiscord is a client-server chat application written in C with PostgreSQL database integration for message and user management.

## Features

- **Multi-user chat system** with channel-based communication
- **User roles** (Admin, Moderator, Member, Guest) with permission levels
- **PostgreSQL database integration** for persistent storage
- **Real-time messaging** with broadcasts to all users in the channel
- **Private messaging** between users
- **Admin features**:
  - Create/delete channels
  - Set user roles
  - Mute/unmute users
- **Moderator features**:
  - Mute/unmute users
  - List muted users
- **Member features**:
  - List available channels
  - Switch between channels
  - Send messages to channels
  - Send private messages to users

## Project Structure

```
mydiscord/
├── src/                  # Server source code
│   └── server_mock_db_fixed.c  # Main server implementation
├── database/             # Database setup scripts
│   └── myDiscord.sql     # Database schema
├── logs/                 # Log files directory
│   └── server.log        # Server logs
├── tests/                # Test scripts and files
│   ├── temp_data/        # Temporary test data
│   ├── test_channel_broadcast.sh  # Channel switching test
│   ├── test_channel.sh   # Interactive channel switching test
│   ├── test_manual.sh    # Manual testing instructions
│   ├── test_mute.sh      # Muting/unmuting test
│   ├── test_all_functionality.sh  # Comprehensive test
│   ├── test_broadcasting.sh       # Message broadcasting test
│   └── test_broadcast_auto.sh     # Automated broadcasting test
├── mydiscord_client.c    # Client implementation
├── run_tests.sh          # Test runner script
├── restart_server.sh     # Script to restart the server
├── init_db.sh            # Database initialization script
├── Makefile              # Build and test targets
└── cleanup_structure.sh  # Script to clean up project directory
```

## Getting Started

### Prerequisites

- GCC or CC compiler
- PostgreSQL database
- POSIX-compliant operating system (Linux, macOS)
- libpq (PostgreSQL client library)

### Building the Project

```bash
make
```

This builds both the server and client.

### Running the Server

```bash
./restart_server.sh
```

This will compile and start the server, loading users and channels from the database.

### Running the Client

```bash
./mydiscord_client
```

Connect with one of the following accounts:
- Admin: username=`admin`, password=`password`
- Moderator: username=`mod`, password=`password`
- User: username=`user1`, password=`password`

## Testing

The project includes various test scripts in the `tests/` directory to verify functionality.

### Running Tests
You can run tests using the `run_tests.sh` script:

```bash
./run_tests.sh                     # List available tests
./run_tests.sh test_channel_broadcast.sh  # Run channel switching test
```

### Functional Testing
```bash
make test
```
This runs the comprehensive test script that verifies all functionality.

### Channel Switching Test
To test switching between channels and message broadcasting:
```bash
./run_tests.sh test_channel_broadcast.sh
```

### Manual Testing
For interactive testing with detailed instructions:
```bash
./run_tests.sh test_manual.sh
```

### Broadcasting Test (Manual)
To test real-time message broadcasting between clients manually:
```bash
make broadcast-test
```
This will open two client windows. Log in with different users and send messages to see them appear in both clients.

### Broadcasting Test (Automated)
To run an automated test of messaging and broadcasting features:
```bash
make auto-broadcast-test
```

## Available Commands

### All Users
- `/help` - Display help information
- `/quit` - Exit the client
- `/list` - List all available channels
- `/msg <channel> <text>` - Send a message to a channel
- `/pm <username> <text>` - Send a private message to a user
- `/channel <n>` - Switch to a different channel

### Moderators and Admins
- `/mute <user> <duration>` - Mute a user for specified minutes
- `/unmute <user>` - Unmute a user
- `/muted` - List all muted users in current channel

### Admins Only
- `/create <n> <type>` - Create a new channel (type: public or private)
- `/delete <n>` - Delete a channel
- `/setrole <user> <role>` - Set a user's role (0=guest, 1=member, 2=mod, 3=admin)

## Troubleshooting

If the server doesn't start:
1. Check PostgreSQL is running
2. Check database connection parameters
3. Check for port conflicts
4. Review server logs in `logs/server.log`

## Project Cleanup

If you want to clean up temporary files and organize the project structure:
```bash
./cleanup_structure.sh
```

This will:
- Move test scripts to the tests/ directory
- Save temporary test data to tests/temp_data/
- Remove temporary files from the root directory
- Update paths in test scripts
- Create a README for the tests directory
- Create a wrapper script for running tests

## License

This software is provided as-is without any warranties. 