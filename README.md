# Discord-like Chat Messenger

A real-time chat messenger application developed in C with GTK interface and PostgreSQL integration.

## Features (Planned)

- TCP socket-based communication
- Multiple concurrent client connections
- GTK-based user interface with dark theme
- PostgreSQL database for message persistence
- User authentication system
- JSON-based communication protocol

## Building

```bash
make
```

## Running

### Server
```bash
./server
```

### Client
```bash
./client
```

## Project Structure

- `src/` - Source code
  - `server/` - Server implementation
  - `client/` - Client implementation
  - `common/` - Shared functionality
  - `db/` - Database interaction
- `include/` - Header files
- `assets/` - Resources for the GTK interface
- `sql/` - SQL scripts for database setup 