#include "my_dispute.h"
#include "network.h"

int create_channel(AppState *state, char *name)
{
  // Check if the channel already exists locally
  for (int i = 0; i < state->channel_count; i++)
  {
    if (strcmp(state->channels[i].name, name) == 0)
    {
      // Channel already exists
      return 0;
    }
  }

  // Check if we have space for a new channel
  if (state->channel_count >= MAX_CHANNELS)
  {
    return 0;
  }

  // Try to create channel on the server first
  int network_result = network_create_channel(name);

  // Add channel locally
  strncpy(state->channels[state->channel_count].name, name, MAX_CHANNEL_NAME_LEN - 1);
  state->channels[state->channel_count].message_count = 0;

  // Create welcome message
  Message *welcome = &state->channels[state->channel_count].messages[0];
  strncpy(welcome->sender, "SYSTEM", MAX_USERNAME_LEN - 1);

  if (network_result)
  {
    snprintf(welcome->text, MAX_MESSAGE_LEN, "Channel '%s' created and synchronized with server", name);
    fprintf(stderr, "Channel created on server successfully\n");
  }
  else
  {
    snprintf(welcome->text, MAX_MESSAGE_LEN, "Channel '%s' created locally (offline mode)", name);
    fprintf(stderr, "Warning: Failed to create channel on server. Channel exists locally only.\n");
  }

  welcome->timestamp = time(NULL);

  state->channels[state->channel_count].message_count = 1;
  state->channel_count++;

  return 1;
}

int delete_channel(AppState *state, char *name)
{
  // Find the channel index
  int channel_idx = -1;
  for (int i = 0; i < state->channel_count; i++)
  {
    if (strcmp(state->channels[i].name, name) == 0)
    {
      channel_idx = i;
      break;
    }
  }

  if (channel_idx == -1)
  {
    // Channel not found
    return 0;
  }

  // Don't allow deleting default channels (first 3)
  if (channel_idx < 3)
  {
    return 0;
  }

  // Try to delete the channel on the server
  int network_result = network_delete_channel(name);
  if (network_result)
  {
    fprintf(stderr, "Channel deleted on server successfully\n");
  }
  else
  {
    fprintf(stderr, "Warning: Failed to delete channel on server. Channel removed locally only.\n");
  }

  // Shift channels to remove the deleted one
  for (int i = channel_idx; i < state->channel_count - 1; i++)
  {
    memcpy(&state->channels[i], &state->channels[i + 1], sizeof(Channel));
  }

  state->channel_count--;

  // If current channel was deleted, switch to general
  if (state->current_channel_index == channel_idx)
  {
    state->current_channel_index = 0; // Switch to first channel (general)
  }
  else if (state->current_channel_index > channel_idx)
  {
    // Adjust current channel index if it was after the deleted one
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

  // Refresh messages from server for this channel if needed
  // This is a placeholder - the actual implementation would depend on how
  // you want to handle channel subscriptions with the server

  return 1;
}