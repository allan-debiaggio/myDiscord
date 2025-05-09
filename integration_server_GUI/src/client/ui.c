#include "my_dispute.h"

// ASCII art from the ascii-art.txt file
static const char *logo[] = {
    "          ",
    "                                                                 ",
    "                 #*####                 ####*#                   ",
    "            %%*#%     %%##############%%      %*+%               ",
    "          %*#   %%#*+%                  #+#%%     #*             ",
    "         %*    %%                           %%%    %#%           ",
    "         *                                          %%           ",
    "        #                                            %%          ",
    "       %#                                            %#%         ",
    "       %                                              %          ",
    "      #%                                               *         ",
    "     %#              %##              %##%             %#        ",
    "                   %     %           %    %                      ",
    "    %#%           %#     %*        %#%     ##           *%       ",
    "    %%            %#     %*        %#%     %#           %#       ",
    "    %%            %#     %#         %%     #%            #       ",
    "    #%              %###%%           %%#*#%              +       ",
    "    %%                                                   *       ",
    "    ##       %#%                               %#        *       ",
    "    %%         %***%                       %**#%         #       ",
    "     %#             %+=%%%%%%%%%%%%%%%*=+             #%        ",
    "       %+%          %#%                 *%          %*#          ",
    "          %*%%      #%                   *%      %##             ",
    "             %%*+++*%                     #+++*#%                ",
    "                                                                 ",
    "                                                                 ",
    "                                                                 "};

// These functions are defined in users.c, only declare them here to remove duplicates
extern int get_selected_user_index(AppState *state);
extern void navigate_users(AppState *state, int direction);
extern void start_pm_with_selected_user(AppState *state);

void init_ui(AppState *state)
{
  clear();
  refresh();

  int max_y, max_x;
  getmaxyx(stdscr, max_y, max_x);

  // Create windows with updated dimensions
  // Logo area (top left)
  state->logo_win = newwin(LOGO_HEIGHT, LOGO_WIDTH, 0, 0);
  box(state->logo_win, 0, 0);

  // Channel list (bottom left) - now aligned with logo width
  state->channels_win = newwin(max_y - LOGO_HEIGHT, CHANNEL_LIST_WIDTH, LOGO_HEIGHT, 0);
  box(state->channels_win, 0, 0);

  // User list (right side)
  state->users_win = newwin(max_y, USER_LIST_WIDTH, 0, max_x - USER_LIST_WIDTH);
  box(state->users_win, 0, 0);

  // Chat area (center) - adjusted to align with the new channel width
  state->chat_win = newwin(max_y - INPUT_HEIGHT, max_x - CHANNEL_LIST_WIDTH - USER_LIST_WIDTH,
                           0, CHANNEL_LIST_WIDTH);
  box(state->chat_win, 0, 0);

  // Input area (bottom center) - adjusted to align with the new channel width
  state->input_win = newwin(INPUT_HEIGHT, max_x - CHANNEL_LIST_WIDTH - USER_LIST_WIDTH,
                            max_y - INPUT_HEIGHT, CHANNEL_LIST_WIDTH);
  box(state->input_win, 0, 0);

  // Set up colors
  start_color();
  init_pair(COLOR_NEON_YELLOW, COLOR_YELLOW, COLOR_BLACK);
  init_pair(COLOR_NEON_GREEN, COLOR_GREEN, COLOR_BLACK);
  init_pair(COLOR_NEON_PINK, COLOR_MAGENTA, COLOR_BLACK);
  init_pair(COLOR_GRAY, COLOR_WHITE, COLOR_BLACK);
  init_pair(COLOR_BRIGHT_RED, COLOR_RED, COLOR_BLACK);
  init_pair(COLOR_DARK_BLUE, COLOR_BLUE, COLOR_BLACK);

  // Initial draw
  draw_logo(state->logo_win);
  draw_channels(state->channels_win, state, false);
  draw_chat(state->chat_win, state);
  draw_users(state->users_win, state, false);
  draw_input(state->input_win, true, "");

  // Refresh all windows
  wrefresh(state->logo_win);
  wrefresh(state->channels_win);
  wrefresh(state->chat_win);
  wrefresh(state->input_win);
  wrefresh(state->users_win);
}

