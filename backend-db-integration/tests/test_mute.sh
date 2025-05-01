#!/bin/bash

echo "Testing muting functionality"
echo "============================"

echo "1. Testing mod muting a user"
echo "mod" > mod_input.txt
echo "password" >> mod_input.txt
echo "/mute user1 5" >> mod_input.txt
echo "/muted" >> mod_input.txt
echo "/quit" >> mod_input.txt

./mydiscord_client < mod_input.txt > mod_output.txt

echo "2. Testing if user1 can send messages"
echo "user1" > tests/temp_data/user1_input.txt
echo "password" >> tests/temp_data/user1_input.txt
echo "This is a test message that should be blocked" >> tests/temp_data/user1_input.txt
echo "/quit" >> tests/temp_data/user1_input.txt

./mydiscord_client < tests/temp_data/user1_input.txt > tests/temp_data/user1_output.txt

echo "3. Testing unmuting user1"
echo "mod" > unmute_input.txt
echo "password" >> unmute_input.txt
echo "/unmute user1" >> unmute_input.txt
echo "/muted" >> unmute_input.txt
echo "/quit" >> unmute_input.txt

./mydiscord_client < unmute_input.txt > unmute_output.txt

echo "4. Testing if user1 can send messages after unmuting"
echo "user1" > tests/temp_data/user1_after_input.txt
echo "password" >> tests/temp_data/user1_after_input.txt
echo "This message should go through after unmuting" >> tests/temp_data/user1_after_input.txt
echo "/quit" >> tests/temp_data/user1_after_input.txt

./mydiscord_client < tests/temp_data/user1_after_input.txt > tests/temp_data/user1_after_output.txt

echo "Results:"
echo "========"
echo "Mod muting output:"
grep -a "muted" mod_output.txt

echo "User1 attempt to send message while muted:"
grep -a "muted" tests/temp_data/user1_output.txt

echo "Mod unmuting output:"
grep -a "unmuted" unmute_output.txt

echo "User1 attempt to send message after unmuting:"
grep -a "successfully" tests/temp_data/user1_after_output.txt 