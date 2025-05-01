#!/bin/bash

# Colors for better output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}Cleaning up project structure...${NC}"

# Create directories if they don't exist
mkdir -p tests/temp_data

# Move test scripts to tests directory
echo -e "${GREEN}Moving test scripts to tests directory...${NC}"
test_scripts=(
    "test_channel_broadcast.sh"
    "test_channel.sh"
    "test_manual.sh"
    "test_mute.sh"
)

for script in "${test_scripts[@]}"; do
    if [ -f "$script" ]; then
        cp "$script" "tests/$script"
        echo "Moved $script to tests/"
        # Remove the original file after copying
        rm "$script"
        echo "Removed original $script from root directory"
    fi
done

# Save temp data for tests if needed
echo -e "${GREEN}Saving temporary test data to tests/temp_data/...${NC}"
temp_files=(
    "user1_login.txt"
    "user1_login_output.txt"
    "user1_channel.txt"
    "user1_channel_output.txt"
    "user1_message.txt"
    "user1_message_output.txt"
    "user2_login.txt"
    "user2_output.txt"
    "user1.log"
    "user2.log"
)

for file in "${temp_files[@]}"; do
    if [ -f "$file" ]; then
        cp "$file" "tests/temp_data/$file"
        echo "Saved $file to tests/temp_data/"
    fi
done

# Remove temporary .txt files
echo -e "${GREEN}Removing temporary .txt files from root directory...${NC}"
txt_files=(
    "user1_login.txt"
    "user1_login_output.txt"
    "user1_channel.txt" 
    "user1_channel_output.txt"
    "user1_message.txt"
    "user1_message_output.txt"
    "user2_login.txt"
    "user2_output.txt"
    "user1_after_output.txt"
    "user1_after_input.txt"
    "unmute_output.txt"
    "unmute_input.txt"
    "user1_input.txt"
    "mod_output.txt"
    "mod_input.txt"
    "user1.log"
    "user2.log"
)

for file in "${txt_files[@]}"; do
    if [ -f "$file" ]; then
        rm "$file"
        echo "Removed $file"
    fi
done

# Update test scripts to use the correct paths
echo -e "${GREEN}Updating paths in test scripts...${NC}"
for script in tests/*.sh; do
    # Replace references to temp data files with tests/temp_data path
    sed -i '' 's#\(user[12]_[a-z]*\.txt\)#tests/temp_data/\1#g' "$script"
    sed -i '' 's#\(user[12]_[a-z]*_[a-z]*\.txt\)#tests/temp_data/\1#g' "$script"
    sed -i '' 's#\(user[12]\.log\)#tests/temp_data/\1#g' "$script"
    
    # Also update references to parent directory commands
    sed -i '' 's#\.\./mydiscord_client#../../mydiscord_client#g' "$script"
    sed -i '' 's#\.\./restart_server.sh#../../restart_server.sh#g' "$script"
    
    # Make scripts executable
    chmod +x "$script"
    echo "Updated paths in $script"
done

# Create a simple README in the tests directory
echo -e "${GREEN}Creating tests/README.md...${NC}"
cat > tests/README.md << 'EOF'
# MyDiscord Test Suite

This directory contains test scripts and files for testing the MyDiscord server and client.

## Test Scripts

- **test_channel_broadcast.sh**: Tests channel switching and message broadcasting between users
- **test_channel.sh**: Interactive test for channel switching functionality
- **test_manual.sh**: Manual testing instructions for channel switching
- **test_mute.sh**: Tests muting and unmuting functionality
- **test_broadcasting.sh**: Tests message broadcasting functionality
- **test_all_functionality.sh**: Comprehensive test of all functionalities

## Running Tests

To run a specific test:

```bash
cd tests
./test_channel_broadcast.sh
```

Or use the wrapper script from the root directory:

```bash
./run_tests.sh test_channel_broadcast.sh
```

## Temporary Test Data

Temporary test data is stored in the `temp_data` directory.
EOF

echo -e "${YELLOW}Creating a simple wrapper script for running tests...${NC}"
cat > run_tests.sh << 'EOF'
#!/bin/bash

# Colors for better output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

if [ $# -eq 0 ]; then
    echo -e "${YELLOW}Available tests:${NC}"
    ls -1 tests/*.sh | sed 's#tests/##'
    echo
    echo -e "Usage: $0 <test_name>"
    echo -e "Example: $0 test_channel_broadcast.sh"
    exit 1
fi

TEST_NAME=$1

if [ -f "tests/$TEST_NAME" ]; then
    echo -e "${GREEN}Running test: $TEST_NAME${NC}"
    cd tests && ./$TEST_NAME
else
    echo -e "${RED}Test not found: $TEST_NAME${NC}"
    echo -e "${YELLOW}Available tests:${NC}"
    ls -1 tests/*.sh | sed 's#tests/##'
fi
EOF

chmod +x run_tests.sh

echo -e "${GREEN}Done cleaning up project structure!${NC}"
echo "You can now run tests using: ./run_tests.sh <test_name>" 