void draw_logo(WINDOW *win)
{
  werase(win);
  box(win, 0, 0);

  int width = getmaxx(win);
  int height = getmaxy(win);

  // Draw the ASCII art from file with neon red color instead of pink
  wattron(win, COLOR_PAIR(COLOR_BRIGHT_RED) | A_BOLD);

  // Calculate available space for centering
  int num_lines = sizeof(logo) / sizeof(logo[0]);
  int start_y = (height - num_lines) / 2;
  if (start_y < 1)
    start_y = 1;

  // Draw each line of the logo, centered horizontally
  for (int i = 0; i < num_lines && i + start_y < height - 1; i++)
  {
    int line_length = strlen(logo[i]);
    int start_x = (width - line_length) / 2;
    if (start_x < 1)
      start_x = 1;

    mvwprintw(win, i + start_y, start_x, "%s", logo[i]);
  }

  wattroff(win, COLOR_PAIR(COLOR_BRIGHT_RED) | A_BOLD);

  wrefresh(win);
}

void draw_channels(WINDOW *win, AppState *state, bool has_focus)
{
  werase(win);
  box(win, 0, 0);

  int width = getmaxx(win);

  // Draw title with focus indicator
  if (has_focus)
  {
    wattron(win, COLOR_PAIR(COLOR_NEON_PINK) | A_BOLD);
    mvwprintw(win, 1, (width - 13) / 2, "< CHANNELS >");
    wattroff(win, COLOR_PAIR(COLOR_NEON_PINK) | A_BOLD);
  }
  else
  {
    wattron(win, COLOR_PAIR(COLOR_NEON_YELLOW) | A_BOLD);
    mvwprintw(win, 1, (width - 9) / 2, "CHANNELS");
    wattroff(win, COLOR_PAIR(COLOR_NEON_YELLOW) | A_BOLD);
  }

  // Draw channel list
  for (int i = 0; i < state->channel_count; i++)
  {
    if (i == state->current_channel_index)
    {
      wattron(win, COLOR_PAIR(COLOR_NEON_GREEN) | A_BOLD | (has_focus ? A_REVERSE : 0));
      mvwprintw(win, i + 3, 2, "> %s", state->channels[i].name);
      wattroff(win, COLOR_PAIR(COLOR_NEON_GREEN) | A_BOLD | (has_focus ? A_REVERSE : 0));
    }
    else
    {
      wattron(win, COLOR_PAIR(COLOR_GRAY));
      mvwprintw(win, i + 3, 2, "  %s", state->channels[i].name);
      wattroff(win, COLOR_PAIR(COLOR_GRAY));
    }
  }

  // Add navigation instructions with clearer wording
  int max_y = getmaxy(win);
  wattron(win, COLOR_PAIR(COLOR_DARK_BLUE));
  if (has_focus)
  {
    mvwprintw(win, max_y - 3, 2, "↑/↓: Navigate");
    mvwprintw(win, max_y - 2, 2, "Enter: Select");
  }
  mvwprintw(win, max_y - 1, 2, "Tab: Switch focus");
  wattroff(win, COLOR_PAIR(COLOR_DARK_BLUE));

  wrefresh(win);
}

