#include "my_dispute.h"

// Selected user index for UI navigation
static int selected_online_user_index = 0;

int set_user_role(AppState *state, char *username, int role)
{
  // Validate role
  if (role != ROLE_USER && role != ROLE_MODERATOR && role != ROLE_ADMIN)
  {
    return 0;
  }

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

  // If user not found
  if (user_index == -1)
  {
    return 0;
  }

  // Set the user's role
  state->users[user_index].role = role;

  // Add system message to current channel about the role change
  Channel *channel = &state->channels[state->current_channel_index];
  if (channel->message_count < MAX_MESSAGES)
  {
    Message *msg = &channel->messages[channel->message_count];
    strcpy(msg->sender, "SYSTEM");

    const char *role_str;
    if (role == ROLE_ADMIN)
    {
      role_str = "Administrator";
    }
    else if (role == ROLE_MODERATOR)
    {
      role_str = "Moderator";
    }
    else
    {
      role_str = "User";
    }

    sprintf(msg->text, "%s's role has been set to %s by %s",
            username, role_str, state->users[state->current_user_index].username);
    msg->timestamp = time(NULL);
    channel->message_count++;
  }

  return 1;
}

int mute_user(AppState *state, char *username, int channel_index, int minutes)
{
  // Validate channel index
  if (channel_index < 0 || channel_index >= state->channel_count)
  {
    return 0;
  }

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

  // If user not found
  if (user_index == -1)
  {
    return 0;
  }

  // Can't mute admins or moderators
  if (state->users[user_index].role >= ROLE_MODERATOR)
  {
    return 0;
  }

  // Set mute expiry time
  time_t now = time(NULL);
  state->users[user_index].muted_until[channel_index] = now + (minutes * 60);

  // Add system message to the channel about the mute
  Channel *channel = &state->channels[channel_index];
  if (channel->message_count < MAX_MESSAGES)
  {
    Message *msg = &channel->messages[channel->message_count];
    strcpy(msg->sender, "SYSTEM");

    sprintf(msg->text, "%s has been muted for %d minutes by %s",
            username, minutes, state->users[state->current_user_index].username);
    msg->timestamp = now;
    channel->message_count++;
  }

  return 1;
}

// Return the index of the currently selected online user
int get_selected_user_index(AppState *state)
{
  return selected_online_user_index;
}

// Navigate through online users
void navigate_users(AppState *state, int direction)
{
  int online_count = 0;

  // Count online users
  for (int i = 0; i < state->user_count; i++)
  {
    if (state->users[i].is_online)
    {
      online_count++;
    }
  }

  if (online_count == 0)
  {
    // No online users to navigate through
    selected_online_user_index = 0;
    return;
  }

  // Update selection
  selected_online_user_index += direction;

  // Wrap around if needed
  if (selected_online_user_index < 0)
  {
    selected_online_user_index = online_count - 1;
  }
  else if (selected_online_user_index >= online_count)
  {
    selected_online_user_index = 0;
  }
}

// Start a private message with the currently selected user
void start_pm_with_selected_user(AppState *state)
{
  int current_index = 0;
  char username[MAX_USERNAME_LEN];
  bool found = false;

  // Find the username of the selected online user
  for (int i = 0; i < state->user_count; i++)
  {
    if (state->users[i].is_online)
    {
      if (current_index == selected_online_user_index)
      {
        strcpy(username, state->users[i].username);
        found = true;
        break;
      }
      current_index++;
    }
  }

  if (!found || strcmp(username, state->users[state->current_user_index].username) == 0)
  {
    // Cannot start PM with self or if user not found
    return;
  }

  // Create PM channel name
  char pm_channel_name[MAX_CHANNEL_NAME_LEN];

  // Sort usernames alphabetically for consistent channel naming
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

  // Find or create the PM channel
  int pm_channel_index = -1;
  for (int i = 0; i < state->channel_count; i++)
  {
    if (strcmp(state->channels[i].name, pm_channel_name) == 0)
    {
      pm_channel_index = i;
      break;
    }
  }

  if (pm_channel_index == -1)
  {
    // Create new channel if it doesn't exist
    if (state->channel_count < MAX_CHANNELS)
    {
      strcpy(state->channels[state->channel_count].name, pm_channel_name);
      state->channels[state->channel_count].message_count = 0;

      // Add system message
      Message *msg = &state->channels[state->channel_count].messages[0];
      strcpy(msg->sender, "SYSTEM");
      sprintf(msg->text, "Private conversation between %s and %s",
              state->users[state->current_user_index].username, username);
      msg->timestamp = time(NULL);
      state->channels[state->channel_count].message_count = 1;

      pm_channel_index = state->channel_count;
      state->channel_count++;
    }
  }

  if (pm_channel_index != -1)
  {
    // Switch to the PM channel
    state->current_channel_index = pm_channel_index;
  }
}