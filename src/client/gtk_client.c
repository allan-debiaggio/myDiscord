#include "../../include/gtk_client.h"
#include "../../include/auth.h"
#include <stdbool.h>

// Global app context
AppContext app;

// Function prototypes for internal functions
static void handle_channel_list(AppContext *app, const char *channel_data);
static void on_main_window_destroy(GtkWidget *widget, gpointer user_data);
static void update_status_label(AppContext *app, const char *text);
static void on_channel_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data);
static void setup_channel_section(AppContext *app, GtkWidget *sidebar);

// Custom message receiving function for GTK client (runs in main GTK thread)
gboolean process_message(gpointer data)
{
  Message *message = (Message *)data;
  extern AppContext app;

  if (!message)
  {
    g_warning("process_message called with NULL data");
    return FALSE;
  }

  // Ensure strings are null-terminated
  message->username[MAX_USERNAME_LEN - 1] = '\0';
  message->content[BUFFER_SIZE - 1] = '\0';

  // Process message based on type
  switch (message->type)
  {
  case MSG_TEXT:
    // Only display if message is for current channel
    if (message->channel_id == app.current_channel_id)
    {
      append_message_to_view(&app, message->username, message->content, FALSE);
    }
    break;

  case MSG_CONNECT:
    // User joined the server
    append_message_to_view(&app, NULL, g_strdup_printf("%s has joined the chat", message->username), TRUE);
    update_user_list(&app, message->username, TRUE);
    break;

  case MSG_DISCONNECT:
    // User disconnect or connection lost
    if (strcmp(message->username, "NetworkThread") == 0)
    {
      // Special case: our connection to server was lost
      append_message_to_view(&app, NULL, "Connection to server lost", TRUE);
      gtk_widget_set_sensitive(app.message_entry, FALSE);
      gtk_widget_set_sensitive(app.send_button, FALSE);
      update_status_label(&app, "Disconnected");
    }
    else
    {
      // Normal user disconnect
      append_message_to_view(&app, NULL, g_strdup_printf("%s has left the chat", message->username), TRUE);
      update_user_list(&app, message->username, FALSE);
    }
    break;

  case MSG_STATUS:
    // Status message from server or other users
    append_message_to_view(&app, message->username, message->content, TRUE);
    break;

  case MSG_SERVER:
    // Server notification message
    append_message_to_view(&app, message->username, message->content, TRUE);

    // Handle channel-related server messages
    if (strstr(message->content, "channel created") ||
        strstr(message->content, "New channel"))
    {
      // Schedule a channel list refresh
      g_timeout_add(500, request_channel_list, &app);
    }
    break;

  case MSG_CHANNEL_LIST:
    // Channel list received from server
    g_print("Received channel list: %s\n", message->content);
    handle_channel_list(&app, message->content);
    break;

  case MSG_CHANNEL_JOIN:
    // Channel join confirmation
    if (app.current_channel_id != message->channel_id)
    {
      g_print("Joined channel: %d - %s\n", message->channel_id, message->content);
      app.current_channel_id = message->channel_id;

      // Update UI elements
      if (app.current_channel_name)
      {
        g_free(app.current_channel_name);
      }
      app.current_channel_name = g_strdup(message->content);

      // Update header
      gtk_header_bar_set_subtitle(GTK_HEADER_BAR(app.header_bar),
                                  g_strdup_printf("%s Channel", app.current_channel_name));

      // Clear message view
      GtkTextIter start, end;
      gtk_text_buffer_get_bounds(app.buffer, &start, &end);
      gtk_text_buffer_delete(app.buffer, &start, &end);

      // Add join confirmation
      append_message_to_view(&app, NULL, g_strdup_printf("Joined channel: %s", message->content), TRUE);
    }
    break;

  case MSG_CHANNEL_LEAVE:
    // Channel leave notification
    append_message_to_view(&app, NULL, g_strdup_printf("Left channel: %s", message->content), TRUE);
    break;

  case MSG_CHANNEL_CREATE:
    // Channel creation confirmation/notification
    append_message_to_view(&app, message->username,
                           g_strdup_printf("Channel created: %s", message->content), TRUE);
    // Request updated channel list
    g_timeout_add(500, request_channel_list, &app);
    break;

  default:
    g_warning("Unknown message type received: %d", message->type);
  }

  // Free the message allocated by the receiving thread
  free(message);
  return FALSE;
}

// Helper function for disconnect messages
gboolean disconnect_message_handler(gpointer user_data)
{
  extern AppContext app;
  return append_message_to_view(&app, NULL, "Disconnected from server. Switching to local mode.", TRUE);
}

// Thread function to receive messages and dispatch to GTK main thread
void *gtk_receive_messages(void *arg)
{
  ClientContext *client = (ClientContext *)arg;
  ssize_t bytes_received;
  extern AppContext app; // Use the global app context

  while (client->connected)
  {
    // Allocate memory for the received message struct ON THE STACK temporarily
    Message received_message;
    memset(&received_message, 0, sizeof(Message));

    // Receive message directly into the temporary struct
    bytes_received = recv(client->socket, &received_message, sizeof(Message), 0);

    if (bytes_received <= 0)
    {
      // Connection closed or error
      g_warning("Receive failed or connection closed. Error: %s",
                (bytes_received == 0) ? "Connection closed by peer" : strerror(errno));
      client->connected = false; // Signal disconnection

      // Dispatch a disconnect notification to the main thread
      // We need to allocate data for this specifically
      Message *disconnect_notice = malloc(sizeof(Message));
      if (disconnect_notice)
      {
        memset(disconnect_notice, 0, sizeof(Message));
        disconnect_notice->type = MSG_DISCONNECT; // Use a specific signal if needed
        strncpy(disconnect_notice->username, "NetworkThread", MAX_USERNAME_LEN - 1);
        strncpy(disconnect_notice->content, "Connection lost", BUFFER_SIZE - 1);
        g_idle_add(process_message, disconnect_notice);
      }
      else
      {
        g_warning("Failed to allocate memory for disconnect notice");
      }
      break; // Exit the loop
    }

    // Check if the received size matches the expected size
    if (bytes_received != sizeof(Message))
    {
      g_warning("Received message of unexpected size: %zd, expected %zd. Discarding.",
                bytes_received, sizeof(Message));
      continue; // Skip processing this potentially corrupted message
    }

    // Allocate memory for a *copy* of the message to pass to the main thread
    Message *message_copy = malloc(sizeof(Message));
    if (!message_copy)
    {
      g_warning("Memory allocation failed for message copy");
      continue; // Skip this message
    }
    memcpy(message_copy, &received_message, sizeof(Message));

    // Dispatch the *copy* to GTK main thread for processing
    // g_idle_add will eventually call process_message(message_copy)
    g_idle_add(process_message, message_copy);
    // The process_message function is now responsible for free(message_copy)
  }

  g_print("Exiting GTK receive message thread.\n");
  return NULL;
}

