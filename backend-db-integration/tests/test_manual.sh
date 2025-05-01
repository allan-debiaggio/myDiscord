#!/bin/bash

# Colors for better output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if server is running
if ! pgrep -f mydiscord-server-fixed > /dev/null; then
    echo -e "${RED}Server is not running. Starting server...${NC}"
    ./restart_server.sh
    sleep 2
else
    echo -e "${GREEN}Server is already running${NC}"
fi

echo -e "\n${YELLOW}Manual Testing Instructions for MyDiscord Channel Switching${NC}"
echo "=================================================================="
echo -e "\n${GREEN}Test Steps:${NC}"
echo "1. Open two terminal windows."
echo
echo -e "${YELLOW}In Terminal 1:${NC}"
echo "------------------------"
echo "$ ./mydiscord_client"
echo "Username: user1"
echo "Password: password"
echo "> /channel random"
echo "> This is a test message in random channel"
echo "> (wait for user2 to join)"
echo "> /quit"
echo
echo -e "${YELLOW}In Terminal 2:${NC}"
echo "------------------------"
echo "$ ./mydiscord_client"
echo "Username: user2"
echo "Password: password"
echo "> /channel random"
echo "> (check if you can see user1's message in history)"
echo "> /quit"
echo
echo -e "${YELLOW}Expected Results:${NC}"
echo "---------------"
echo "1. User1 should be able to switch to the random channel"
echo "2. User1 should be able to send a message in the random channel"
echo "3. User2 should be able to switch to the random channel"
echo "4. User2 should see User1's message history in the random channel"
echo
echo -e "${GREEN}Server logs will be available at: logs/server.log${NC}"
echo "You can check them with: tail -n 50 logs/server.log"
echo
echo -e "${RED}Happy testing!${NC}" 