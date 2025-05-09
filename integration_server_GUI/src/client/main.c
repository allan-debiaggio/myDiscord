#include "my_dispute.h"
#include "network.h"

AppState app_state;
int network_connected = 0;

void initialize_app()
{
  // Initialize default channels
  strcpy(app_state.channels[0].name, "general");
  strcpy(app_state.channels[1].name, "random");
  strcpy(app_state.channels[2].name, "help");
  app_state.channel_count = 3;

  // Initialize with no users (they will be added via registration)
  app_state.user_count = 0;

  // Set current indexes
  app_state.current_channel_index = 0;
  app_state.current_user_index = -1; // Not logged in yet

  // Try to establish server connection early
  if (network_connect())
  {
    network_connected = 1;
    // fprintf(stderr, "Connected to server successfully\n");
  }
  else
  {
    fprintf(stderr, "Warning: Unable to connect to server. Will attempt again after login.\n");
  }
}

void run_auth_screen()
{
  clear();

  // Initial authentication screen
  int max_y, max_x;
  getmaxyx(stdscr, max_y, max_x);

  // Create a window for the login form
  int form_height = 10;
  int form_width = 40;
  int start_y = (max_y - form_height) / 2;
  int start_x = (max_x - form_width) / 2;

  WINDOW *auth_win = newwin(form_height, form_width, start_y, start_x);
  box(auth_win, 0, 0);

  // Add cyberpunk style colors
  start_color();
  init_pair(COLOR_NEON_YELLOW, COLOR_YELLOW, COLOR_BLACK);
  init_pair(COLOR_NEON_GREEN, COLOR_GREEN, COLOR_BLACK);
  init_pair(COLOR_NEON_PINK, COLOR_MAGENTA, COLOR_BLACK);
  init_pair(COLOR_BRIGHT_RED, COLOR_RED, COLOR_BLACK);
  init_pair(COLOR_DARK_BLUE, COLOR_BLUE, COLOR_BLACK);

  // Display title
  wattron(auth_win, COLOR_PAIR(COLOR_NEON_PINK) | A_BOLD);
  mvwprintw(auth_win, 1, (form_width - 12) / 2, "MY DISPUTE");
  wattroff(auth_win, COLOR_PAIR(COLOR_NEON_PINK) | A_BOLD);

  // Display options
  wattron(auth_win, COLOR_PAIR(COLOR_NEON_GREEN));
  mvwprintw(auth_win, 4, 5, "1. New User (Register)");
  mvwprintw(auth_win, 6, 5, "2. Existing User (Login)");
  wattroff(auth_win, COLOR_PAIR(COLOR_NEON_GREEN));

  wattron(auth_win, COLOR_PAIR(COLOR_NEON_YELLOW));
  mvwprintw(auth_win, 8, 5, "Select an option (1 or 2): ");
  wattroff(auth_win, COLOR_PAIR(COLOR_NEON_YELLOW));

  wrefresh(auth_win);

  // Get user choice
  int choice = wgetch(auth_win);

  if (choice == '1')
  {
    // Register new user
    int success = register_screen();
    if (!success)
    {
      // Registration failed, return to auth screen
      delwin(auth_win);
      run_auth_screen();
      return;
    }
  }
  else if (choice == '2')
  {
    // Login existing user
    int success = login_screen();
    if (!success)
    {
      // Login failed, return to auth screen
      delwin(auth_win);
      run_auth_screen();
      return;
    }
  }
  else
  {
    // Invalid choice, redisplay auth screen
    delwin(auth_win);
    run_auth_screen();
    return;
  }

  delwin(auth_win);
}

