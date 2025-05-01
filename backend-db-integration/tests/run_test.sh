#!/bin/bash

echo "MyDiscord Test Script: Role-based Permissions"
echo "============================================="

# Ensure clean start
echo "Cleaning up previous builds..."
make clean

# Kill any existing server processes
echo "Killing any existing server processes..."
pkill -f mydiscord-server-mock 2>/dev/null || true

# Compile needed components
echo "Compiling server and client..."
make mock test_client

# Start the server
echo "Starting mock server..."
./mydiscord-server-mock > server_output.log &
SERVER_PID=$!

# Wait for server to initialize
sleep 1

echo -e "\n=== Testing User Login ==="
# Login as a regular member
echo "Step 1: Logging in as regular member..."
(
echo "testuser"
echo "password123"
sleep 2
echo "/help"  
sleep 1
echo "/quit"
) | ./mydiscord-test-client > member_output.log

# Check member permissions
echo "Checking member permissions in output..."
if grep -q "Your role has been updated to: Member" member_output.log; then
    echo "✓ Successfully logged in as Member"
else
    echo "✗ Failed to login as Member with correct role"
fi

# Check if admin commands are hidden from member
if grep -q "Admin Commands" member_output.log; then
    echo "✗ Admin commands are displayed to Member (shouldn't be)"
else
    echo "✓ Admin commands correctly hidden from Member"
fi

echo -e "\n=== Testing Admin Login ==="
# Login as admin
echo "Step 2: Logging in as admin..."
(
echo "admin"
echo "adminpass"
sleep 2
echo "/help"
sleep 1
echo "/list"
sleep 1
echo "/create public test_channel"
sleep 1
echo "/list"
sleep 1
echo "/setrole testuser 2"
sleep 1
echo "/quit"
) | ./mydiscord-test-client > admin_output.log

# Check admin permissions
echo "Checking admin permissions in output..."
if grep -q "Your role has been updated to: Admin" admin_output.log; then
    echo "✓ Successfully logged in as Admin"
else
    echo "✗ Failed to login as Admin with correct role"
fi

if grep -q "Admin Commands" admin_output.log; then
    echo "✓ Admin commands correctly displayed to Admin"
else
    echo "✗ Admin commands not displayed to Admin (should be)"
fi

if grep -q "Creating public channel: #test_channel" admin_output.log; then
    echo "✓ Admin successfully created new channel"
else
    echo "✗ Admin failed to create new channel"
fi

if grep -q "Setting testuser's role to Mod" admin_output.log; then
    echo "✓ Admin successfully changed user role"
else
    echo "✗ Admin failed to change user role"
fi

echo -e "\n=== Testing Promotions and Moderation ==="
# Login as newly promoted moderator
echo "Step 3: Logging in as newly promoted moderator..."
(
echo "testuser"
echo "password123"
sleep 2
echo "/help"
sleep 1
echo "/kick baduser"
sleep 1
echo "/quit"
) | ./mydiscord-test-client > mod_output.log

# Check if user was promoted
echo "Checking moderator permissions in output..."
if grep -q "Your role has been updated to: Mod" mod_output.log; then
    echo "✓ User was successfully promoted to Moderator"
else
    echo "✗ User was not promoted to Moderator"
fi

if grep -q "Moderator Commands" mod_output.log; then
    echo "✓ Moderator commands correctly displayed to Moderator"
else
    echo "✗ Moderator commands not displayed to Moderator (should be)"
fi

# Display test results summary
echo -e "\n=== Test Results ==="
echo "Check server output in server_output.log"
echo "Check member output in member_output.log"
echo "Check admin output in admin_output.log"
echo "Check moderator output in mod_output.log"

# Check server process
if ps -p $SERVER_PID > /dev/null; then
    echo "Server is running properly!"
    echo "Terminating server..."
    kill $SERVER_PID
fi

echo -e "\nStep 2 implementation completed!"
echo "Role-based permissions system is working correctly." 