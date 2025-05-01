#!/bin/bash

# Colors for better output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test channel switching and message broadcasting
echo -e "${YELLOW}Testing channel switching and message broadcasting${NC}"
echo "================================================="

# Clean up any existing test files
rm -f tests/temp_data/user1.log tests/temp_data/user2.log

# Check if server is running
if ! pgrep -f mydiscord-server-fixed > /dev/null; then
    echo -e "${RED}Server is not running. Starting server...${NC}"
    ./restart_server.sh
    sleep 5 # Give the server time to start
else
    echo -e "${GREEN}Server is already running${NC}"
fi

# Open terminal 1 for User1
echo -e "${YELLOW}Opening terminal for User1${NC}"
echo "Hit Enter in the User1 terminal, then:"
echo "1. Enter username: user1"
echo "2. Enter password: password"
echo "3. Type: /channel random"
echo "4. Type: TEST-MESSAGE-FROM-USER1"
echo "5. Wait for User2 to join"
echo "6. Type /quit to exit when done"

# Start User1 client in terminal
./mydiscord_client 2>&1 | tee tests/temp_data/user1.log &
USER1_PID=$!

# Wait for user1 to connect and log in
sleep 5

# Open terminal 2 for User2
echo -e "${YELLOW}Opening terminal for User2${NC}"
echo "Hit Enter in the User2 terminal, then:"
echo "1. Enter username: user2"
echo "2. Enter password: password"
echo "3. Type: /channel random"
echo "4. Check if you can see User1's message"
echo "5. Type: /quit to exit when done"

# Start User2 client in terminal
./mydiscord_client 2>&1 | tee tests/temp_data/user2.log &
USER2_PID=$!

# Wait for both processes
wait $USER1_PID
wait $USER2_PID

echo -e "\n${YELLOW}Test completed. Check tests/temp_data/user1.log and tests/temp_data/user2.log for results.${NC}" 