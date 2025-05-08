#include "my_dispute.h"

int create_channel(AppState *state, char *name)
{
  // Check if we already have too many channels
  if (state->channel_count >= MAX_CHANNELS)
  {
    return 0;
  }

  // Check if the channel name is valid
  if (strlen(name) == 0 || strlen(name) >= MAX_CHANNEL_NAME_LEN)
  {
    return 0;
  }

  // Check if channel with that name already exists
  for (int i = 0; i < state->channel_count; i++)
  {
    if (strcmp(state->channels[i].name, name) == 0)
    {
      return 0;
    }
  }

  // Create the new channel
  strcpy(state->channels[state->channel_count].name, name);
  state->channels[state->channel_count].message_count = 0;

  // Add a system message to the channel
  Message *msg = &state->channels[state->channel_count].messages[0];
  strcpy(msg->sender, "SYSTEM");
  sprintf(msg->text, "Channel '%s' created by %s", name,
          state->users[state->current_user_index].username);
  msg->timestamp = time(NULL);
  state->channels[state->channel_count].message_count = 1;

  // Increment channel count
  state->channel_count++;

  return 1;
}

int delete_channel(AppState *state, char *name)
{
  int channel_index = -1;

  // Find channel with the given name
  for (int i = 0; i < state->channel_count; i++)
  {
    if (strcmp(state->channels[i].name, name) == 0)
    {
      channel_index = i;
      break;
    }
  }

  // If channel not found
  if (channel_index == -1)
  {
    return 0;
  }

  // Don't allow deletion of default channels (first 3)
  if (channel_index < 3)
  {
    return 0;
  }

  // Move all channels after this one up one slot
  for (int i = channel_index; i < state->channel_count - 1; i++)
  {
    strcpy(state->channels[i].name, state->channels[i + 1].name);
    memcpy(&state->channels[i].messages, &state->channels[i + 1].messages,
           sizeof(Message) * MAX_MESSAGES);
    state->channels[i].message_count = state->channels[i + 1].message_count;
  }

  // Decrement channel count
  state->channel_count--;

  // If current channel was deleted, move to the general channel
  if (state->current_channel_index == channel_index)
  {
    state->current_channel_index = 0;
  }
  else if (state->current_channel_index > channel_index)
  {
    // If current channel was after the deleted one, adjust the index
    state->current_channel_index--;
  }

  return 1;
}

int join_channel(AppState *state, int channel_index)
{
  // Validate channel index
  if (channel_index < 0 || channel_index >= state->channel_count)
  {
    return 0;
  }

  // Switch to the new channel
  state->current_channel_index = channel_index;

  return 1;
}