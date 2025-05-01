#!/bin/bash

# Colors for better output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}[TEST]${NC} Starting message broadcasting test"

# Make sure server is running
if ! pgrep -f mydiscord-server-fixed > /dev/null; then
    echo -e "${YELLOW}[INFO]${NC} Server not running, starting it now..."
    ./restart_server.sh
    sleep 2
else
    echo -e "${GREEN}[INFO]${NC} Server is already running"
fi

# Open two client terminals
echo -e "${YELLOW}[INFO]${NC} Opening two client terminals. Please log in with different users in each window."
echo -e "${YELLOW}[INFO]${NC} Send messages to the same channel to test broadcasting."
echo -e "${YELLOW}[INFO]${NC} Use the /pm command to test private messaging."

# On macOS
if [[ "$OSTYPE" == "darwin"* ]]; then
    # Check if iTerm2 is available
    if [ -d "/Applications/iTerm.app" ]; then
        echo -e "${GREEN}[INFO]${NC} Opening terminals with iTerm2"
        osascript -e 'tell application "iTerm"
            create window with default profile
            tell current window
                tell current session
                    write text "cd \"'"$PWD"'\" && ./mydiscord_client"
                end tell
            end tell
            create window with default profile
            tell current window
                tell current session
                    write text "cd \"'"$PWD"'\" && ./mydiscord_client"
                end tell
            end tell
        end tell'
    else
        echo -e "${GREEN}[INFO]${NC} Opening terminals with Terminal.app"
        osascript -e 'tell application "Terminal"
            do script "cd \"'"$PWD"'\" && ./mydiscord_client"
            do script "cd \"'"$PWD"'\" && ./mydiscord_client"
        end tell'
    fi
# Linux
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    if command -v gnome-terminal &> /dev/null; then
        echo -e "${GREEN}[INFO]${NC} Opening terminals with gnome-terminal"
        gnome-terminal -- bash -c "cd \"$PWD\" && ./mydiscord_client; exec bash"
        gnome-terminal -- bash -c "cd \"$PWD\" && ./mydiscord_client; exec bash"
    elif command -v xterm &> /dev/null; then
        echo -e "${GREEN}[INFO]${NC} Opening terminals with xterm"
        xterm -e "cd \"$PWD\" && ./mydiscord_client" &
        xterm -e "cd \"$PWD\" && ./mydiscord_client" &
    else
        echo -e "${RED}[ERROR]${NC} No suitable terminal emulator found"
        exit 1
    fi
else
    echo -e "${RED}[ERROR]${NC} Unsupported operating system"
    exit 1
fi

echo -e "${GREEN}[TEST]${NC} Test setup complete. Please use the clients to test broadcasting."
echo -e "${YELLOW}[INFO]${NC} Instructions for testing:"
echo -e " 1. Log in as different users in each window (e.g., user1 and user2)"
echo -e " 2. Make sure both clients are in the same channel (e.g., 'general')"
echo -e " 3. Send a message from one client and verify it appears in the other"
echo -e " 4. Try /pm <username> <message> to test private messaging"
echo
echo -e "${YELLOW}[INFO]${NC} When finished, use /quit in both clients to exit"
echo -e "${YELLOW}[INFO]${NC} To stop the server: pkill -f mydiscord-server-fixed" 