// Initialize the application
void initialize_app(AppContext *app)
{
  memset(app, 0, sizeof(AppContext));

  // Initialize mutex
  if (pthread_mutex_init(&app->gtk_mutex, NULL) != 0)
  {
    g_error("Mutex initialization failed");
    exit(EXIT_FAILURE);
  }

  // Set default values
  app->authenticated = FALSE;
  app->client.connected = FALSE;
  app->user_id = -1;

  // Try to connect to database
  if (db_connect(&app->db))
  {
    g_print("Connected to database\n");
  }
  else
  {
    g_print("Failed to connect to database. Authentication features may not work.\n");
  }
}

// Clean up application resources
void cleanup_app(AppContext *app)
{
  // Disconnect from server if connected
  if (app->client.connected)
  {
    disconnect_from_server(&app->client);
  }

  // Disconnect from database if connected
  if (app->db.connected)
  {
    db_disconnect(&app->db);
  }

  // Destroy mutex
  pthread_mutex_destroy(&app->gtk_mutex);
}

// Create login window
void create_login_window(AppContext *app)
{
  // Create window
  app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(app->window), "MyDispute - Login");
  gtk_window_set_default_size(GTK_WINDOW(app->window), 400, 280);
  gtk_window_set_position(GTK_WINDOW(app->window), GTK_WIN_POS_CENTER);
  g_signal_connect(app->window, "delete-event", G_CALLBACK(on_window_delete), app);

  // Create main box
  GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_set_border_width(GTK_CONTAINER(main_box), 20);
  gtk_container_add(GTK_CONTAINER(app->window), main_box);

  // Add title label
  GtkWidget *title_label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(title_label), "<big><b>MyDispute</b></big>");
  gtk_box_pack_start(GTK_BOX(main_box), title_label, FALSE, FALSE, 10);

  // Add username field
  GtkWidget *username_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  GtkWidget *username_label = gtk_label_new("Username:");
  gtk_widget_set_size_request(username_label, 80, -1);
  gtk_box_pack_start(GTK_BOX(username_box), username_label, FALSE, FALSE, 0);

  GtkWidget *username_entry = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(username_entry), MAX_USERNAME_LEN - 1);
  gtk_box_pack_start(GTK_BOX(username_box), username_entry, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(main_box), username_box, FALSE, FALSE, 5);

  // Add password field
  GtkWidget *password_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  GtkWidget *password_label = gtk_label_new("Password:");
  gtk_widget_set_size_request(password_label, 80, -1);
  gtk_box_pack_start(GTK_BOX(password_box), password_label, FALSE, FALSE, 0);

  GtkWidget *password_entry = gtk_entry_new();
  gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE); // Hide password
  gtk_entry_set_max_length(GTK_ENTRY(password_entry), MAX_USERNAME_LEN - 1);
  gtk_box_pack_start(GTK_BOX(password_box), password_entry, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(main_box), password_box, FALSE, FALSE, 5);

  // Add server address field
  GtkWidget *server_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  GtkWidget *server_label = gtk_label_new("Server:");
  gtk_widget_set_size_request(server_label, 80, -1);
  gtk_box_pack_start(GTK_BOX(server_box), server_label, FALSE, FALSE, 0);

  GtkWidget *server_entry = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(server_entry), "127.0.0.1");
  gtk_box_pack_start(GTK_BOX(server_box), server_entry, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(main_box), server_box, FALSE, FALSE, 5);

  // Add buttons
  GtkWidget *button_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_END);
  gtk_box_set_spacing(GTK_BOX(button_box), 5);

  GtkWidget *register_button = gtk_button_new_with_label("Register");
  GtkWidget *quit_button = gtk_button_new_with_label("Quit");
  GtkWidget *login_button = gtk_button_new_with_label("Login");

  gtk_container_add(GTK_CONTAINER(button_box), register_button);
  gtk_container_add(GTK_CONTAINER(button_box), quit_button);
  gtk_container_add(GTK_CONTAINER(button_box), login_button);
  gtk_box_pack_end(GTK_BOX(main_box), button_box, FALSE, FALSE, 5);

  // Add status message at the bottom
  GtkWidget *status_label = gtk_label_new("");
  gtk_widget_set_name(status_label, "status-label");
  gtk_box_pack_end(GTK_BOX(main_box), status_label, FALSE, FALSE, 5);

  // Connect signals
  g_signal_connect(quit_button, "clicked", G_CALLBACK(on_app_quit), app);
  g_signal_connect(login_button, "clicked", G_CALLBACK(on_login_clicked), app);
  g_signal_connect(register_button, "clicked", G_CALLBACK(on_register_clicked), app);
  g_signal_connect(username_entry, "activate", G_CALLBACK(on_login_clicked), app);
  g_signal_connect(password_entry, "activate", G_CALLBACK(on_login_clicked), app);
  g_signal_connect(server_entry, "activate", G_CALLBACK(on_login_clicked), app);

  // Store widgets for later use
  g_object_set_data(G_OBJECT(login_button), "username_entry", username_entry);
  g_object_set_data(G_OBJECT(login_button), "password_entry", password_entry);
  g_object_set_data(G_OBJECT(login_button), "server_entry", server_entry);
  g_object_set_data(G_OBJECT(login_button), "status_label", status_label);

  g_object_set_data(G_OBJECT(register_button), "username_entry", username_entry);
  g_object_set_data(G_OBJECT(register_button), "password_entry", password_entry);

  // Show all widgets
  gtk_widget_show_all(app->window);
}

