#include "my_dispute.h"

int send_message(AppState *state, char *text)
{
  // Check if the user is muted in this channel
  time_t now = time(NULL);
  if (state->users[state->current_user_index].muted_until[state->current_channel_index] > now)
  {
    // User is muted, don't allow sending message
    Channel *channel = &state->channels[state->current_channel_index];
    if (channel->message_count < MAX_MESSAGES)
    {
      Message *msg = &channel->messages[channel->message_count];
      strcpy(msg->sender, "SYSTEM");
      sprintf(msg->text, "You are muted in this channel and cannot send messages");
      msg->timestamp = now;
      channel->message_count++;
    }
    return 0;
  }

  // Check if text is empty
  if (strlen(text) == 0)
  {
    return 0;
  }

  // Get the current channel
  Channel *channel = &state->channels[state->current_channel_index];

  // Check if there's room for a new message
  if (channel->message_count >= MAX_MESSAGES)
  {
    // If full, remove oldest message (shift all messages)
    for (int i = 0; i < MAX_MESSAGES - 1; i++)
    {
      memcpy(&channel->messages[i], &channel->messages[i + 1], sizeof(Message));
    }
    channel->message_count--;
  }

  // Add the new message
  Message *msg = &channel->messages[channel->message_count];
  strcpy(msg->sender, state->users[state->current_user_index].username);
  strncpy(msg->text, text, MAX_MESSAGE_LEN - 1);
  msg->text[MAX_MESSAGE_LEN - 1] = '\0'; // Ensure null termination
  msg->timestamp = time(NULL);

  // Clear reactions
  memset(msg->reactions, 0, MAX_REACTIONS);
  memset(msg->reaction_count, 0, sizeof(int) * MAX_REACTIONS);

  // Increment message count
  channel->message_count++;

  return 1;
}

int send_private_message(AppState *state, char *username, char *text)
{
  // Find user with given username
  int user_index = -1;
  for (int i = 0; i < state->user_count; i++)
  {
    if (strcmp(state->users[i].username, username) == 0)
    {
      user_index = i;
      break;
    }
  }

  // If user not found or not online
  if (user_index == -1 || !state->users[user_index].is_online)
  {
    // Notify the sender
    Channel *channel = &state->channels[state->current_channel_index];
    if (channel->message_count < MAX_MESSAGES)
    {
      Message *msg = &channel->messages[channel->message_count];
      strcpy(msg->sender, "SYSTEM");
      sprintf(msg->text, "User '%s' is not online or doesn't exist", username);
      msg->timestamp = time(NULL);
      channel->message_count++;
    }
    return 0;
  }

  // Check if we can create a private message
  // For simplicity, we'll use a special channel "PM_<username1>_<username2>"
  char pm_channel_name[MAX_CHANNEL_NAME_LEN];

  // Sort usernames alphabetically to ensure consistent channel naming
  if (strcmp(state->users[state->current_user_index].username, username) < 0)
  {
    sprintf(pm_channel_name, "PM_%s_%s",
            state->users[state->current_user_index].username, username);
  }
  else
  {
    sprintf(pm_channel_name, "PM_%s_%s",
            username, state->users[state->current_user_index].username);
  }

  // Check if PM channel already exists
  int pm_channel_index = -1;
  for (int i = 0; i < state->channel_count; i++)
  {
    if (strcmp(state->channels[i].name, pm_channel_name) == 0)
    {
      pm_channel_index = i;
      break;
    }
  }

  // If PM channel doesn't exist, create it
  if (pm_channel_index == -1)
  {
    if (state->channel_count >= MAX_CHANNELS)
    {
      // No room for new channels
      return 0;
    }

    // Create new PM channel
    strcpy(state->channels[state->channel_count].name, pm_channel_name);
    state->channels[state->channel_count].message_count = 0;

    // Add a system message to mark channel creation
    Message *msg = &state->channels[state->channel_count].messages[0];
    strcpy(msg->sender, "SYSTEM");
    sprintf(msg->text, "Private conversation between %s and %s",
            state->users[state->current_user_index].username, username);
    msg->timestamp = time(NULL);
    state->channels[state->channel_count].message_count = 1;

    pm_channel_index = state->channel_count;
    state->channel_count++;
  }

  // Switch to the PM channel
  int old_channel = state->current_channel_index;
  state->current_channel_index = pm_channel_index;

  // Send the message
  int result = send_message(state, text);

  // Switch back to the original channel
  state->current_channel_index = old_channel;

  return result;
}

int add_reaction(AppState *state, int message_index, char reaction)
{
  // Get the current channel
  Channel *channel = &state->channels[state->current_channel_index];

  // Check if message index is valid
  if (message_index < 0 || message_index >= channel->message_count)
  {
    return 0;
  }

  // Get the message
  Message *msg = &channel->messages[message_index];

  // Check if reaction already exists for this message
  for (int i = 0; i < MAX_REACTIONS; i++)
  {
    if (msg->reactions[i] == reaction)
    {
      // Increment the count for this reaction
      msg->reaction_count[i]++;
      return 1;
    }
    else if (msg->reactions[i] == 0)
    {
      // Found an empty reaction slot, add the new reaction
      msg->reactions[i] = reaction;
      msg->reaction_count[i] = 1;
      return 1;
    }
  }

  // If we get here, all reaction slots are full
  return 0;
}