void draw_users(WINDOW *win, AppState *state, bool has_focus)
{
  werase(win);
  box(win, 0, 0);

  int width = getmaxx(win);

  // Draw title with focus indicator
  if (has_focus)
  {
    wattron(win, COLOR_PAIR(COLOR_NEON_PINK) | A_BOLD);
    mvwprintw(win, 1, (width - 15) / 2, "< ONLINE USERS >");
    wattroff(win, COLOR_PAIR(COLOR_NEON_PINK) | A_BOLD);
  }
  else
  {
    wattron(win, COLOR_PAIR(COLOR_NEON_YELLOW) | A_BOLD);
    mvwprintw(win, 1, (width - 11) / 2, "ONLINE USERS");
    wattroff(win, COLOR_PAIR(COLOR_NEON_YELLOW) | A_BOLD);
  }

  // Track the selected user index
  int selected_user_idx = -1;
  if (has_focus && state->user_count > 0)
  {
    // Get selected user from the app state or use first online user
    selected_user_idx = get_selected_user_index(state);
  }

  // Draw online user list
  int online_count = 0;

  for (int i = 0; i < state->user_count; i++)
  {
    if (state->users[i].is_online)
    {
      // Determine if this user is selected
      bool is_selected = (has_focus && online_count == selected_user_idx);

      if (is_selected)
      {
        wattron(win, COLOR_PAIR(COLOR_NEON_GREEN) | A_BOLD | A_REVERSE);
      }
      else
      {
        wattron(win, COLOR_PAIR(COLOR_GRAY));
      }

      // Draw role indicator
      if (state->users[i].role == ROLE_ADMIN)
      {
        if (!is_selected)
        {
          wattroff(win, COLOR_PAIR(COLOR_GRAY));
          wattron(win, COLOR_PAIR(COLOR_BRIGHT_RED) | A_BOLD);
        }
        mvwprintw(win, online_count + 3, 2, "[A]");
        if (!is_selected)
        {
          wattroff(win, COLOR_PAIR(COLOR_BRIGHT_RED) | A_BOLD);
          wattron(win, COLOR_PAIR(COLOR_GRAY));
        }
        mvwprintw(win, online_count + 3, 6, "%s", state->users[i].username);
      }
      else if (state->users[i].role == ROLE_MODERATOR)
      {
        if (!is_selected)
        {
          wattroff(win, COLOR_PAIR(COLOR_GRAY));
          wattron(win, COLOR_PAIR(COLOR_NEON_GREEN) | A_BOLD);
        }
        mvwprintw(win, online_count + 3, 2, "[M]");
        if (!is_selected)
        {
          wattroff(win, COLOR_PAIR(COLOR_NEON_GREEN) | A_BOLD);
          wattron(win, COLOR_PAIR(COLOR_GRAY));
        }
        mvwprintw(win, online_count + 3, 6, "%s", state->users[i].username);
      }
      else
      {
        mvwprintw(win, online_count + 3, 2, "    %s", state->users[i].username);
      }

      if (is_selected)
      {
        wattroff(win, COLOR_PAIR(COLOR_NEON_GREEN) | A_BOLD | A_REVERSE);
      }
      else
      {
        wattroff(win, COLOR_PAIR(COLOR_GRAY));
      }

      online_count++;
    }
  }

  // If no users online
  if (online_count == 0)
  {
    wattron(win, COLOR_PAIR(COLOR_GRAY));
    mvwprintw(win, 3, 2, "  No users online");
    wattroff(win, COLOR_PAIR(COLOR_GRAY));
  }

  // Add instructions if focused with clearer wording
  if (has_focus)
  {
    int max_y = getmaxy(win);
    wattron(win, COLOR_PAIR(COLOR_DARK_BLUE));
    mvwprintw(win, max_y - 3, 2, "↑/↓: Navigate");
    mvwprintw(win, max_y - 2, 2, "Enter: Start PM");
    wattroff(win, COLOR_PAIR(COLOR_DARK_BLUE));
  }

  wrefresh(win);
}

void format_message_time(time_t timestamp, char *buffer, size_t size)
{
  struct tm *tm_info = localtime(&timestamp);
  strftime(buffer, size, "%H:%M", tm_info);
}

void draw_chat(WINDOW *win, AppState *state)
{
  werase(win);
  box(win, 0, 0);

  int width = getmaxx(win);
  int height = getmaxy(win);

  // Draw channel name as title
  if (state->current_channel_index >= 0 && state->current_channel_index < state->channel_count)
  {
    wattron(win, COLOR_PAIR(COLOR_NEON_YELLOW) | A_BOLD);
    mvwprintw(win, 1, (width - strlen(state->channels[state->current_channel_index].name) - 4) / 2,
              "# %s", state->channels[state->current_channel_index].name);
    wattroff(win, COLOR_PAIR(COLOR_NEON_YELLOW) | A_BOLD);
  }

  // Display messages for current channel
  if (state->current_channel_index >= 0 && state->current_channel_index < state->channel_count)
  {
    Channel *channel = &state->channels[state->current_channel_index];

    // Calculate how many messages we can show
    int max_messages = height - 4; // Accounting for borders and title
    int start_idx = channel->message_count > max_messages ? channel->message_count - max_messages : 0;

    for (int i = start_idx, line = 3; i < channel->message_count; i++, line++)
    {
      Message *msg = &channel->messages[i];
      char time_buffer[10];
      format_message_time(msg->timestamp, time_buffer, sizeof(time_buffer));

      // Username display
      wattron(win, COLOR_PAIR(COLOR_NEON_GREEN) | A_BOLD);
      mvwprintw(win, line, 2, "%s", msg->sender);
      wattroff(win, COLOR_PAIR(COLOR_NEON_GREEN) | A_BOLD);

      // Timestamp
      wattron(win, COLOR_PAIR(COLOR_DARK_BLUE));
      mvwprintw(win, line, 2 + strlen(msg->sender) + 1, "[%s]:", time_buffer);
      wattroff(win, COLOR_PAIR(COLOR_DARK_BLUE));

      // Message text
      wattron(win, COLOR_PAIR(COLOR_GRAY));
      mvwprintw(win, line, 2 + strlen(msg->sender) + strlen(time_buffer) + 5, "%s", msg->text);
      wattroff(win, COLOR_PAIR(COLOR_GRAY));

      // Show reactions if any
      int reaction_x = 2;
      for (int r = 0; r < MAX_REACTIONS; r++)
      {
        if (msg->reactions[r] && msg->reaction_count[r] > 0)
        {
          wattron(win, COLOR_PAIR(COLOR_NEON_PINK));
          mvwprintw(win, line + 1, reaction_x, "%c %d", msg->reactions[r], msg->reaction_count[r]);
          wattroff(win, COLOR_PAIR(COLOR_NEON_PINK));
          reaction_x += 5; // Space for emoji and count
        }
      }
    }
  }

  wrefresh(win);
}

