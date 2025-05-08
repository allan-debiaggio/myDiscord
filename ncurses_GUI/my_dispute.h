#ifndef MY_DISPUTE_H
#define MY_DISPUTE_H

#include <ncurses.h>
#include <panel.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>

// Color pairs
#define COLOR_NEON_YELLOW 1
#define COLOR_NEON_GREEN 2
#define COLOR_NEON_PINK 3
#define COLOR_GRAY 4
#define COLOR_BRIGHT_RED 5
#define COLOR_DARK_BLUE 6

// User roles
#define ROLE_USER 1
#define ROLE_MODERATOR 2
#define ROLE_ADMIN 3

// Maximum lengths
#define MAX_USERNAME_LEN 20
#define MAX_EMAIL_LEN 50
#define MAX_PASSWORD_LEN 30
#define MAX_MESSAGE_LEN 256
#define MAX_CHANNEL_NAME_LEN 30
#define MAX_INPUT_LEN 512
#define MAX_USERS 100
#define MAX_CHANNELS 30
#define MAX_MESSAGES 1000
#define MAX_REACTIONS 10

// UI dimensions and positions
#define LOGO_HEIGHT 30
#define LOGO_WIDTH 60
#define USER_LIST_WIDTH 25
#define CHANNEL_LIST_WIDTH 60
#define INPUT_HEIGHT 3

// Structures
typedef struct
{
  char username[MAX_USERNAME_LEN];
  char email[MAX_EMAIL_LEN];
  char password[MAX_PASSWORD_LEN];
  int role;
  int is_online;
  time_t muted_until[MAX_CHANNELS]; // Time until when user is muted on each channel
} User;

typedef struct
{
  char text[MAX_MESSAGE_LEN];
  char sender[MAX_USERNAME_LEN];
  time_t timestamp;
  char reactions[MAX_REACTIONS]; // Unicode emoji reactions
  int reaction_count[MAX_REACTIONS];
} Message;

typedef struct
{
  char name[MAX_CHANNEL_NAME_LEN];
  Message messages[MAX_MESSAGES];
  int message_count;
} Channel;

// Global state
typedef struct
{
  User users[MAX_USERS];
  int user_count;
  Channel channels[MAX_CHANNELS];
  int channel_count;
  int current_user_index;
  int current_channel_index;
  WINDOW *logo_win;
  WINDOW *channels_win;
  WINDOW *chat_win;
  WINDOW *input_win;
  WINDOW *users_win;
} AppState;

// Function declarations
// UI
void init_ui(AppState *state);
void draw_logo(WINDOW *win);
void draw_channels(WINDOW *win, AppState *state, bool has_focus);
void draw_chat(WINDOW *win, AppState *state);
void draw_users(WINDOW *win, AppState *state, bool has_focus);
void draw_input(WINDOW *win, bool has_focus, char *current_input);
void handle_input(AppState *state, char *input);
void cleanup_ui();

// Authentication
int login_screen();
int register_screen();
int authenticate_user(AppState *state, char *username, char *password);
int add_new_user(AppState *state, char *username, char *email, char *password);
int validate_password(char *password);

// Channels
int create_channel(AppState *state, char *name);
int delete_channel(AppState *state, char *name);
int join_channel(AppState *state, int channel_index);

// Users
int set_user_role(AppState *state, char *username, int role);
int mute_user(AppState *state, char *username, int channel_index, int minutes);
int get_selected_user_index(AppState *state);
void navigate_users(AppState *state, int direction);
void start_pm_with_selected_user(AppState *state);

// Messaging
int send_message(AppState *state, char *text);
int send_private_message(AppState *state, char *username, char *text);
int add_reaction(AppState *state, int message_index, char reaction);
void process_command(AppState *state, char *command);

#endif /* MY_DISPUTE_H */