// Create main chat window
GtkWidget *create_main_window(AppContext *app)
{
  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  app->window = window;

  // Set window properties
  gtk_window_set_title(GTK_WINDOW(window), "MyDispute");
  gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
  g_signal_connect(window, "delete-event", G_CALLBACK(on_window_delete), app);

  // Create main box
  app->main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add(GTK_CONTAINER(window), app->main_box);

  // Create header bar
  app->header_bar = gtk_header_bar_new();
  gtk_header_bar_set_title(GTK_HEADER_BAR(app->header_bar), "MyDispute");
  gtk_header_bar_set_subtitle(GTK_HEADER_BAR(app->header_bar), "General Channel");
  gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(app->header_bar), TRUE);

  // Add disconnect button to header bar
  GtkWidget *disconnect_button = gtk_button_new_with_label("Disconnect");
  gtk_header_bar_pack_start(GTK_HEADER_BAR(app->header_bar), disconnect_button);
  g_signal_connect(disconnect_button, "clicked", G_CALLBACK(on_back_to_login_clicked), app);

  // Add create channel button to header bar
  GtkWidget *create_channel_button = gtk_button_new_with_label("Create Channel");
  gtk_header_bar_pack_end(GTK_HEADER_BAR(app->header_bar), create_channel_button);
  g_signal_connect(create_channel_button, "clicked", G_CALLBACK(on_create_channel_clicked), app);

  gtk_window_set_titlebar(GTK_WINDOW(window), app->header_bar);

  // Initialize channel storage
  app->channel_store = NULL; // Not using tree store in this version
  app->channel_count = 0;

  // Set default channel
  app->current_channel_id = 1;
  app->current_channel_name = g_strdup("General");

  // Initialize the default General channel
  app->channels[0].channel_id = 1;
  strncpy(app->channels[0].name, "General", MAX_CHANNEL_NAME_LEN - 1);
  strncpy(app->channels[0].description, "General Discussion", BUFFER_SIZE - 1);
  app->channels[0].is_public = true;
  app->channel_count = 1;

  // Create content box
  GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start(GTK_BOX(app->main_box), content_box, TRUE, TRUE, 0);

  // Create horizontal paned container
  GtkWidget *hpaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start(GTK_BOX(content_box), hpaned, TRUE, TRUE, 0);

  // Create sidebar
  GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_widget_set_size_request(sidebar, 200, -1);
  gtk_paned_add1(GTK_PANED(hpaned), sidebar);

  // Create and configure the channel section of the UI
  setup_channel_section(app, sidebar);

  // Online users section
  GtkWidget *users_title = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(users_title), "<b>Online Users</b>");
  gtk_widget_set_halign(users_title, GTK_ALIGN_START);
  gtk_widget_set_margin_start(users_title, 5);
  gtk_widget_set_margin_top(users_title, 10);
  gtk_box_pack_start(GTK_BOX(sidebar), users_title, FALSE, FALSE, 5);

  // Create scrolled window for user list
  GtkWidget *users_scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(users_scroll),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_widget_set_size_request(users_scroll, -1, 150);
  gtk_box_pack_start(GTK_BOX(sidebar), users_scroll, FALSE, FALSE, 0);

  // Create user list
  app->user_list = gtk_list_box_new();
  gtk_container_add(GTK_CONTAINER(users_scroll), app->user_list);

  // Create chat area in right pane
  GtkWidget *chat_area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_paned_add2(GTK_PANED(hpaned), chat_area);

  // Create scrolled window for chat view
  app->scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(app->scrolled_window),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_widget_set_vexpand(app->scrolled_window, TRUE);
  gtk_box_pack_start(GTK_BOX(chat_area), app->scrolled_window, TRUE, TRUE, 0);

  // Create text view for messages
  app->message_view = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(app->message_view), FALSE);
  gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(app->message_view), FALSE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app->message_view), GTK_WRAP_WORD_CHAR);
  gtk_container_add(GTK_CONTAINER(app->scrolled_window), app->message_view);

  // Get text buffer
  app->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->message_view));

  // Create input area
  GtkWidget *input_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(chat_area), input_box, FALSE, FALSE, 5);

  // Message entry
  app->message_entry = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(app->message_entry), "Type a message...");
  g_signal_connect(app->message_entry, "activate", G_CALLBACK(on_entry_activate), app);
  gtk_box_pack_start(GTK_BOX(input_box), app->message_entry, TRUE, TRUE, 0);

  // Send button
  app->send_button = gtk_button_new_with_label("Send");
  g_signal_connect(app->send_button, "clicked", G_CALLBACK(on_send_clicked), app);
  gtk_box_pack_start(GTK_BOX(input_box), app->send_button, FALSE, FALSE, 0);

  // Status bar
  app->status_bar = gtk_statusbar_new();
  gtk_box_pack_start(GTK_BOX(content_box), app->status_bar, FALSE, FALSE, 0);
  gtk_statusbar_push(GTK_STATUSBAR(app->status_bar), 0, "Connected to server");

  // Update the channel list UI
  update_channel_list(app);

  // Request channel list from server if connected
  if (app->client.connected)
  {
    g_timeout_add(500, request_channel_list, app);
  }
  else
  {
    // Add a welcome message
    append_message_to_view(app, NULL, "Welcome to the chat!", TRUE);
  }

  // Show all widgets
  gtk_widget_show_all(window);

  return window;
}

// Append message to text view
gboolean append_message_to_view(AppContext *app, const char *username, const char *text, gboolean is_status)
{
  pthread_mutex_lock(&app->gtk_mutex);

  GtkTextIter iter;
  char time_str[20];
  time_t now = time(NULL);
  struct tm *time_info = localtime(&now);
  strftime(time_str, sizeof(time_str), "%H:%M:%S", time_info);

  // Get end iterator
  gtk_text_buffer_get_end_iter(app->buffer, &iter);

  // Insert timestamp
  gtk_text_buffer_insert(app->buffer, &iter, "[", -1);
  gtk_text_buffer_insert(app->buffer, &iter, time_str, -1);
  gtk_text_buffer_insert(app->buffer, &iter, "] ", -1);

  // Apply appropriate styling based on message type
  if (is_status)
  {
    // Status message (join/leave/system)
    gtk_text_buffer_insert(app->buffer, &iter, text, -1);
  }
  else
  {
    // Regular chat message
    gtk_text_buffer_insert(app->buffer, &iter, username, -1);
    gtk_text_buffer_insert(app->buffer, &iter, ": ", -1);
    gtk_text_buffer_insert(app->buffer, &iter, text, -1);
  }

  // Add newline
  gtk_text_buffer_insert(app->buffer, &iter, "\n", -1);

  // Scroll to bottom
  gtk_text_buffer_get_end_iter(app->buffer, &iter);
  GtkTextMark *mark = gtk_text_buffer_create_mark(app->buffer, NULL, &iter, FALSE);
  gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(app->message_view), mark, 0.0, TRUE, 0.0, 1.0);
  gtk_text_buffer_delete_mark(app->buffer, mark);

  pthread_mutex_unlock(&app->gtk_mutex);
  return FALSE;
}

