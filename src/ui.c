#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

// Login Window Structure
typedef struct
{
  GtkWidget *window;
  GtkWidget *username_entry;
  GtkWidget *password_entry;
  int connected_socket;
} LoginWindow;

// Main Application Window Structure
typedef struct
{
  GtkWidget *window;
  GtkWidget *channel_list;
  GtkWidget *message_view;
  GtkWidget *message_entry;
  int socket;
  GtkTextBuffer *message_buffer;
} MainWindow;

// Global application state
MainWindow *main_window = NULL;

void show_error_dialog(GtkWindow *parent, const char *message)
{
  GtkWidget *dialog = gtk_message_dialog_new(parent,
                                             GTK_DIALOG_DESTROY_WITH_PARENT,
                                             GTK_MESSAGE_ERROR,
                                             GTK_BUTTONS_CLOSE,
                                             "%s", message);
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}

void on_message_send(GtkWidget *widget, gpointer data)
{
  MainWindow *mw = (MainWindow *)data;
  const char *message = gtk_entry_get_text(GTK_ENTRY(mw->message_entry));

  if (strlen(message) == 0)
    return;

  // Send message to server
  if (send(mw->socket, message, strlen(message), 0) < 0)
  {
    show_error_dialog(GTK_WINDOW(mw->window), "Failed to send message");
  }
  else
  {
    // Add message to UI
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(mw->message_buffer, &end);
    gtk_text_buffer_insert(mw->message_buffer, &end, "You: ", -1);
    gtk_text_buffer_insert(mw->message_buffer, &end, message, -1);
    gtk_text_buffer_insert(mw->message_buffer, &end, "\n", -1);

    // Clear entry
    gtk_entry_set_text(GTK_ENTRY(mw->message_entry), "");
  }
}

gboolean receive_messages(gpointer data)
{
  MainWindow *mw = (MainWindow *)data;
  char buffer[1024];

  // Non-blocking check for messages
  fd_set readfds;
  struct timeval tv = {0, 0}; // Don't block

  FD_ZERO(&readfds);
  FD_SET(mw->socket, &readfds);

  int activity = select(mw->socket + 1, &readfds, NULL, NULL, &tv);

  if (activity > 0 && FD_ISSET(mw->socket, &readfds))
  {
    ssize_t bytes_read = recv(mw->socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read > 0)
    {
      buffer[bytes_read] = '\0';

      // Add received message to UI
      GtkTextIter end;
      gtk_text_buffer_get_end_iter(mw->message_buffer, &end);
      gtk_text_buffer_insert(mw->message_buffer, &end, "Server: ", -1);
      gtk_text_buffer_insert(mw->message_buffer, &end, buffer, -1);
      gtk_text_buffer_insert(mw->message_buffer, &end, "\n", -1);
    }
  }

  return G_SOURCE_CONTINUE;
}

gboolean on_window_close(GtkWidget *widget, GdkEvent *event, gpointer data)
{
  if (main_window && main_window->socket > 0)
  {
    close(main_window->socket);
  }
  gtk_main_quit();
  return FALSE;
}

void on_login_clicked(GtkWidget *widget, gpointer data)
{
  LoginWindow *lw = (LoginWindow *)data;
  const char *username = gtk_entry_get_text(GTK_ENTRY(lw->username_entry));
  const char *password = gtk_entry_get_text(GTK_ENTRY(lw->password_entry));

  // Basic validation
  if (strlen(username) == 0 || strlen(password) == 0)
  {
    show_error_dialog(GTK_WINDOW(lw->window), "Username and password required");
    return;
  }

  // Test server connection
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
  {
    show_error_dialog(GTK_WINDOW(lw->window), "Failed to create socket");
    return;
  }

  struct sockaddr_in serv_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(8080),
      .sin_addr.s_addr = inet_addr("127.0.0.1")};

  printf("Attempting to connect to server...\n");
  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
  {
    show_error_dialog(GTK_WINDOW(lw->window), "Connection to server failed");
    close(sockfd);
    return;
  }

  printf("Connected to server successfully\n");

  // Send authentication message (for testing)
  char auth_msg[256];
  snprintf(auth_msg, sizeof(auth_msg), "LOGIN:%s:%s", username, password);
  if (send(sockfd, auth_msg, strlen(auth_msg), 0) < 0)
  {
    show_error_dialog(GTK_WINDOW(lw->window), "Failed to send authentication");
    close(sockfd);
    return;
  }

  // If connection successful, proceed to main window
  main_window = create_main_window();
  main_window->socket = sockfd;

  // Add message polling
  g_timeout_add(100, receive_messages, main_window);

  gtk_widget_show_all(main_window->window);
  gtk_widget_hide(lw->window);
}

