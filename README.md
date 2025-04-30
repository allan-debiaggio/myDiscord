# MyDiscord - Discord-like Chat Application

A C-based real-time chat application similar to Discord, built using PostgreSQL and GTK.

## Prerequisites

- C compiler (GCC recommended)
- PostgreSQL
- GTK3 development libraries
- libpq (PostgreSQL client library)
- OpenSSL development libraries
- libbcrypt (for password hashing)

### Installation on macOS

```bash
# Install dependencies using Homebrew
brew install postgresql
brew install gtk+3
brew install openssl
brew install libbcrypt
```

### Installation on Linux (Ubuntu/Debian)

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y build-essential libgtk-3-dev postgresql postgresql-contrib libpq-dev libssl-dev libbcrypt-dev
```

## Setting Up

1. Clone the repository
```bash
git clone https://github.com/yourusername/mydiscord.git
cd mydiscord
```

2. Set up the database
```bash
make setup_db
```

This will:
- Create a test database called "mydiscord_test"
- Set up the database schema
- Insert test data

## Compilation

Compile the project using:
```bash
make
```

This will build both the server and client applications.

## Running the Test

To run a quick test of the application:
```bash
make test
```

This will start the server in the background and launch the client.

## Manual Testing

1. Start the server:
```bash
./mydiscord-server
```

2. In a new terminal, start the client:
```bash
./mydiscord-client
```

3. Use these test credentials:
   - Username: testuser
   - Password: password

## Features Being Tested

- Database connection
- Server-client socket communication
- User login
- Real-time message sending/receiving
- Basic UI functionality

## Troubleshooting

- If you encounter GTK warnings, you may need to update your GTK installation
- Database connection issues: Make sure PostgreSQL is running with `brew services start postgresql` or `sudo service postgresql start`
- On macOS, if you encounter library loading issues, you may need to set DYLD_LIBRARY_PATH to include the PostgreSQL lib directory 