// Update user list with addition or removal of a user
void update_user_list(AppContext *app, const char *username, gboolean add)
{
  if (!app || !app->user_list || !username || strlen(username) == 0)
  {
    g_warning("Cannot update user list: invalid parameters");
    return;
  }

  // Lock mutex to avoid race conditions with UI updates
  pthread_mutex_lock(&app->gtk_mutex);

  g_print("Updating user list: %s %s\n", add ? "Adding" : "Removing", username);

  if (add)
  {
    // Check if user already exists to avoid duplicates
    GList *children = gtk_container_get_children(GTK_CONTAINER(app->user_list));
    gboolean user_exists = FALSE;

    for (GList *l = children; l != NULL; l = l->next)
    {
      GtkWidget *row = l->data;
      const char *row_username = (const char *)g_object_get_data(G_OBJECT(row), "username");

      if (row_username && strcmp(row_username, username) == 0)
      {
        user_exists = TRUE;
        break;
      }
    }
    g_list_free(children);

    if (!user_exists)
    {
      // Create a box for the user row with a status indicator
      GtkWidget *user_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
      gtk_widget_set_margin_start(user_box, 5);
      gtk_widget_set_margin_end(user_box, 5);
      gtk_widget_set_margin_top(user_box, 3);
      gtk_widget_set_margin_bottom(user_box, 3);

      // Add status icon (green dot)
      GtkWidget *icon = gtk_label_new("●");

      // Set text color to green using CSS
      GtkStyleContext *context = gtk_widget_get_style_context(icon);
      GtkCssProvider *provider = gtk_css_provider_new();
      gtk_css_provider_load_from_data(provider,
                                      "label { color: #4CAF50; }", // Green color
                                      -1, NULL);
      gtk_style_context_add_provider(context,
                                     GTK_STYLE_PROVIDER(provider),
                                     GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
      g_object_unref(provider);

      gtk_widget_set_margin_start(icon, 3);
      gtk_box_pack_start(GTK_BOX(user_box), icon, FALSE, FALSE, 2);

      // Add username label
      GtkWidget *name_label = gtk_label_new(username);
      gtk_widget_set_halign(name_label, GTK_ALIGN_START);
      gtk_label_set_ellipsize(GTK_LABEL(name_label), PANGO_ELLIPSIZE_END);
      gtk_box_pack_start(GTK_BOX(user_box), name_label, TRUE, TRUE, 2);

      // Create a row for the user
      GtkWidget *row = gtk_list_box_row_new();

      // Store username with the row for later reference
      g_object_set_data_full(G_OBJECT(row), "username", g_strdup(username), g_free);

      // Add row to the list
      gtk_container_add(GTK_CONTAINER(row), user_box);
      gtk_list_box_insert(GTK_LIST_BOX(app->user_list), row, -1);
      gtk_widget_show_all(row);

      g_print("Added user to UI: %s\n", username);
    }
  }
  else // Remove user
  {
    // Find and remove the user from the list
    GList *children = gtk_container_get_children(GTK_CONTAINER(app->user_list));
    for (GList *l = children; l != NULL; l = l->next)
    {
      GtkWidget *row = l->data;
      const char *row_username = (const char *)g_object_get_data(G_OBJECT(row), "username");

      if (row_username && strcmp(row_username, username) == 0)
      {
        gtk_widget_destroy(row);
        g_print("Removed user from UI: %s\n", username);
        break;
      }
    }
    g_list_free(children);
  }

  pthread_mutex_unlock(&app->gtk_mutex);
}

// Button click callbacks for authentication

// Handle login button click
void on_login_clicked(GtkWidget *button, gpointer user_data)
{
  AppContext *app = (AppContext *)user_data;

  // Get entries from button data
  GtkWidget *username_entry = g_object_get_data(G_OBJECT(button), "username_entry");
  GtkWidget *password_entry = g_object_get_data(G_OBJECT(button), "password_entry");
  GtkWidget *server_entry = g_object_get_data(G_OBJECT(button), "server_entry");
  GtkWidget *status_label = g_object_get_data(G_OBJECT(button), "status_label");

  const char *username = gtk_entry_get_text(GTK_ENTRY(username_entry));
  const char *password = gtk_entry_get_text(GTK_ENTRY(password_entry));
  const char *server_ip = gtk_entry_get_text(GTK_ENTRY(server_entry));

  // Validate input
  if (strlen(username) == 0 || strlen(password) == 0)
  {
    gtk_label_set_text(GTK_LABEL(status_label), "Username and password are required");
    return;
  }

  // Copy username and password to app context
  strncpy(app->username, username, MAX_USERNAME_LEN - 1);
  app->username[MAX_USERNAME_LEN - 1] = '\0';

  strncpy(app->password, password, MAX_USERNAME_LEN - 1);
  app->password[MAX_USERNAME_LEN - 1] = '\0';

  // Copy username to client context
  strncpy(app->client.username, username, MAX_USERNAME_LEN - 1);
  app->client.username[MAX_USERNAME_LEN - 1] = '\0';

  // Try to authenticate against database if connected
  bool auth_success = false;
  if (app->db.connected)
  {
    // Hash the password
    char hash[32]; // Simple hash output size
    hash_password(password, hash, sizeof(hash));

    // Check credentials in database
    auth_success = db_authenticate_user(&app->db, username, hash);

    if (!auth_success)
    {
      gtk_label_set_text(GTK_LABEL(status_label), "Invalid username or password");
      return;
    }
  }
  else if (strlen(server_ip) == 0)
  {
    // If database isn't connected and no server IP provided, we can't authenticate
    gtk_label_set_text(GTK_LABEL(status_label), "Database not connected and no server provided. Cannot proceed.");
    return;
  }

  bool server_connected = false;

  // Only try to connect to server if an IP was provided
  if (strlen(server_ip) > 0)
  {
    // Try to connect to server
    server_connected = connect_to_server(&app->client, server_ip);

    if (!server_connected)
    {
      // Failed to connect to server
      if (!app->db.connected)
      {
        // If database is also not connected, we can't proceed
        gtk_label_set_text(GTK_LABEL(status_label), "Failed to connect to server and database not available");
        return;
      }

      // Ask the user if they want to continue in local mode
      GtkWidget *dialog = gtk_message_dialog_new(
          GTK_WINDOW(app->window),
          GTK_DIALOG_MODAL,
          GTK_MESSAGE_QUESTION,
          GTK_BUTTONS_YES_NO,
          "Could not connect to server. Continue in local mode? (Messages will be stored locally only)");

      int response = gtk_dialog_run(GTK_DIALOG(dialog));
      gtk_widget_destroy(dialog);

      if (response != GTK_RESPONSE_YES)
      {
        // User chose not to continue in local mode
        return;
      }
    }
  }

  // Set authenticated flag
  app->authenticated = TRUE;

  // Make sure client connection status is correct
  app->client.connected = server_connected;

  // Destroy login window
  GtkWidget *old_window = app->window;
  app->window = NULL; // Clear it first to avoid issues

  if (old_window)
    gtk_widget_destroy(old_window);

  // Create main window
  create_main_window(app);

  if (server_connected)
  {
    // Explicitly request the channel list once after successful connection
    request_channel_list(app);
  }
}

// Handle register button click
void on_register_clicked(GtkWidget *button, gpointer user_data)
{
  AppContext *app = (AppContext *)user_data;

  // Destroy login window
  gtk_widget_destroy(app->window);

  // Create registration window
  create_register_window(app);
}

// Handle create account button click
void on_create_account_clicked(GtkWidget *button, gpointer user_data)
{
  AppContext *app = (AppContext *)user_data;

  // Get entries from button data
  GtkWidget *username_entry = g_object_get_data(G_OBJECT(button), "username_entry");
  GtkWidget *password_entry = g_object_get_data(G_OBJECT(button), "password_entry");
  GtkWidget *confirm_entry = g_object_get_data(G_OBJECT(button), "confirm_entry");
  GtkWidget *email_entry = g_object_get_data(G_OBJECT(button), "email_entry");
  GtkWidget *status_label = g_object_get_data(G_OBJECT(button), "status_label");

  const char *username = gtk_entry_get_text(GTK_ENTRY(username_entry));
  const char *password = gtk_entry_get_text(GTK_ENTRY(password_entry));
  const char *confirm = gtk_entry_get_text(GTK_ENTRY(confirm_entry));
  const char *email = gtk_entry_get_text(GTK_ENTRY(email_entry));

  // Validate input
  if (strlen(username) < 3)
  {
    gtk_label_set_text(GTK_LABEL(status_label), "Username must be at least 3 characters");
    return;
  }

  if (strlen(password) < 4)
  {
    gtk_label_set_text(GTK_LABEL(status_label), "Password must be at least 4 characters");
    return;
  }

  if (strcmp(password, confirm) != 0)
  {
    gtk_label_set_text(GTK_LABEL(status_label), "Passwords do not match");
    return;
  }

  // Try to create user in database if connected
  if (app->db.connected)
  {
    // Hash the password
    char hash[32]; // Simple hash output size
    hash_password(password, hash, sizeof(hash));

    // Create user in database
    bool success = db_create_user(&app->db, username, hash, email);

    if (!success)
    {
      gtk_label_set_text(GTK_LABEL(status_label), "Failed to create user. Username may already exist.");
      return;
    }

    // Success message
    gtk_label_set_text(GTK_LABEL(status_label), "Account created successfully!");

    // Create a dialog to inform the user
    GtkWidget *dialog = gtk_message_dialog_new(
        GTK_WINDOW(app->window),
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_INFO,
        GTK_BUTTONS_OK,
        "Account created successfully! You can now log in.");

    // Run the dialog and wait for response
    gtk_dialog_run(GTK_DIALOG(dialog));

    // Destroy the dialog
    gtk_widget_destroy(dialog);

    // Return to login screen directly
    // First, destroy registration window (already stored in app->window)
    GtkWidget *old_window = app->window;
    app->window = NULL; // Clear it first to avoid issues
    if (old_window)
      gtk_widget_destroy(old_window);

    // Create login window
    create_login_window(app);
  }
  else
  {
    gtk_label_set_text(GTK_LABEL(status_label), "Database not connected. Registration not available.");
  }
}

// Handle back to login button click
gboolean on_back_to_login_clicked(GtkWidget *widget, gpointer user_data)
{
  // Safety check for user_data
  if (!user_data)
  {
    return FALSE;
  }

  AppContext *app = (AppContext *)user_data;

  // If called via g_timeout_add, widget might be NULL
  if (app->window == NULL)
  {
    return FALSE; // Window already destroyed, don't proceed
  }

  // Store window before destroying it
  GtkWidget *old_window = app->window;
  app->window = NULL; // Mark as destroyed first to avoid issues

  // Destroy window if it's valid
  if (old_window && GTK_IS_WIDGET(old_window))
  {
    gtk_widget_destroy(old_window);
  }

  // Create login window only if app is valid
  create_login_window(app);

  return FALSE; // For use with g_timeout_add
}

void on_send_clicked(GtkWidget *button, gpointer user_data)
{
  AppContext *app = (AppContext *)user_data;

  // Get message from entry
  const char *message_text = gtk_entry_get_text(GTK_ENTRY(app->message_entry));

  // Skip if message is empty
  if (strlen(message_text) == 0)
  {
    return;
  }

  bool message_handled = false;

  // Try to send message to server
  if (app->client.connected)
  {
    // Use the current channel ID when sending
    if (send_message(&app->client, app->current_channel_id, message_text))
    {
      // Add message to view (local echo)
      append_message_to_view(app, app->username, message_text, FALSE);
      message_handled = true;
    }
    else
    {
      // Send failed, mark as disconnected
      app->client.connected = false;
      append_message_to_view(app, NULL, "Lost connection to server. Switching to local mode.", TRUE);
    }
  }

  // Store in database if we're not connected to server or if sending failed
  if (!message_handled && app->db.connected)
  {
    // Store message in database (local mode)
    time_t timestamp = time(NULL);
    if (db_store_message(&app->db, app->current_channel_id, app->user_id, app->username, timestamp, MSG_TEXT, message_text))
    {
      // Add message to view
      append_message_to_view(app, app->username, message_text, FALSE);
      message_handled = true;

      // Update status bar to show we're in local mode
      gtk_statusbar_push(GTK_STATUSBAR(app->status_bar), 0, "Local mode - messages stored in database only");
    }
  }

  // If we couldn't handle the message anywhere, show error
  if (!message_handled)
  {
    append_message_to_view(app, NULL, "Failed to send message. Not connected to server or database.", TRUE);
  }

  // Clear entry
  gtk_entry_set_text(GTK_ENTRY(app->message_entry), "");
}

void on_entry_activate(GtkWidget *entry, gpointer user_data)
{
  on_send_clicked(NULL, user_data);
}

gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  AppContext *app = (AppContext *)user_data;
  cleanup_app(app);
  gtk_main_quit();
  return FALSE;
}

