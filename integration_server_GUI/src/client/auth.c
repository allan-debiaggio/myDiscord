#include "my_dispute.h"
#include "network.h"

// ASCII art for login screen from login-ascii.txt
static const char *login_ascii[] = {
    " __  __         ____  _                 _               _   _       _   ",
    "|  \\/  |_   _  |  _ \\(_)___ _ __  _   _| |_ ___        | \\ | | ___ | |_ ",
    "| |\\/| | | | | | | | | / __| '_ \\| | | | __/ _ \\  _____|  \\| |/ _ \\| __|",
    "| |  | | |_| | | |_| | \\__ \\ |_) | |_| | ||  __/ |_____| |\\  | (_) | |_ ",
    "|_|  |_|\\__, | |____/|_|___/ .__/ \\__,_|\\__\\___|       |_| \\_|\\___/ \\__|",
    "__   __ |___/           ___|_|           _                              ",
    "\\ \\ / /__  _   _ _ __  | __ )  ___  _ __(_)_ __   __ _                  ",
    " \\ V / _ \\| | | | '__| |  _ \\ / _ \\| '__| | '_ \\ / _` |                 ",
    "  | | (_) | |_| | |    | |_) | (_) | |  | | | | | (_| |                 ",
    " _|_|\\___/ \\__,_|_|    |____/ \\___/|_|  |_|_| |_|\\__, |                 ",
    "|  _ \\(_)___  ___ ___  _ __ __| |                |___/                  ",
    "| | | | / __|/ __/ _ \\| '__/ _` |                                       ",
    "| |_| | \\__ \\ (_| (_) | | | (_| |                                       ",
    "|____/|_|___/\\___\\___/|_|  \\__,_|                _                      ",
    "|  \\/  | ___  ___ ___  ___ _ __   __ _  ___ _ __| |                     ",
    "| |\\/| |/ _ \\/ __/ __|/ _ \\ '_ \\ / _` |/ _ \\ '__| |                     ",
    "| |  | |  __/\\__ \\__ \\  __/ | | | (_| |  __/ |  |_|                     ",
    "|_|  |_|\\___||___/___/\\___|_| |_|\\__, |\\___|_|  (_)                     ",
    "                                 |___/                                  "};

// Helper function to display ASCII art for login/register screens
static void display_login_ascii(WINDOW *win, int start_y, int start_x)
{
  int num_lines = sizeof(login_ascii) / sizeof(login_ascii[0]);

  // Draw the ASCII art with red color
  wattron(win, COLOR_PAIR(COLOR_BRIGHT_RED) | A_BOLD);

  for (int i = 0; i < num_lines; i++)
  {
    mvwprintw(win, start_y + i, start_x, "%s", login_ascii[i]);
  }

  wattroff(win, COLOR_PAIR(COLOR_BRIGHT_RED) | A_BOLD);
  wrefresh(win);
}

// Helper function to get masked input (for passwords)
static void get_masked_input(WINDOW *win, char *buffer, int max_len, int y, int x)
{
  int i = 0;
  int ch;

  // Turn off echoing to prevent duplicate characters
  noecho();

  while (1)
  {
    ch = wgetch(win);

    if (ch == '\n' || ch == KEY_ENTER)
    {
      buffer[i] = '\0';
      break;
    }
    else if (ch == KEY_BACKSPACE || ch == 127)
    {
      if (i > 0)
      {
        i--;
        mvwaddch(win, y, x + i, ' ');
        wmove(win, y, x + i);
        wrefresh(win);
      }
    }
    else if (i < max_len - 1 && isprint(ch))
    {
      buffer[i++] = ch;
      waddch(win, '*');
      wrefresh(win);
    }
  }
}

// Helper function to get regular text input
static void get_text_input(WINDOW *win, char *buffer, int max_len, int y, int x)
{
  int i = 0;
  int ch;

  // Turn off echoing to prevent duplicate characters
  noecho();

  while (1)
  {
    ch = wgetch(win);

    if (ch == '\n' || ch == KEY_ENTER)
    {
      buffer[i] = '\0';
      break;
    }
    else if (ch == KEY_BACKSPACE || ch == 127)
    {
      if (i > 0)
      {
        i--;
        mvwaddch(win, y, x + i, ' ');
        wmove(win, y, x + i);
        wrefresh(win);
      }
    }
    else if (i < max_len - 1 && isprint(ch))
    {
      buffer[i++] = ch;
      mvwaddch(win, y, x + i - 1, ch); // Manually add the character
      wrefresh(win);
    }
  }
}

int validate_password(char *password)
{
  int len = strlen(password);
  int has_upper = 0;
  int has_special = 0;

  if (len < 8)
  {
    return 0;
  }

  for (int i = 0; i < len; i++)
  {
    if (isupper(password[i]))
    {
      has_upper = 1;
    }
    else if (!isalnum(password[i]))
    {
      has_special = 1;
    }
  }

  return has_upper && has_special;
}

