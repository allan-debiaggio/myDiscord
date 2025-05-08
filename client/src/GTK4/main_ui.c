#include <gtk/gtk.h>
#include <unistd.h>
#include "ui_utils.h"

// Structure to store main widgets
typedef struct {
    GtkWidget *stack;
    GtkWidget *login_page;
    GtkWidget *register_page;
    GtkWidget *main_page;
} AppWindows;


static void load_css(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    
    gtk_css_provider_load_from_path(provider, "../client/src/GTK4/style.css");
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(provider);
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *stack;

    load_css();

    window = gtk_application_window_new(app);
    gtk_widget_add_css_class(GTK_WIDGET(window), "window");  
    gtk_window_set_title(GTK_WINDOW(window), "Dispute");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

    stack = gtk_stack_new();
    
    create_register_page(stack);
    create_connect_page(stack);
    create_chat_friends_page(stack);
    create_chat_server_page(stack);  
    
    // Set register as default page
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "register");
    
    gtk_window_set_child(GTK_WINDOW(window), stack);
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;
    
    app = gtk_application_new("com.dispute.app", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    
    return status;
}