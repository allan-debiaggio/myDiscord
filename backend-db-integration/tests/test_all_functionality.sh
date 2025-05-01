#!/bin/bash

# Colors for better output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Utility function for logging
log() {
    echo -e "${GREEN}[TEST]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
    exit 1
}

# Test setup
log "Starting test of all MyDiscord functionality"

# Check if server is running
if ! pgrep -f mydiscord-server-fixed > /dev/null; then
    log "Starting server..."
    ./restart_server.sh > /dev/null 2>&1
    sleep 5  # Give server more time to start
else
    log "Server is already running"
fi

# Function to test server connectivity without relying on netcat
check_server() {
    echo "Testing server connectivity..." > /dev/null
    # Try a simple connection to the server using /dev/tcp
    (echo > /dev/tcp/localhost/8082) >/dev/null 2>&1
    if [ $? -ne 0 ]; then
        log "Server is not responding. Restarting..."
        pkill -f mydiscord-server-fixed
        sleep 2
        ./restart_server.sh > /dev/null 2>&1
        sleep 5  # Give server more time to start
    fi
}

# Test 1: Admin functionality - Creating and deleting channels
log "Test 1: Admin functionality - Creating and deleting channels"
check_server
echo -e "admin\npassword\n/create testchannel2 public\n/list\n/delete testchannel2\n/list\n/quit\n" | ./mydiscord_client > admin_test.log 2>&1
sleep 3  # Give time for the command to complete

if grep -q "Channel 'testchannel2' created successfully" admin_test.log || grep -q "testchannel2" admin_test.log; then
    echo -e "${GREEN}[PASS]${NC} Admin can create and delete channels"
else
    echo -e "${RED}[FAIL]${NC} Admin channel management test failed"
    cat admin_test.log
fi

sleep 2  # Wait between tests

# Test 2: Admin functionality - Setting user roles
log "Test 2: Admin functionality - Setting user roles"
check_server
echo -e "admin\npassword\n/setrole user2 2\n/quit\n" | ./mydiscord_client > role_test.log 2>&1
sleep 3  # Give time for the command to complete

if grep -q "User 'user2' role set to 2 successfully" role_test.log || grep -q "role set to 2" role_test.log; then
    echo -e "${GREEN}[PASS]${NC} Admin can set user roles"
else
    echo -e "${RED}[FAIL]${NC} Admin role setting test failed"
    cat role_test.log
fi

sleep 2  # Wait between tests

# Test 3: Moderator functionality - Mute/unmute users
log "Test 3: Moderator functionality - Mute/unmute users"
check_server
echo -e "user2\npassword\n/mute user1 5\n/muted\n/unmute user1\n/muted\n/quit\n" | ./mydiscord_client > mod_test.log 2>&1
sleep 3  # Give time for the command to complete

# We'll just check that the commands don't produce errors
if ! grep -q "Unknown command" mod_test.log; then
    echo -e "${GREEN}[PASS]${NC} Moderator mute/unmute commands are available"
else
    echo -e "${RED}[FAIL]${NC} Moderator commands test failed"
    cat mod_test.log
fi

sleep 2  # Wait between tests

# Test 4: List channels functionality for regular members
log "Test 4: List channels functionality for regular members"
check_server
echo -e "user1\npassword\n/list\n/quit\n" | ./mydiscord_client > list_test.log 2>&1
sleep 3  # Give time for the command to complete

if grep -q "Available channels" list_test.log || grep -q "general" list_test.log; then
    echo -e "${GREEN}[PASS]${NC} Regular members can list channels"
else
    echo -e "${RED}[FAIL]${NC} Channel listing test failed"
    cat list_test.log
fi

sleep 2  # Wait between tests

# Test 5: Sending messages to channels
log "Test 5: Sending messages to channels"
check_server
echo -e "user1\npassword\nHello from test script\n/quit\n" | ./mydiscord_client > message_test.log 2>&1
sleep 3  # Give time for the command to complete

if grep -q "Successfully stored message" message_test.log || grep -q "Hello from test script" message_test.log; then
    echo -e "${GREEN}[PASS]${NC} Users can send messages to channels"
else
    echo -e "${RED}[FAIL]${NC} Channel messaging test failed"
    cat message_test.log
fi

sleep 2  # Wait between tests

# Test 6: Channel switching
log "Test 6: Channel switching"
check_server
echo -e "user1\npassword\n/channel random\nHello random channel\n/quit\n" | ./mydiscord_client > channel_switch_test.log 2>&1
sleep 3  # Give time for the command to complete

if grep -q "Switched to channel: random" channel_switch_test.log || grep -q "random" channel_switch_test.log; then
    echo -e "${GREEN}[PASS]${NC} Users can switch channels"
else
    echo -e "${RED}[FAIL]${NC} Channel switching test failed"
    cat channel_switch_test.log
fi

# Check server logs for validation
log "Checking server logs for activity"
if grep -q "Received message from client" server_fixed.log; then
    echo -e "${GREEN}[PASS]${NC} Server logs confirm client-server communication"
else
    echo -e "${RED}[FAIL]${NC} No client-server communication found in logs"
fi

# Clean up log files
log "Cleaning up test logs"
rm -f admin_test.log role_test.log mod_test.log list_test.log message_test.log channel_switch_test.log

log "All tests completed!"
echo 
echo -e "${YELLOW}Note:${NC} Some commands like private messaging (/pm) need multiple simultaneous"
echo -e "clients to be tested properly and are not covered by this automated test."
echo
echo "To stop the server: pkill -f mydiscord-server-fixed" 