int authenticate_user(AppState *state, char *username, char *password)
{
  // First try network authentication if possible
  int network_auth_success = network_authenticate(username, password);

  // Store the credentials locally
  int user_idx = -1;

  // Look if user exists locally
  for (int i = 0; i < state->user_count; i++)
  {
    if (strcmp(state->users[i].username, username) == 0)
    {
      user_idx = i;
      break;
    }
  }

  // If user doesn't exist in local state, create a new entry
  if (user_idx == -1)
  {
    if (state->user_count >= MAX_USERS)
    {
      // No space for new users
      return 0;
    }
    user_idx = state->user_count;
    state->user_count++;
  }

  // Store credentials
  strncpy(state->users[user_idx].username, username, MAX_USERNAME_LEN - 1);
  strncpy(state->users[user_idx].password, password, MAX_PASSWORD_LEN - 1);

  // Set current user
  state->current_user_index = user_idx;
  state->users[user_idx].is_online = 1;

  // If network authentication was successful, refresh data
  if (network_auth_success)
  {
    // Refresh channel and user lists if online
    network_get_channels();
    network_get_users();
    fprintf(stderr, "Network authentication successful\n");
  }
  else
  {
    // If network authentication fails, still allow local mode
    fprintf(stderr, "Warning: Network authentication failed. Running in offline mode.\n");
  }

  return 1;
}

int add_new_user(AppState *state, char *username, char *email, char *password)
{
  // Check if user already exists locally
  for (int i = 0; i < state->user_count; i++)
  {
    if (strcmp(state->users[i].username, username) == 0)
    {
      // Username already in use
      return 0;
    }
  }

  // First try to register with the server
  int network_reg_success = network_register(username, email, password);

  // Add user to local state
  if (state->user_count >= MAX_USERS)
  {
    // No space for new users
    return 0;
  }

  int user_idx = state->user_count;

  // Store user data
  strncpy(state->users[user_idx].username, username, MAX_USERNAME_LEN - 1);
  strncpy(state->users[user_idx].email, email, MAX_EMAIL_LEN - 1);
  strncpy(state->users[user_idx].password, password, MAX_PASSWORD_LEN - 1);
  state->users[user_idx].role = ROLE_USER;
  state->users[user_idx].is_online = 1;

  state->user_count++;

  // Set as current user
  state->current_user_index = user_idx;

  if (network_reg_success)
  {
    // Refresh channel and user lists if online
    network_get_channels();
    network_get_users();
    fprintf(stderr, "Network registration successful\n");
  }
  else
  {
    fprintf(stderr, "Warning: Network registration failed. Running in offline mode.\n");
  }

  return 1;
}

int login_screen()
{
  clear();

  // Create a window for the ASCII art
  int max_y, max_x;
  getmaxyx(stdscr, max_y, max_x);

  // Calculate sizes for the ASCII art
  int ascii_height = sizeof(login_ascii) / sizeof(login_ascii[0]);
  int ascii_width = strlen(login_ascii[0]);
  int ascii_win_height = ascii_height + 2; // +2 for spacing
  int ascii_win_width = ascii_width + 4;   // +4 for spacing

  // Create ASCII art window - without border
  WINDOW *ascii_win = newwin(ascii_win_height, ascii_win_width, 2, (max_x - ascii_win_width) / 2);

  // Display the ASCII art
  display_login_ascii(ascii_win, 1, 2);
  wrefresh(ascii_win);

  // Create login form window - position adjusted to be below ASCII art
  int form_height = 8;
  int form_width = 50;
  int start_y = 2 + ascii_win_height + 1; // Position below ASCII art
  int start_x = (max_x - form_width) / 2;

  WINDOW *login_win = newwin(form_height, form_width, start_y, start_x);
  box(login_win, 0, 0);

  // Enable keypad mode for special key input
  keypad(login_win, TRUE);

  // Display title
  wattron(login_win, COLOR_PAIR(COLOR_NEON_PINK) | A_BOLD);
  mvwprintw(login_win, 1, (form_width - 5) / 2, "LOGIN");
  wattroff(login_win, COLOR_PAIR(COLOR_NEON_PINK) | A_BOLD);

  // Display form fields
  wattron(login_win, COLOR_PAIR(COLOR_NEON_GREEN));
  mvwprintw(login_win, 3, 5, "Username: ");
  mvwprintw(login_win, 5, 5, "Password: ");
  wattroff(login_win, COLOR_PAIR(COLOR_NEON_GREEN));

  wrefresh(login_win);

  // Get user input
  char username[MAX_USERNAME_LEN] = {0};
  char password[MAX_PASSWORD_LEN] = {0};

  // Set input mode for username
  cbreak(); // Line buffering disabled
  echo();   // Echo input

  // Get username
  wmove(login_win, 3, 15);
  get_text_input(login_win, username, MAX_USERNAME_LEN, 3, 15);

  // Set input mode for password
  noecho(); // Don't echo password

  // Get password
  wmove(login_win, 5, 15);
  get_masked_input(login_win, password, MAX_PASSWORD_LEN, 5, 15);

  // Authenticate user
  extern AppState app_state;
  int success = authenticate_user(&app_state, username, password);

  if (!success)
  {
    // Show error message
    wattron(login_win, COLOR_PAIR(COLOR_BRIGHT_RED));
    mvwprintw(login_win, 6, 5, "Invalid username or password. Press any key...");
    wattroff(login_win, COLOR_PAIR(COLOR_BRIGHT_RED));
    wrefresh(login_win);
    wgetch(login_win);
  }

  delwin(login_win);
  delwin(ascii_win);
  return success;
}