void on_app_quit(GtkWidget *widget, gpointer user_data)
{
  AppContext *app = (AppContext *)user_data;
  cleanup_app(app);
  gtk_main_quit();
}

// Create registration window
void create_register_window(AppContext *app)
{
  // Create window
  app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(app->window), "MyDispute - Register");
  gtk_window_set_default_size(GTK_WINDOW(app->window), 400, 320);
  gtk_window_set_position(GTK_WINDOW(app->window), GTK_WIN_POS_CENTER);
  g_signal_connect(app->window, "delete-event", G_CALLBACK(on_window_delete), app);

  // Create main box
  GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_set_border_width(GTK_CONTAINER(main_box), 20);
  gtk_container_add(GTK_CONTAINER(app->window), main_box);

  // Add title label
  GtkWidget *title_label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(title_label), "<big><b>MyDispute - Create New Account</b></big>");
  gtk_box_pack_start(GTK_BOX(main_box), title_label, FALSE, FALSE, 10);

  // Add username field
  GtkWidget *username_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  GtkWidget *username_label = gtk_label_new("Username:");
  gtk_widget_set_size_request(username_label, 80, -1);
  gtk_box_pack_start(GTK_BOX(username_box), username_label, FALSE, FALSE, 0);

  GtkWidget *username_entry = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(username_entry), MAX_USERNAME_LEN - 1);
  gtk_box_pack_start(GTK_BOX(username_box), username_entry, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(main_box), username_box, FALSE, FALSE, 5);

  // Add password field
  GtkWidget *password_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  GtkWidget *password_label = gtk_label_new("Password:");
  gtk_widget_set_size_request(password_label, 80, -1);
  gtk_box_pack_start(GTK_BOX(password_box), password_label, FALSE, FALSE, 0);

  GtkWidget *password_entry = gtk_entry_new();
  gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE); // Hide password
  gtk_entry_set_max_length(GTK_ENTRY(password_entry), MAX_USERNAME_LEN - 1);
  gtk_box_pack_start(GTK_BOX(password_box), password_entry, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(main_box), password_box, FALSE, FALSE, 5);

  // Add confirm password field
  GtkWidget *confirm_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  GtkWidget *confirm_label = gtk_label_new("Confirm:");
  gtk_widget_set_size_request(confirm_label, 80, -1);
  GtkWidget *confirm_entry = gtk_entry_new();
  gtk_entry_set_visibility(GTK_ENTRY(confirm_entry), FALSE); // Hide password
  gtk_entry_set_max_length(GTK_ENTRY(confirm_entry), MAX_USERNAME_LEN - 1);
  gtk_box_pack_start(GTK_BOX(confirm_box), confirm_label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(confirm_box), confirm_entry, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(main_box), confirm_box, FALSE, FALSE, 5);

  // Add email field (optional)
  GtkWidget *email_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  GtkWidget *email_label = gtk_label_new("Email:");
  gtk_widget_set_size_request(email_label, 80, -1);
  gtk_box_pack_start(GTK_BOX(email_box), email_label, FALSE, FALSE, 0);

  GtkWidget *email_entry = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(email_entry), "Optional");
  gtk_box_pack_start(GTK_BOX(email_box), email_entry, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(main_box), email_box, FALSE, FALSE, 5);

  // Add buttons
  GtkWidget *button_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_END);
  gtk_box_set_spacing(GTK_BOX(button_box), 5);

  GtkWidget *back_button = gtk_button_new_with_label("Back to Login");
  GtkWidget *register_button = gtk_button_new_with_label("Create Account");

  gtk_container_add(GTK_CONTAINER(button_box), back_button);
  gtk_container_add(GTK_CONTAINER(button_box), register_button);
  gtk_box_pack_end(GTK_BOX(main_box), button_box, FALSE, FALSE, 5);

  // Add status message at the bottom
  GtkWidget *status_label = gtk_label_new("");
  gtk_widget_set_name(status_label, "status-label");
  gtk_box_pack_end(GTK_BOX(main_box), status_label, FALSE, FALSE, 5);

  // Connect signals
  g_signal_connect(back_button, "clicked", G_CALLBACK(on_back_to_login_clicked), app);
  g_signal_connect(register_button, "clicked", G_CALLBACK(on_create_account_clicked), app);

  // Store widgets for later use
  g_object_set_data(G_OBJECT(register_button), "username_entry", username_entry);
  g_object_set_data(G_OBJECT(register_button), "password_entry", password_entry);
  g_object_set_data(G_OBJECT(register_button), "confirm_entry", confirm_entry);
  g_object_set_data(G_OBJECT(register_button), "email_entry", email_entry);
  g_object_set_data(G_OBJECT(register_button), "status_label", status_label);

  // Show all widgets
  gtk_widget_show_all(app->window);
}

