#!/bin/bash

# Stop any running server
echo "Stopping any running servers..."
pkill -f "mydiscord-server-fixed" || true

# Recompile
echo "Recompiling server..."
cc -Wall -g -I/opt/homebrew/opt/libpq/include -L/opt/homebrew/opt/libpq/lib -DUSE_DATABASE -o mydiscord-server-fixed src/server_mock_db_fixed.c -lpq -pthread

# Check compilation status
if [ $? -ne 0 ]; then
    echo "Compilation failed. Exiting."
    exit 1
fi

# Create logs directory if it doesn't exist
mkdir -p logs

# Start server
echo "Starting server..."
./mydiscord-server-fixed --use-database --port 8082 > logs/server.log 2>&1 &
SERVER_PID=$!
echo "Server started with PID: $SERVER_PID"
echo "Logs are being written to logs/server.log"

# Wait a moment and show logs
sleep 2
echo "Server logs:"
tail -n 20 logs/server.log

echo ""
echo "Server is running. To stop it, use: pkill -f mydiscord-server-fixed" 