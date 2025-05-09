#include "my_dispute.h"
#include "network.h"

int send_message(AppState *state, char *text)
{
  // Only send message to server (do not add locally)
  int channel_index = state->current_channel_index;
  int result = network_send_channel_message(channel_index + 1, text);
  if (result)
  {
    // Optionally log to file or status bar
  }
  else
  {
    // Optionally log to file or status bar
  }
  return 1;
}

int send_private_message(AppState *state, char *username, char *text)
{
  // Only send message to server (do not add locally)
  int result = network_send_private_message(username, text);
  if (result)
  {
    // Optionally log to file or status bar
  }
  else
  {
    // Optionally log to file or status bar
  }
  return 1;
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