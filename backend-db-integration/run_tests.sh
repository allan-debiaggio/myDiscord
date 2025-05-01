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