// Update channel list in the UI
void update_channel_list(AppContext *app)
{
  if (!app || !app->channel_list)
  {
    g_warning("Cannot update channel list: app context or list widget is NULL");
    return;
  }

  // Lock the GTK mutex to prevent race conditions
  pthread_mutex_lock(&app->gtk_mutex);

  // Remove all existing channel rows
  GList *children = gtk_container_get_children(GTK_CONTAINER(app->channel_list));
  g_list_foreach(children, (GFunc)gtk_widget_destroy, NULL);
  g_list_free(children);

  // Debug output
  g_print("Updating channel list with %d channels\n", app->channel_count);

  // Add each channel as a row
  for (int i = 0; i < app->channel_count; i++)
  {
    // Create a box for the channel row
    GtkWidget *channel_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_margin_start(channel_box, 5);
    gtk_widget_set_margin_end(channel_box, 5);
    gtk_widget_set_margin_top(channel_box, 3);
    gtk_widget_set_margin_bottom(channel_box, 3);

    // Add channel icon based on type
    GtkWidget *icon = NULL;
    if (app->channels[i].channel_id == 1)
    {
      // Globe icon for general channel
      icon = gtk_image_new_from_icon_name("network-workgroup-symbolic", GTK_ICON_SIZE_MENU);
    }
    else if (app->channels[i].is_public)
    {
      // Hash/pound sign for public channels
      icon = gtk_label_new("#");
    }
    else
    {
      // Lock for private channels
      icon = gtk_image_new_from_icon_name("channel-secure-symbolic", GTK_ICON_SIZE_MENU);
    }

    if (GTK_IS_WIDGET(icon))
    {
      gtk_widget_set_margin_start(icon, 3);
      gtk_box_pack_start(GTK_BOX(channel_box), icon, FALSE, FALSE, 2);
    }

    // Add channel name label
    GtkWidget *name_label = gtk_label_new(app->channels[i].name);
    gtk_widget_set_halign(name_label, GTK_ALIGN_START);
    gtk_label_set_ellipsize(GTK_LABEL(name_label), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(channel_box), name_label, TRUE, TRUE, 2);

    // Create a row for the channel
    GtkWidget *row = gtk_list_box_row_new();

    // Store channel ID and name with the row
    g_object_set_data(G_OBJECT(row), "channel_id", GINT_TO_POINTER(app->channels[i].channel_id));
    g_object_set_data_full(G_OBJECT(row), "channel_name", g_strdup(app->channels[i].name), g_free);

    // Add tooltip with description
    gtk_widget_set_tooltip_text(row, app->channels[i].description);

    // Add row to the list
    gtk_container_add(GTK_CONTAINER(row), channel_box);
    gtk_list_box_insert(GTK_LIST_BOX(app->channel_list), row, -1);

    // Show the new row
    gtk_widget_show_all(row);

    g_print("Adding channel to UI: %d - %s\n", app->channels[i].channel_id, app->channels[i].name);
  }

  // Highlight currently selected channel
  if (app->current_channel_id > 0)
  {
    children = gtk_container_get_children(GTK_CONTAINER(app->channel_list));
    for (GList *l = children; l != NULL; l = l->next)
    {
      GtkWidget *row = l->data;
      int channel_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "channel_id"));

      if (channel_id == app->current_channel_id)
      {
        gtk_list_box_select_row(GTK_LIST_BOX(app->channel_list), GTK_LIST_BOX_ROW(row));
        break;
      }
    }
    g_list_free(children);
  }

  pthread_mutex_unlock(&app->gtk_mutex);
}

// Request channel list from server
gboolean request_channel_list(gpointer app)
{
  AppContext *app_ctx = (AppContext *)app;

  if (!app_ctx)
  {
    g_warning("Cannot request channel list: app is NULL");
    return FALSE;
  }

  if (!app_ctx->client.connected)
  {
    g_warning("Cannot request channel list: not connected to server");
    return FALSE;
  }

  g_print("Requesting channel list from server\n");

  // Create a MSG_CHANNEL_LIST message
  Message msg;
  memset(&msg, 0, sizeof(Message));
  msg.type = MSG_CHANNEL_LIST;

  // Copy username
  strncpy(msg.username, app_ctx->username, MAX_USERNAME_LEN - 1);
  msg.username[MAX_USERNAME_LEN - 1] = '\0';

  // Set timestamp
  msg.timestamp = time(NULL);

  // Send to server
  if (send(app_ctx->client.socket, &msg, sizeof(Message), 0) < 0)
  {
    g_warning("Send channel list request failed: %s", strerror(errno));
    app_ctx->client.connected = FALSE;
    return FALSE;
  }

  return FALSE; // Don't repeat the timeout
}

