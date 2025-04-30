#include "../../include/gtk_client.h"

extern AppContext app;
extern void *gtk_receive_messages(void *arg);

// Function to load CSS for styling
static void load_css(void)
{
  GtkCssProvider *provider;
  GdkDisplay *display;
  GdkScreen *screen;

  provider = gtk_css_provider_new();
  display = gdk_display_get_default();
  screen = gdk_display_get_default_screen(display);

  gtk_style_context_add_provider_for_screen(screen,
                                            GTK_STYLE_PROVIDER(provider),
                                            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  GError *error = NULL;
  gtk_css_provider_load_from_file(provider, g_file_new_for_path("assets/style.css"), &error);

  if (error)
  {
    g_warning("Failed to load CSS: %s", error->message);
    g_error_free(error);
  }

  g_object_unref(provider);
}

int main(int argc, char *argv[])
{
  // Initialize GTK
  gtk_init(&argc, &argv);

  // Load CSS styles
  load_css();

  // Initialize app
  initialize_app(&app);

  // Create login window
  create_login_window(&app);

  // Start GTK main loop
  gtk_main();

  return 0;
}