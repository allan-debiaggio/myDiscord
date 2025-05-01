#!/bin/bash

# Colors for better output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}[CLEANUP]${NC} Removing unnecessary files and organizing the project..."

# Make sure logs and tests directories exist
mkdir -p logs/test_logs tests

# Stop any running servers
echo -e "${YELLOW}[CLEANUP]${NC} Stopping any running servers..."
pkill -f "mydiscord-server-fixed" || true
pkill -f "server_db_test" || true
pkill -f "mydiscord-server" || true

# Remove old executables
echo -e "${YELLOW}[CLEANUP]${NC} Removing old executables..."
rm -f server_db_test mydiscord-server-db-mock mydiscord-server-full
rm -f test_mute_client test_client client_test db_test mydiscord-test-client mydiscord-ui

# Remove old logs
echo -e "${YELLOW}[CLEANUP]${NC} Cleaning up old log files..."
rm -f server_test_debug.log server_fixed.log server_direct.log server_debug.log server_test.log server_output.log server_log.txt server_full.log
rm -f client1.log client2.log client_output.log
rm -f db_message_check.log db_message_output.log db_user_check.log db_newuser_output.log db_user_output.log db_admin_output.log db_newuser_check.log db_channel_check.log
rm -f client1_output.log client2_output.log pm_receiver.log pm_sender.log
rm -f mod_output.log admin_output.log member_output.log
rm -f test_client1_input.txt test_client2_input.txt test_pm_receiver.txt test_pm_sender.txt

# Move test-related files to tests directory
echo -e "${YELLOW}[CLEANUP]${NC} Moving test files to tests directory..."

# Check if files exist before attempting to move them
if [ -f "test_mute_users.sh" ]; then
    mv test_mute_users.sh tests/ || true
fi

if [ -f "test_mute_functionality.sh" ]; then
    mv test_mute_functionality.sh tests/ || true
fi

if [ -f "test_db_integration.sh" ]; then
    mv test_db_integration.sh tests/ || true
fi

if [ -f "test_broadcast_auto.sh" ]; then
    mv test_broadcast_auto.sh tests/ || true
fi

if [ -f "test_client.c" ]; then
    mv test_client.c tests/ || true
fi

if [ -f "test_mute_client.c" ]; then
    mv test_mute_client.c tests/ || true
fi

if [ -f "client_test.c" ]; then
    mv client_test.c tests/ || true
fi

if [ -f "db_test.c" ]; then
    mv db_test.c tests/ || true
fi

if [ -f "server_db_test.c" ]; then
    mv server_db_test.c tests/ || true
fi

if [ -f "run_test.sh" ]; then
    mv run_test.sh tests/ || true
fi

if [ -f "Makefile.test" ]; then
    mv Makefile.test tests/ || true
fi

# Move root test scripts to tests directory
if [ -f "test_all_functionality.sh" ]; then
    mv test_all_functionality.sh tests/ || true
fi

if [ -f "test_broadcasting.sh" ]; then
    mv test_broadcasting.sh tests/ || true
fi

# Clean up DSYMs
echo -e "${YELLOW}[CLEANUP]${NC} Removing debug symbol files..."
rm -rf mydiscord-server-fixed.dSYM mydiscord-server-full.dSYM

# Move SQL files to database directory
echo -e "${YELLOW}[CLEANUP]${NC} Organizing database files..."
mkdir -p database
mv -f test_data.sql database/ 2>/dev/null || true
mv -f myDiscord.sql database/ 2>/dev/null || true

# Make sure the core files are kept and executable
echo -e "${YELLOW}[CLEANUP]${NC} Ensuring core files are executable..."
chmod +x restart_server.sh tests/*.sh init_db.sh

echo -e "${GREEN}[CLEANUP]${NC} Project directory structure cleaned up successfully!"
echo -e "${YELLOW}[INFO]${NC} Main components:"
echo -e "  - Server source: ${GREEN}src/server_mock_db_fixed.c${NC}"
echo -e "  - Client source: ${GREEN}mydiscord_client.c${NC}"
echo -e "  - Database scripts: ${GREEN}database/${NC}"
echo -e "  - Tests: ${GREEN}tests/${NC}"
echo -e "  - Logs: ${GREEN}logs/${NC}"
echo -e "${YELLOW}[INFO]${NC} Use 'make' to build the project"
echo -e "${YELLOW}[INFO]${NC} Use './restart_server.sh' to start the server"
echo -e "${YELLOW}[INFO]${NC} Use 'make test' to run tests" 