// Join a channel
void join_channel(AppContext *app, int channel_id)
{
  if (!app || !app->client.connected)
  {
    return;
  }

  // If already in this channel, do nothing
  if (app->current_channel_id == channel_id)
  {
    return;
  }

  // Create a MSG_CHANNEL_JOIN message
  Message msg;
  memset(&msg, 0, sizeof(Message));
  msg.type = MSG_CHANNEL_JOIN;

  // Put channel ID in content
  snprintf(msg.content, BUFFER_SIZE, "%d", channel_id);

  // Copy username and channel
  strncpy(msg.username, app->username, MAX_USERNAME_LEN - 1);
  msg.channel_id = channel_id;

  // Send to server
  if (send(app->client.socket, &msg, sizeof(Message), 0) < 0)
  {
    perror("Send channel join request failed");
    app->client.connected = false;
    append_message_to_view(app, NULL, "Lost connection to server. Switching to local mode.", TRUE);
  }
}

// Create a new channel
void create_new_channel(AppContext *app, const char *name, const char *description, bool is_public)
{
  if (!app || !app->client.connected)
  {
    g_warning("Cannot create channel: not connected to server");
    update_status_label(app, "Cannot create channel: not connected to server");
    return;
  }

  if (!name || strlen(name) == 0)
  {
    g_warning("Cannot create channel: empty name");
    update_status_label(app, "Cannot create channel: empty name");
    return;
  }

  // Construct message content: name|description|is_public
  char content[BUFFER_SIZE];
  snprintf(content, sizeof(content), "%s|%s|%d",
           name,
           description ? description : "Channel created by user",
           is_public ? 1 : 0);

  // Prepare message
  Message msg = {0};
  msg.type = MSG_CHANNEL_CREATE;
  strncpy(msg.username, app->username, MAX_USERNAME_LEN - 1);
  msg.username[MAX_USERNAME_LEN - 1] = '\0';
  strncpy(msg.content, content, BUFFER_SIZE - 1);
  msg.content[BUFFER_SIZE - 1] = '\0';
  msg.timestamp = time(NULL);

  // Send message to server
  if (send(app->client.socket, &msg, sizeof(Message), 0) < 0)
  {
    g_warning("Failed to send channel creation request: %s", strerror(errno));
    update_status_label(app, "Channel creation failed: network error");
    return;
  }

  g_print("Sent channel creation request for: '%s'\n", name);
  update_status_label(app, g_strdup_printf("Channel creation request sent: %s", name));
}

// Create channel button callback
void on_create_channel_clicked(GtkWidget *button, gpointer user_data)
{
  AppContext *app = (AppContext *)user_data;

  // Check if connected to server
  if (!app->client.connected)
  {
    GtkWidget *error_dialog = gtk_message_dialog_new(GTK_WINDOW(app->window),
                                                     GTK_DIALOG_MODAL,
                                                     GTK_MESSAGE_ERROR,
                                                     GTK_BUTTONS_OK,
                                                     "Cannot create channel: not connected to server");
    gtk_dialog_run(GTK_DIALOG(error_dialog));
    gtk_widget_destroy(error_dialog);
    return;
  }

  // Create dialog for channel creation
  GtkWidget *dialog = gtk_dialog_new_with_buttons("Create New Channel",
                                                  GTK_WINDOW(app->window),
                                                  GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  "Cancel", GTK_RESPONSE_CANCEL,
                                                  "Create", GTK_RESPONSE_ACCEPT,
                                                  NULL);

  // Create content area
  GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  gtk_container_set_border_width(GTK_CONTAINER(content_area), 10);
  gtk_widget_set_size_request(dialog, 400, -1);

  // Channel name field
  GtkWidget *name_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  GtkWidget *name_label = gtk_label_new("Name:");
  gtk_widget_set_size_request(name_label, 80, -1);
  GtkWidget *name_entry = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(name_entry), MAX_CHANNEL_NAME_LEN - 1);

  gtk_box_pack_start(GTK_BOX(name_box), name_label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(name_box), name_entry, TRUE, TRUE, 0);

  // Description field
  GtkWidget *desc_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  GtkWidget *desc_label = gtk_label_new("Description:");
  gtk_widget_set_size_request(desc_label, 80, -1);
  GtkWidget *desc_entry = gtk_entry_new();

  gtk_box_pack_start(GTK_BOX(desc_box), desc_label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(desc_box), desc_entry, TRUE, TRUE, 0);

  // Visibility checkbox
  GtkWidget *public_check = gtk_check_button_new_with_label("Public channel");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(public_check), TRUE);

  // Add to dialog
  gtk_box_pack_start(GTK_BOX(content_area), name_box, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(content_area), desc_box, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(content_area), public_check, FALSE, FALSE, 5);

  // Set focus to name entry
  gtk_widget_grab_focus(name_entry);

  // Show dialog
  gtk_widget_show_all(dialog);

  // Run dialog
  int response = gtk_dialog_run(GTK_DIALOG(dialog));
  if (response == GTK_RESPONSE_ACCEPT)
  {
    const char *name = gtk_entry_get_text(GTK_ENTRY(name_entry));
    const char *description = gtk_entry_get_text(GTK_ENTRY(desc_entry));
    bool is_public = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(public_check));

    // Validate name
    if (name && name[0] != '\0')
    {
      // Create the channel
      create_new_channel(app, name, description, is_public);

      // Show a status message in the UI
      append_message_to_view(app, NULL, g_strdup_printf("Creating channel '%s'...", name), TRUE);
    }
    else
    {
      // Show error message
      GtkWidget *error_dialog = gtk_message_dialog_new(GTK_WINDOW(dialog),
                                                       GTK_DIALOG_MODAL,
                                                       GTK_MESSAGE_ERROR,
                                                       GTK_BUTTONS_OK,
                                                       "Channel name cannot be empty");
      gtk_dialog_run(GTK_DIALOG(error_dialog));
      gtk_widget_destroy(error_dialog);
    }
  }

  // Destroy dialog
  gtk_widget_destroy(dialog);
}

