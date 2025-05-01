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
rm -f tests/temp_data/user1_channel.txt tests/temp_data/user1_output.txt tests/temp_data/user2_channel.txt tests/temp_data/user2_output.txt

# Check if server is running
if ! pgrep -f mydiscord-server-fixed > /dev/null; then
    echo -e "${RED}Server is not running. Starting server...${NC}"
    ./restart_server.sh
    sleep 5 # Give the server time to start
else
    echo -e "${GREEN}Server is already running${NC}"
fi

# Make sure the code is up to date
echo -e "${YELLOW}Recompiling client and server...${NC}"
make
sleep 2

echo -e "${YELLOW}1. User1 connects and logs in${NC}"
echo "user1" > tests/temp_data/user1_login.txt
echo "password" >> tests/temp_data/user1_login.txt
./mydiscord_client < tests/temp_data/user1_login.txt > tests/temp_data/user1_login_output.txt

sleep 3

echo -e "${YELLOW}2. User1 switches to random channel${NC}"
echo "/channel random" > tests/temp_data/user1_channel.txt
./mydiscord_client < tests/temp_data/user1_channel.txt > tests/temp_data/user1_channel_output.txt

sleep 3

echo -e "${YELLOW}3. User1 sends a test message${NC}"
echo "TEST-MESSAGE-FROM-USER1" > tests/temp_data/user1_message.txt
./mydiscord_client < tests/temp_data/user1_message.txt > tests/temp_data/user1_message_output.txt

sleep 5

echo -e "${YELLOW}4. User2 connects and switches to random channel${NC}"
echo "user2" > tests/temp_data/user2_login.txt
echo "password" >> tests/temp_data/user2_login.txt
echo "/channel random" >> tests/temp_data/user2_login.txt
./mydiscord_client < tests/temp_data/user2_login.txt > tests/temp_data/user2_output.txt

sleep 2

# Check test results
echo -e "\n${YELLOW}Test Results:${NC}"
echo "============="

# Check if User1 switched channels
if grep -q "Switching to channel: random" tests/temp_data/user1_channel_output.txt; then
  echo -e "${GREEN}[PASS]${NC} User1 successfully switched to random channel"
else
  echo -e "${RED}[FAIL]${NC} User1 failed to switch to random channel"
  echo -e "${YELLOW}User1 channel output:${NC}"
  cat tests/temp_data/user1_channel_output.txt
fi

# Check if User2 switched channels
if grep -q "Switching to channel: random" tests/temp_data/user2_output.txt; then
  echo -e "${GREEN}[PASS]${NC} User2 successfully switched to random channel"
else
  echo -e "${RED}[FAIL]${NC} User2 failed to switch to random channel"
  echo -e "${YELLOW}User2 output:${NC}"
  cat tests/temp_data/user2_output.txt
fi

# Check if User2 received User1's message in the channel history
if grep -q "TEST-MESSAGE-FROM-USER1" tests/temp_data/user2_output.txt; then
  echo -e "${GREEN}[PASS]${NC} User2 received User1's message history in the random channel"
else
  echo -e "${RED}[FAIL]${NC} User2 did not receive User1's message history"
  echo -e "${YELLOW}User2 output:${NC}"
  cat tests/temp_data/user2_output.txt
fi

# Show server logs
echo -e "\n${YELLOW}Recent server logs:${NC}"
tail -30 logs/server.log

# Clean up
echo -e "\n${YELLOW}Cleaning up...${NC}"
# Comment out the removal to keep files for debugging
# rm -f user1_*.txt user2_*.txt

echo -e "${GREEN}Test completed${NC}" 