void draw_input(WINDOW *win, bool has_focus, char *current_input)
{
  werase(win);
  box(win, 0, 0);

  // Draw input prompt with focus indicator
  if (has_focus)
  {
    wattron(win, COLOR_PAIR(COLOR_NEON_PINK) | A_BOLD);
    mvwprintw(win, 1, 2, "Enter message: ");
    wattroff(win, COLOR_PAIR(COLOR_NEON_PINK) | A_BOLD);
  }
  else
  {
    wattron(win, COLOR_PAIR(COLOR_NEON_GREEN));
    mvwprintw(win, 1, 2, "Enter message: ");
    wattroff(win, COLOR_PAIR(COLOR_NEON_GREEN));
  }

  // Display the current input text
  wattron(win, COLOR_PAIR(COLOR_GRAY));
  mvwprintw(win, 1, 16, "%s", current_input);
  wattroff(win, COLOR_PAIR(COLOR_GRAY));

  // Position cursor at the end of input
  wmove(win, 1, 16 + strlen(current_input));

  wrefresh(win);
}

void handle_input(AppState *state, char *input)
{
  if (input[0] == '/')
  {
    // Command processing
    process_command(state, input);
  }
  else
  {
    // Regular message
    send_message(state, input);
  }
}

void cleanup_ui()
{
  // Cleanup and end ncurses
  endwin();
}

void process_command(AppState *state, char *command)
{
  // Skip the leading slash
  char *cmd = command + 1;

  if (strncmp(cmd, "msg ", 4) == 0)
  {
    // Format: /msg channel_name message
    char *args = cmd + 4;
    char channel_name[MAX_CHANNEL_NAME_LEN];
    char *message_text;

    // Extract channel name (text until the first space)
    message_text = strchr(args, ' ');
    if (message_text)
    {
      // Split the string
      int name_len = message_text - args;
      strncpy(channel_name, args, name_len);
      channel_name[name_len] = '\0';
      message_text++; // Skip the space

      // Find the channel
      for (int i = 0; i < state->channel_count; i++)
      {
        if (strcmp(state->channels[i].name, channel_name) == 0)
        {
          int old_channel = state->current_channel_index;
          state->current_channel_index = i;
          send_message(state, message_text);
          state->current_channel_index = old_channel;
          return;
        }
      }
    }
  }
  else if (strncmp(cmd, "pm ", 3) == 0)
  {
    // Format: /pm username message
    char *args = cmd + 3;
    char username[MAX_USERNAME_LEN];
    char *message_text;

    // Extract username (text until the first space)
    message_text = strchr(args, ' ');
    if (message_text)
    {
      // Split the string
      int name_len = message_text - args;
      strncpy(username, args, name_len);
      username[name_len] = '\0';
      message_text++; // Skip the space

      send_private_message(state, username, message_text);
    }
  }
  else if (strncmp(cmd, "mute ", 5) == 0)
  {
    // Format: /mute username minutes
    if (state->users[state->current_user_index].role >= ROLE_MODERATOR)
    {
      char *args = cmd + 5;
      char username[MAX_USERNAME_LEN];
      int minutes = 0;

      // Extract username and minutes
      sscanf(args, "%s %d", username, &minutes);

      mute_user(state, username, state->current_channel_index, minutes);
    }
  }
  else if (strncmp(cmd, "create ", 7) == 0)
  {
    // Format: /create channel_name
    if (state->users[state->current_user_index].role == ROLE_ADMIN)
    {
      char *channel_name = cmd + 7;
      create_channel(state, channel_name);
    }
  }
  else if (strncmp(cmd, "delete ", 7) == 0)
  {
    // Format: /delete channel_name
    if (state->users[state->current_user_index].role == ROLE_ADMIN)
    {
      char *channel_name = cmd + 7;
      delete_channel(state, channel_name);
    }
  }
  else if (strncmp(cmd, "setrole ", 8) == 0)
  {
    // Format: /setrole username role
    if (state->users[state->current_user_index].role == ROLE_ADMIN)
    {
      char *args = cmd + 8;
      char username[MAX_USERNAME_LEN];
      int role = 0;

      // Extract username and role
      sscanf(args, "%s %d", username, &role);

      set_user_role(state, username, role);
    }
  }
}