// Channel row activation handler for list box
static void on_channel_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data)
{
  AppContext *app = (AppContext *)user_data;

  if (!app || !row)
  {
    g_warning("Invalid channel row activation: NULL app or row");
    return;
  }

  // Get the channel ID from the row data
  int channel_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "channel_id"));
  const char *channel_name = (const char *)g_object_get_data(G_OBJECT(row), "channel_name");

  if (!channel_name)
  {
    g_warning("Invalid channel row: no channel name data");
    return;
  }

  g_print("Selected channel: %d - %s\n", channel_id, channel_name);

  // If already in this channel, do nothing
  if (app->current_channel_id == channel_id)
  {
    g_print("Already in channel %d, not switching\n", channel_id);
    return;
  }

  // Update current channel
  app->current_channel_id = channel_id;

  if (app->current_channel_name)
  {
    g_free(app->current_channel_name);
  }
  app->current_channel_name = g_strdup(channel_name);

  // Update the header bar subtitle
  gtk_header_bar_set_subtitle(GTK_HEADER_BAR(app->header_bar),
                              g_strdup_printf("%s Channel", channel_name));

  // Clear message view before showing channel messages
  GtkTextIter start, end;
  gtk_text_buffer_get_bounds(app->buffer, &start, &end);
  gtk_text_buffer_delete(app->buffer, &start, &end);

  // Add joining message
  append_message_to_view(app, NULL, g_strdup_printf("Joined channel: %s", channel_name), TRUE);

  // If connected to server, send channel join message
  if (app->client.connected)
  {
    Message msg = {0};
    msg.type = MSG_CHANNEL_JOIN;
    msg.channel_id = channel_id;
    strncpy(msg.username, app->username, MAX_USERNAME_LEN - 1);
    msg.username[MAX_USERNAME_LEN - 1] = '\0';
    strncpy(msg.content, channel_name, BUFFER_SIZE - 1);
    msg.content[BUFFER_SIZE - 1] = '\0';

    if (send(app->client.socket, &msg, sizeof(Message), 0) < 0)
    {
      g_warning("Failed to send channel join request: %s", strerror(errno));
      app->client.connected = false;
      append_message_to_view(app, NULL, "Failed to join channel: Network error", TRUE);
      return;
    }

    g_print("Successfully joined channel %d (%s)\n", channel_id, channel_name);
  }
  else
  {
    g_warning("Not connected to server, channel join is local only");
    append_message_to_view(app, NULL, "Not connected to server. Channel change is local only.", TRUE);
  }
}

// Handle channel list message
static void handle_channel_list(AppContext *app, const char *channel_data)
{
  if (!app)
  {
    g_warning("Cannot handle channel list: app context is NULL");
    return;
  }

  g_print("Received channel list: %s\n", channel_data);

  // Reset channel count before parsing
  app->channel_count = 0;

  // Format: comma-separated list of "id:name:description:is_public" entries
  // Skip if empty
  if (!channel_data || strlen(channel_data) == 0)
  {
    g_warning("Received empty channel list");
    goto ensure_general;
  }

  // Make a copy of the data for parsing
  char *data_copy = g_strdup(channel_data);
  if (!data_copy)
  {
    g_warning("Failed to allocate memory for channel data");
    goto ensure_general;
  }

  // Parse comma-separated channel entries
  char *saveptr = NULL;
  char *entry = strtok_r(data_copy, ",", &saveptr);

  while (entry && app->channel_count < MAX_CHANNELS)
  {
    // Parse "id:name:description:is_public" format
    char *id_str = strtok(entry, ":");
    char *name = id_str ? strtok(NULL, ":") : NULL;
    char *description = name ? strtok(NULL, ":") : NULL;
    char *is_public_str = description ? strtok(NULL, ":") : NULL;

    if (id_str && name)
    {
      int channel_id = atoi(id_str);
      bool is_public = is_public_str ? (atoi(is_public_str) != 0) : true;

      // Store channel in app's channel array
      app->channels[app->channel_count].channel_id = channel_id;
      strncpy(app->channels[app->channel_count].name, name, MAX_CHANNEL_NAME_LEN - 1);
      app->channels[app->channel_count].name[MAX_CHANNEL_NAME_LEN - 1] = '\0';

      if (description)
      {
        strncpy(app->channels[app->channel_count].description, description, BUFFER_SIZE - 1);
        app->channels[app->channel_count].description[BUFFER_SIZE - 1] = '\0';
      }
      else
      {
        app->channels[app->channel_count].description[0] = '\0';
      }

      app->channels[app->channel_count].is_public = is_public;
      app->channel_count++;

      g_print("Added channel: ID=%d, Name=%s, Public=%d\n",
              channel_id, name, is_public ? 1 : 0);
    }

    // Get next entry
    entry = strtok_r(NULL, ",", &saveptr);
  }

  g_free(data_copy);

  // Update the channel list UI
  update_channel_list(app);
  return;

ensure_general:
  // Ensure at least the General channel exists
  if (app->channel_count == 0)
  {
    app->channels[0].channel_id = 0;
    strncpy(app->channels[0].name, "General", MAX_CHANNEL_NAME_LEN - 1);
    strncpy(app->channels[0].description, "Default channel", BUFFER_SIZE - 1);
    app->channels[0].is_public = true;
    app->channel_count = 1;

    // Update the channel list UI
    update_channel_list(app);
  }
}

// Helper for updating status label
static void update_status_label(AppContext *app, const char *text)
{
  if (!app || !app->status_bar)
  {
    return;
  }

  gtk_statusbar_pop(GTK_STATUSBAR(app->status_bar), 0);
  gtk_statusbar_push(GTK_STATUSBAR(app->status_bar), 0, text);
}

// Create and configure the channel section of the UI
static void setup_channel_section(AppContext *app, GtkWidget *sidebar)
{
  // Channels section title
  GtkWidget *channels_title = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(channels_title), "<b>Channels</b>");
  gtk_widget_set_halign(channels_title, GTK_ALIGN_START);
  gtk_widget_set_margin_start(channels_title, 5);
  gtk_widget_set_margin_top(channels_title, 5);
  gtk_box_pack_start(GTK_BOX(sidebar), channels_title, FALSE, FALSE, 5);

  // Channel section container with buttons
  GtkWidget *channels_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start(GTK_BOX(sidebar), channels_header, FALSE, FALSE, 0);

  // Add button to create new channel
  GtkWidget *add_channel_button = gtk_button_new_from_icon_name("list-add-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_tooltip_text(add_channel_button, "Create New Channel");
  g_signal_connect(add_channel_button, "clicked", G_CALLBACK(on_create_channel_clicked), app);
  gtk_box_pack_end(GTK_BOX(channels_header), add_channel_button, FALSE, FALSE, 5);

  // Create channels list box
  GtkWidget *channels_scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(channels_scroll),
                                 GTK_POLICY_NEVER,
                                 GTK_POLICY_AUTOMATIC);
  gtk_widget_set_vexpand(channels_scroll, TRUE);

  app->channel_list = gtk_list_box_new();
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(app->channel_list), GTK_SELECTION_SINGLE);
  g_signal_connect(app->channel_list, "row-activated", G_CALLBACK(on_channel_row_activated), app);

  gtk_container_add(GTK_CONTAINER(channels_scroll), app->channel_list);
  gtk_box_pack_start(GTK_BOX(sidebar), channels_scroll, TRUE, TRUE, 0);

  // Request channel list when connected
  if (app->client.connected)
  {
    g_timeout_add(500, request_channel_list, app);
  }
}