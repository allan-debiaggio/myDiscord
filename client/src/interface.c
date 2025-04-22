#include <gtk/gtk.h>
#include <stdio.h>

static void activate(GtkApplication *app, gpointer user_data) {
    // CSS pour fond noir et boutons ronds
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider,
        "window { background-color: #000; }"
        ".round-btn {"
        "  min-width: 80px; max-width: 80px;"
        "  min-height: 80px; max-height: 80px;"
        "  border-radius: 80px;"
        "  padding: 0;"
        "  font-size: 12px;"
        "  background-color: #444;"
        "  color: black;"
        "}"
        ".round-btn:hover {"
        "  background-color: #666;"
        "}"
        ".round-btn:active {"
        "  background-color: #222;"
        "}"
        ".btn-red { background-color: #e74c3c; }"
        ".btn-green { background-color: #2ecc71; }"
        ".btn-blue { background-color: #3498db; }"
        ".btn-red:hover { background-color: #c0392b; }"
        ".btn-green:hover { background-color: #27ae60; }"
        ".btn-blue:hover { background-color: #2980b9; }"
        "frame > label {"
        "font-size: 20px;"
        "margin-top: 20px;"
        "margin-bottom: 20px;"
        "margin-left: 40px;"
        "  color: white;"
        "}"
    );
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(provider);

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Frame");
    gtk_window_maximize(GTK_WINDOW(window));

    GtkWidget *fixed = gtk_fixed_new();
    gtk_window_set_child(GTK_WINDOW(window), fixed);

    GtkWidget *frame = gtk_frame_new("Serveur");
    gtk_widget_set_size_request(frame, 150, 750);
    gtk_fixed_put(GTK_FIXED(fixed), frame, 25, 25);

    GtkWidget *frame2 = gtk_frame_new("");
    gtk_widget_set_size_request(frame2, 1375, 100);
    gtk_fixed_put(GTK_FIXED(fixed), frame2, 200, 25);


    GtkWidget *top_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_halign(top_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(top_box, GTK_ALIGN_START);

    
    GtkWidget *button1 = gtk_button_new_with_label("LaPlateforme");
    GtkWidget *button2 = gtk_button_new_with_label("Serveur Public");
    GtkWidget *button3 = gtk_button_new_with_label("Serveur Privé");

    gtk_widget_add_css_class(button1, "round-btn");
    gtk_widget_add_css_class(button2, "round-btn");
    gtk_widget_add_css_class(button3, "round-btn");

    gtk_box_append(GTK_BOX(top_box), button1);
    gtk_box_append(GTK_BOX(top_box), button2);
    gtk_box_append(GTK_BOX(top_box), button3);

    gtk_frame_set_child(GTK_FRAME(frame), top_box);


    GtkWidget *center_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_halign(center_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(center_box, GTK_ALIGN_CENTER);

    GtkWidget *button4 = gtk_button_new_with_label("4");
    gtk_widget_add_css_class(button4, "round-btn");

    gtk_box_append(GTK_BOX(center_box), button4);
    gtk_frame_set_child(GTK_FRAME(frame2), center_box);

    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    printf("Hello World\n");
    GtkApplication *app = gtk_application_new("org.gtk.framepos", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    
    return status;
}