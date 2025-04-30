#ifndef JSON_PROTOCOL_H
#define JSON_PROTOCOL_H

#include <json-c/json.h>
#include "common.h"

// Convert Message structure to JSON string
char *message_to_json(const Message *message);

// Parse JSON string to Message structure
bool json_to_message(const char *json_str, Message *message);

// Send JSON message over socket
bool send_json_message(int socket_fd, const Message *message);

// Receive JSON message from socket
bool receive_json_message(int socket_fd, Message *message);

#endif /* JSON_PROTOCOL_H */