int register_screen()
{
  clear();

  // Create a window for the ASCII art
  int max_y, max_x;
  getmaxyx(stdscr, max_y, max_x);

  // Calculate sizes for the ASCII art
  int ascii_height = sizeof(login_ascii) / sizeof(login_ascii[0]);
  int ascii_width = strlen(login_ascii[0]);
  int ascii_win_height = ascii_height + 2; // +2 for spacing
  int ascii_win_width = ascii_width + 4;   // +4 for spacing

  // Create ASCII art window - without border
  WINDOW *ascii_win = newwin(ascii_win_height, ascii_win_width, 2, (max_x - ascii_win_width) / 2);

  // Display the ASCII art
  display_login_ascii(ascii_win, 1, 2);
  wrefresh(ascii_win);

  // Create registration form window - position adjusted to be below ASCII art
  int form_height = 12;
  int form_width = 60;
  int start_y = 2 + ascii_win_height + 1; // Position below ASCII art
  int start_x = (max_x - form_width) / 2;

  WINDOW *reg_win = newwin(form_height, form_width, start_y, start_x);
  box(reg_win, 0, 0);

  // Enable keypad mode for special key input
  keypad(reg_win, TRUE);

  // Display title
  wattron(reg_win, COLOR_PAIR(COLOR_NEON_PINK) | A_BOLD);
  mvwprintw(reg_win, 1, (form_width - 13) / 2, "REGISTRATION");
  wattroff(reg_win, COLOR_PAIR(COLOR_NEON_PINK) | A_BOLD);

  // Display form fields
  wattron(reg_win, COLOR_PAIR(COLOR_NEON_GREEN));
  mvwprintw(reg_win, 3, 5, "Username: ");
  mvwprintw(reg_win, 5, 5, "Email: ");
  mvwprintw(reg_win, 7, 5, "Password: ");
  wattron(reg_win, COLOR_PAIR(COLOR_NEON_YELLOW));
  mvwprintw(reg_win, 9, 5, "Password must be 8+ characters with 1 capital letter and 1 special character");
  wattroff(reg_win, COLOR_PAIR(COLOR_NEON_YELLOW));
  wattroff(reg_win, COLOR_PAIR(COLOR_NEON_GREEN));

  wrefresh(reg_win);

  // Get user input
  char username[MAX_USERNAME_LEN] = {0};
  char email[MAX_EMAIL_LEN] = {0};
  char password[MAX_PASSWORD_LEN] = {0};

  // Set input mode for text fields
  cbreak(); // Line buffering disabled
  echo();   // Echo input

  // Get username and email
  wmove(reg_win, 3, 15);
  get_text_input(reg_win, username, MAX_USERNAME_LEN, 3, 15);

  wmove(reg_win, 5, 15);
  get_text_input(reg_win, email, MAX_EMAIL_LEN, 5, 15);

  // Set input mode for password
  noecho(); // Don't echo password

  // Get password
  wmove(reg_win, 7, 15);
  get_masked_input(reg_win, password, MAX_PASSWORD_LEN, 7, 15);

  // Validate inputs
  int valid = 1;

  if (strlen(username) < 3)
  {
    wattron(reg_win, COLOR_PAIR(COLOR_BRIGHT_RED));
    mvwprintw(reg_win, 10, 5, "Username must be at least 3 characters long");
    wattroff(reg_win, COLOR_PAIR(COLOR_BRIGHT_RED));
    valid = 0;
  }
  else if (!validate_password(password))
  {
    wattron(reg_win, COLOR_PAIR(COLOR_BRIGHT_RED));
    mvwprintw(reg_win, 10, 5, "Password doesn't meet requirements");
    wattroff(reg_win, COLOR_PAIR(COLOR_BRIGHT_RED));
    valid = 0;
  }

  wrefresh(reg_win);

  if (!valid)
  {
    wgetch(reg_win);
    delwin(reg_win);
    delwin(ascii_win);
    return 0;
  }

  // Register user
  extern AppState app_state;
  int success = add_new_user(&app_state, username, email, password);

  if (!success)
  {
    wattron(reg_win, COLOR_PAIR(COLOR_BRIGHT_RED));
    mvwprintw(reg_win, 10, 5, "Registration failed. Press any key...");
    wattroff(reg_win, COLOR_PAIR(COLOR_BRIGHT_RED));
    wrefresh(reg_win);
    wgetch(reg_win);
  }

  delwin(reg_win);
  delwin(ascii_win);
  return success;
}