#include "../../include/json_protocol.h"

// Convert Message structure to JSON string
char *message_to_json(const Message *message)
{
  struct json_object *json_obj = json_object_new_object();

  // Add message type
  json_object_object_add(json_obj, "type", json_object_new_int(message->type));

  // Add username
  json_object_object_add(json_obj, "username", json_object_new_string(message->username));

  // Add content
  json_object_object_add(json_obj, "content", json_object_new_string(message->content));

  // Add timestamp
  json_object_object_add(json_obj, "timestamp", json_object_new_int64(message->timestamp));

  // Get JSON string
  const char *json_str = json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PRETTY);

  // Copy string as the original will be freed with the json_obj
  char *result = strdup(json_str);

  // Free the JSON object
  json_object_put(json_obj);

  return result;
}

// Parse JSON string to Message structure
bool json_to_message(const char *json_str, Message *message)
{
  struct json_object *json_obj;
  struct json_object *type_obj, *username_obj, *content_obj, *timestamp_obj;

  // Parse JSON string
  json_obj = json_tokener_parse(json_str);
  if (!json_obj)
  {
    return false;
  }

  // Get message type
  if (!json_object_object_get_ex(json_obj, "type", &type_obj))
  {
    json_object_put(json_obj);
    return false;
  }
  message->type = (MessageType)json_object_get_int(type_obj);

  // Get username
  if (!json_object_object_get_ex(json_obj, "username", &username_obj))
  {
    json_object_put(json_obj);
    return false;
  }
  strncpy(message->username, json_object_get_string(username_obj), MAX_USERNAME_LEN - 1);
  message->username[MAX_USERNAME_LEN - 1] = '\0';

  // Get content
  if (!json_object_object_get_ex(json_obj, "content", &content_obj))
  {
    json_object_put(json_obj);
    return false;
  }
  strncpy(message->content, json_object_get_string(content_obj), BUFFER_SIZE - 1);
  message->content[BUFFER_SIZE - 1] = '\0';

  // Get timestamp
  if (!json_object_object_get_ex(json_obj, "timestamp", &timestamp_obj))
  {
    json_object_put(json_obj);
    return false;
  }
  message->timestamp = (time_t)json_object_get_int64(timestamp_obj);

  // Free the JSON object
  json_object_put(json_obj);

  return true;
}

// Send JSON message over socket
bool send_json_message(int socket_fd, const Message *message)
{
  // Convert message to JSON
  char *json_str = message_to_json(message);
  if (!json_str)
  {
    return false;
  }

  // Get JSON string length
  uint32_t str_len = strlen(json_str);
  uint32_t net_len = htonl(str_len);

  // Send string length first (network byte order)
  ssize_t bytes_sent = send(socket_fd, &net_len, sizeof(net_len), 0);
  if (bytes_sent != sizeof(net_len))
  {
    free(json_str);
    return false;
  }

  // Send the JSON string
  bytes_sent = send(socket_fd, json_str, str_len, 0);
  free(json_str);

  return (bytes_sent == str_len);
}

// Receive JSON message from socket
bool receive_json_message(int socket_fd, Message *message)
{
  // Receive string length first (network byte order)
  uint32_t net_len, str_len;
  ssize_t bytes_received = recv(socket_fd, &net_len, sizeof(net_len), 0);

  if (bytes_received != sizeof(net_len))
  {
    return false;
  }

  // Convert to host byte order
  str_len = ntohl(net_len);

  // Allocate buffer for JSON string
  char *json_str = malloc(str_len + 1);
  if (!json_str)
  {
    return false;
  }

  // Receive the JSON string
  bytes_received = recv(socket_fd, json_str, str_len, 0);
  if (bytes_received != str_len)
  {
    free(json_str);
    return false;
  }

  // Null-terminate the string
  json_str[str_len] = '\0';

  // Parse JSON to message
  bool success = json_to_message(json_str, message);
  free(json_str);

  return success;
}