void on_register_clicked(GtkWidget *widget, gpointer data)
{
  LoginWindow *lw = (LoginWindow *)data;
  const char *username = gtk_entry_get_text(GTK_ENTRY(lw->username_entry));
  const char *password = gtk_entry_get_text(GTK_ENTRY(lw->password_entry));

  // Basic validation
  if (strlen(username) == 0 || strlen(password) == 0)
  {
    show_error_dialog(GTK_WINDOW(lw->window), "Username and password required");
    return;
  }

  show_error_dialog(GTK_WINDOW(lw->window), "Registration functionality not implemented yet");
}

LoginWindow *create_login_window()
{
  LoginWindow *lw = g_malloc(sizeof(LoginWindow));

  lw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(lw->window), "MyDiscord Login");
  gtk_window_set_default_size(GTK_WINDOW(lw->window), 300, 200);
  gtk_container_set_border_width(GTK_CONTAINER(lw->window), 10);

  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
  gtk_container_add(GTK_CONTAINER(lw->window), grid);

  // Username
  GtkWidget *username_label = gtk_label_new("Username:");
  lw->username_entry = gtk_entry_new();
  gtk_grid_attach(GTK_GRID(grid), username_label, 0, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), lw->username_entry, 1, 0, 1, 1);

  // Password
  GtkWidget *password_label = gtk_label_new("Password:");
  lw->password_entry = gtk_entry_new();
  gtk_entry_set_visibility(GTK_ENTRY(lw->password_entry), FALSE);
  gtk_grid_attach(GTK_GRID(grid), password_label, 0, 1, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), lw->password_entry, 1, 1, 1, 1);

  // Buttons
  GtkWidget *login_btn = gtk_button_new_with_label("Login");
  GtkWidget *register_btn = gtk_button_new_with_label("Register");
  gtk_grid_attach(GTK_GRID(grid), login_btn, 0, 2, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), register_btn, 1, 2, 1, 1);

  // Connect signals
  g_signal_connect(login_btn, "clicked", G_CALLBACK(on_login_clicked), lw);
  g_signal_connect(register_btn, "clicked", G_CALLBACK(on_register_clicked), lw);
  g_signal_connect(lw->window, "delete-event", G_CALLBACK(on_window_close), NULL);

  return lw;
}

MainWindow *create_main_window()
{
  MainWindow *mw = g_malloc(sizeof(MainWindow));

  mw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(mw->window), "MyDiscord");
  gtk_window_set_default_size(GTK_WINDOW(mw->window), 800, 600);

  GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_container_add(GTK_CONTAINER(mw->window), main_box);

  // Channel List - Left sidebar
  GtkWidget *channel_scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(channel_scroll),
                                 GTK_POLICY_NEVER,
                                 GTK_POLICY_AUTOMATIC);
  mw->channel_list = gtk_list_box_new();
  gtk_container_add(GTK_CONTAINER(channel_scroll), mw->channel_list);
  gtk_box_pack_start(GTK_BOX(main_box), channel_scroll, FALSE, FALSE, 0);
  gtk_widget_set_size_request(channel_scroll, 150, -1);

  // Test channels
  GtkWidget *general = gtk_label_new("# general");
  gtk_widget_set_halign(general, GTK_ALIGN_START);
  gtk_list_box_insert(GTK_LIST_BOX(mw->channel_list), general, -1);

  GtkWidget *random = gtk_label_new("# random");
  gtk_widget_set_halign(random, GTK_ALIGN_START);
  gtk_list_box_insert(GTK_LIST_BOX(mw->channel_list), random, -1);

  // Main Chat Area
  GtkWidget *chat_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_box_pack_start(GTK_BOX(main_box), chat_box, TRUE, TRUE, 0);

  // Message Display
  GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  mw->message_view = gtk_text_view_new();
  mw->message_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(mw->message_view));
  gtk_text_view_set_editable(GTK_TEXT_VIEW(mw->message_view), FALSE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(mw->message_view), GTK_WRAP_WORD);
  gtk_container_add(GTK_CONTAINER(scrolled_window), mw->message_view);
  gtk_box_pack_start(GTK_BOX(chat_box), scrolled_window, TRUE, TRUE, 0);

  // Welcome message
  gtk_text_buffer_set_text(mw->message_buffer, "Welcome to MyDiscord!\n", -1);

  // Message Input Box
  GtkWidget *input_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  mw->message_entry = gtk_entry_new();
  GtkWidget *send_button = gtk_button_new_with_label("Send");

  gtk_box_pack_start(GTK_BOX(input_box), mw->message_entry, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(input_box), send_button, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(chat_box), input_box, FALSE, FALSE, 0);

  g_signal_connect(send_button, "clicked", G_CALLBACK(on_message_send), mw);
  g_signal_connect(mw->message_entry, "activate", G_CALLBACK(on_message_send), mw);
  g_signal_connect(mw->window, "delete-event", G_CALLBACK(on_window_close), NULL);

  return mw;
}

int main(int argc, char *argv[])
{
  gtk_init(&argc, &argv);

  LoginWindow *login_win = create_login_window();
  gtk_widget_show_all(login_win->window);

  gtk_main();
  return 0;
}