#!/bin/bash

# Colors for better output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}[TEST]${NC} Starting automated message broadcasting test"

# Make sure logs directories exist
mkdir -p ../logs/test_logs

# Make sure server is running
if ! pgrep -f mydiscord-server-fixed > /dev/null; then
    echo -e "${YELLOW}[INFO]${NC} Server not running, starting it now..."
    ../../restart_server.sh
    sleep 5  # Longer delay for server startup
else
    echo -e "${GREEN}[INFO]${NC} Server is already running"
fi

# Kill any existing clients
pkill -f mydiscord_client 2>/dev/null
sleep 2  # Make sure all clients are terminated

# Make sure the log file has initial content we can differentiate
TIMESTAMP=$(date +%s)
echo "[MARKER] Test starting at timestamp $TIMESTAMP" >> ../logs/server.log

# Test channel broadcast
echo -e "${YELLOW}[INFO]${NC} Testing channel broadcasting..."

# Create test files for the clients
cat > ../logs/test_logs/test_client1_input.txt << EOL
user1
password
This is a test message from user1 at $TIMESTAMP
/quit
EOL

cat > ../logs/test_logs/test_client2_input.txt << EOL
user2
password
/channel general
This is a test message from user2 at $TIMESTAMP
/quit
EOL

# Run client 1 with test input
echo -e "${GREEN}[INFO]${NC} Starting client 1 (user1)"
../../mydiscord_client < ../logs/test_logs/test_client1_input.txt > ../logs/test_logs/client1_output.log 2>&1 &
CLIENT1_PID=$!

# Wait longer for client 1 to connect and log in
sleep 5

# Run client 2 with test input
echo -e "${GREEN}[INFO]${NC} Starting client 2 (user2)"
../../mydiscord_client < ../logs/test_logs/test_client2_input.txt > ../logs/test_logs/client2_output.log 2>&1 &
CLIENT2_PID=$!

# Wait longer for clients to finish
echo -e "${YELLOW}[INFO]${NC} Waiting for clients to finish..."
sleep 10

# Force terminate any lingering clients
pkill -f mydiscord_client 2>/dev/null
sleep 1

# Check the logs for broadcast messages
echo -e "${GREEN}[INFO]${NC} Checking logs for broadcast messages"

# Check if client2 received user1's message (look for the timestamp which is unique)
if grep -q "user1.*$TIMESTAMP" ../logs/test_logs/client2_output.log; then
    echo -e "${GREEN}[PASS]${NC} Client 2 received messages from user1"
else
    echo -e "${RED}[FAIL]${NC} Client 2 did NOT receive messages from user1"
    echo -e "${YELLOW}[INFO]${NC} Client 2 log content:"
    grep -a "user1\|@\|Command\|Server" ../logs/test_logs/client2_output.log || echo "No relevant content"
fi

# Check if client1 received user2's message
if grep -q "user2.*$TIMESTAMP" ../logs/test_logs/client1_output.log; then
    echo -e "${GREEN}[PASS]${NC} Client 1 received messages from user2"
else
    echo -e "${RED}[FAIL]${NC} Client 1 did NOT receive messages from user2"
    echo -e "${YELLOW}[INFO]${NC} Client 1 log content:"
    grep -a "user2\|@\|Command\|Server" ../logs/test_logs/client1_output.log || echo "No relevant content"
fi

# Check server log for broadcast indicators
if grep -q "Broadcasted message.*$TIMESTAMP" ../logs/server.log; then
    echo -e "${GREEN}[PASS]${NC} Server successfully broadcasted messages"
else
    echo -e "${RED}[FAIL]${NC} Server did not broadcast messages with expected content"
    echo -e "${YELLOW}[INFO]${NC} Server broadcast log content:"
    grep -a "Broadcasted message" ../logs/server.log | tail -5
fi

# Add another timestamp marker
TIMESTAMP_PM=$(date +%s)
echo "[MARKER] Private message test starting at timestamp $TIMESTAMP_PM" >> ../logs/server.log

# Testing private messaging
echo -e "\n${YELLOW}[INFO]${NC} Testing private messaging..."

# Create test files for PM testing
cat > ../logs/test_logs/test_pm_sender.txt << EOL
user1
password
/pm user2 Private message from user1 at $TIMESTAMP_PM
/quit
EOL

cat > ../logs/test_logs/test_pm_receiver.txt << EOL
user2
password
/pm user1 Private message from user2 at $TIMESTAMP_PM
/quit
EOL

# Run PM sender 
echo -e "${GREEN}[INFO]${NC} Starting private message sender (user1)"
../../mydiscord_client < ../logs/test_logs/test_pm_sender.txt > ../logs/test_logs/pm_sender.log 2>&1 &
SENDER_PID=$!

# Wait longer
sleep 5

echo -e "${GREEN}[INFO]${NC} Starting private message receiver (user2)"
../../mydiscord_client < ../logs/test_logs/test_pm_receiver.txt > ../logs/test_logs/pm_receiver.log 2>&1 &
RECEIVER_PID=$!

# Wait longer
echo -e "${YELLOW}[INFO]${NC} Waiting for clients to finish..."
sleep 10

# Force terminate any lingering clients
pkill -f mydiscord_client 2>/dev/null
sleep 1

# Check logs for private messages
echo -e "${GREEN}[INFO]${NC} Checking logs for private messages"

# Check if receiver got the private message
if grep -q "Private.*user1.*$TIMESTAMP_PM" ../logs/test_logs/pm_receiver.log; then
    echo -e "${GREEN}[PASS]${NC} Receiver got private message from user1"
else
    echo -e "${RED}[FAIL]${NC} Receiver did NOT get private message from user1"
    echo -e "${YELLOW}[INFO]${NC} Receiver log content:"
    grep -a "Private\|user1\|Command\|Server" ../logs/test_logs/pm_receiver.log || echo "No relevant content"
fi

# Check if sender got the private message
if grep -q "Private.*user2.*$TIMESTAMP_PM" ../logs/test_logs/pm_sender.log; then
    echo -e "${GREEN}[PASS]${NC} Sender got private message from user2"
else
    echo -e "${RED}[FAIL]${NC} Sender did NOT get private message from user2"
    echo -e "${YELLOW}[INFO]${NC} Sender log content:"
    grep -a "Private\|user2\|Command\|Server" ../logs/test_logs/pm_sender.log || echo "No relevant content"
fi

# Check server log for PM delivery
if grep -q "Delivered private message.*$TIMESTAMP_PM" ../logs/server.log; then
    echo -e "${GREEN}[PASS]${NC} Server successfully delivered private messages"
else
    echo -e "${RED}[FAIL]${NC} Server did not deliver private messages with expected content"
    echo -e "${YELLOW}[INFO]${NC} Server private message log content:"
    grep -a "private message\|Private" ../logs/server.log | tail -5
fi

echo -e "\n${GREEN}[TEST]${NC} Automated broadcast test completed" 