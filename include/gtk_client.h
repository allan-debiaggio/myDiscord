#ifndef GTK_CLIENT_H
#define GTK_CLIENT_H

#include <gtk/gtk.h>
#include "client.h"
#include "database.h" // Re-enabling database integration

// Channel structure for client side
typedef struct
{
  int channel_id;
  char name[MAX_CHANNEL_NAME_LEN];
  char description[BUFFER_SIZE];
  bool is_public;
} ChannelInfo;

// Main application structure
typedef struct
{
  // Windows and widgets
  GtkWidget *window;
  GtkWidget *main_box;
  GtkWidget *header_bar;
  GtkWidget *chat_box;
  GtkWidget *message_view;
  GtkWidget *scrolled_window;
  GtkWidget *user_list;
  GtkWidget *message_entry;
  GtkWidget *send_button;
  GtkWidget *status_bar;
  GtkWidget *channel_list;     // Channel list widget (GtkListBox)
  GtkListStore *channel_store; // Store for channel data (optional, may be NULL)
  GtkWidget *channel_tree;     // Tree view for channels (optional, may be NULL)

  // For channel name
  char *current_channel_name;

  // Message buffer
  GtkTextBuffer *buffer;

  // Application data
  ClientContext client;
  DatabaseContext db; // Re-enabling database integration

  // User information
  char username[MAX_USERNAME_LEN];
  char password[MAX_USERNAME_LEN];
  char email[100];
  bool authenticated;
  int user_id;

  // Channel information
  int current_channel_id;             // Current active channel
  ChannelInfo channels[MAX_CHANNELS]; // List of available channels
  int channel_count;                  // Number of available channels

  // Mutex for GTK operations
  pthread_mutex_t gtk_mutex;
} AppContext;

// Function prototypes
void create_login_window(AppContext *app);
void create_register_window(AppContext *app);
GtkWidget *create_main_window(AppContext *app);
void initialize_app(AppContext *app);
void cleanup_app(AppContext *app);
gboolean append_message_to_view(AppContext *app, const char *username, const char *text, gboolean is_status);
void update_user_list(AppContext *app, const char *username, gboolean add);
void update_channel_list(AppContext *app);
gboolean request_channel_list(gpointer app);
void join_channel(AppContext *app, int channel_id);
void create_new_channel(AppContext *app, const char *name, const char *description, bool is_public);

// GTK signal callbacks
void on_login_clicked(GtkWidget *button, gpointer user_data);
void on_register_clicked(GtkWidget *button, gpointer user_data);
void on_create_account_clicked(GtkWidget *button, gpointer user_data);
gboolean on_back_to_login_clicked(GtkWidget *button, gpointer user_data);
void on_send_clicked(GtkWidget *button, gpointer user_data);
void on_entry_activate(GtkWidget *entry, gpointer user_data);
gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer user_data);
void on_app_quit(GtkWidget *widget, gpointer user_data);
void on_create_channel_clicked(GtkWidget *button, gpointer user_data);

// GTK-specific message processing and receiving functions
gboolean process_message(gpointer data); // Message processing for GTK main loop
void *gtk_receive_messages(void *arg);   // Wrapper for network thread to use GTK idle add

#endif /* GTK_CLIENT_H */