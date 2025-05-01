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