int main(int argc, char *argv[])
{
  // Initialize ncurses
  initscr();
  cbreak();
  noecho();             // Don't echo input automatically
  keypad(stdscr, TRUE); // Enable special keys

  // Enable keypad mode for all windows
  // This allows arrow keys to be captured in each window

  // Initialize application state
  initialize_app();

  // Display authentication screen
  run_auth_screen();

  // If we reach here, user is authenticated, initialize UI
  init_ui(&app_state);

  // Enable keypad for all windows
  keypad(app_state.logo_win, TRUE);
  keypad(app_state.channels_win, TRUE);
  keypad(app_state.chat_win, TRUE);
  keypad(app_state.input_win, TRUE);
  keypad(app_state.users_win, TRUE);

  // If we're already connected, try to authenticate with the server
  // Otherwise, try to establish a fresh connection
  if (network_connected)
  {
    // Use current user credentials for server authentication
    const char *username = app_state.users[app_state.current_user_index].username;
    const char *password = app_state.users[app_state.current_user_index].password;

    if (network_authenticate(username, password))
    {
      // Refresh channel and user lists since we're online
      network_get_channels();
      network_get_users();
      // fprintf(stderr, "Authenticated with server successfully\n");
    }
    else
    {
      // fprintf(stderr, "Warning: Server authentication failed. Running in offline mode.\n");
    }
  }
  else if (network_init())
  {
    // If we just connected and authenticated successfully
    network_connected = 1;
    // Refresh channel and user lists since we're online
    network_get_channels();
    network_get_users();
    // fprintf(stderr, "Connected and authenticated with server successfully\n");
  }
  else
  {
    // fprintf(stderr, "Warning: Unable to connect to server. Running in offline mode.\n");
  }

  // Main input loop
  char input[MAX_INPUT_LEN] = {0};
  int input_pos = 0;
  int ch;

  // Current focus (0=channels, 1=input, 2=users)
  int current_focus = 1; // Start with input field focus

  while (1)
  {
    // Draw UI elements with focus indicators
    draw_logo(app_state.logo_win);
    draw_channels(app_state.channels_win, &app_state, current_focus == 0);
    draw_chat(app_state.chat_win, &app_state);
    draw_users(app_state.users_win, &app_state, current_focus == 2);
    draw_input(app_state.input_win, current_focus == 1, input); // Pass current input to draw

    // Debug: Display key pressed to identify issues
    if (current_focus == 1)
    {
      mvwprintw(app_state.input_win, 0, 40, "Last key: %d  ", ch);
      wrefresh(app_state.input_win);
    }

    // Get user input based on current focus
    if (current_focus == 0)
    {
      // Channel list has focus
      ch = wgetch(app_state.channels_win);
    }
    else if (current_focus == 1)
    {
      // Input field has focus
      ch = wgetch(app_state.input_win);
    }
    else
    {
      // User list has focus
      ch = wgetch(app_state.users_win);
    }

    // Handle key explicitly by value to ensure arrow keys work
    if (ch == '\t' || ch == 9)
    {
      // Tab key: cycle through focuses
      current_focus = (current_focus + 1) % 3;
    }
    else if (ch == KEY_UP || ch == 259)
    {
      // Explicit check for up arrow
      if (current_focus == 0 && app_state.current_channel_index > 0)
      {
        // Navigate channel list up
        app_state.current_channel_index--;
      }
      else if (current_focus == 2)
      {
        // Navigate user list up
        navigate_users(&app_state, -1);
      }
    }
    else if (ch == KEY_DOWN || ch == 258)
    {
      // Explicit check for down arrow
      if (current_focus == 0 && app_state.current_channel_index < app_state.channel_count - 1)
      {
        // Navigate channel list down
        app_state.current_channel_index++;
      }
      else if (current_focus == 2)
      {
        // Navigate user list down
        navigate_users(&app_state, 1);
      }
    }
    else if (ch == '\n' || ch == KEY_ENTER || ch == 10 || ch == 13)
    {
      if (current_focus == 1)
      {
        // Input field has focus, process entered command/message
        input[input_pos] = '\0';
        handle_input(&app_state, input);
        input_pos = 0;
        memset(input, 0, MAX_INPUT_LEN);
      }
      else if (current_focus == 0)
      {
        // Channel selection confirmed
        // Already set by arrow keys
      }
      else if (current_focus == 2)
      {
        // User selection confirmed - start a private message
        start_pm_with_selected_user(&app_state);
        // Switch focus to input field
        current_focus = 1;
      }
    }
    else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8)
    {
      // Handle backspace (only in input field)
      if (current_focus == 1 && input_pos > 0)
      {
        input_pos--;
        input[input_pos] = '\0';
      }
    }
    else if (ch == KEY_F(10))
    {
      // Exit application
      break;
    }
    else if (current_focus == 1 && isprint(ch) && input_pos < MAX_INPUT_LEN - 1)
    {
      // Add character to input (only in input field)
      input[input_pos++] = ch;
      input[input_pos] = '\0';
    }
  }

  // Cleanup
  cleanup_ui();
  endwin();

  return 0;
}