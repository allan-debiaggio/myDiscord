#include <gtk/gtk.h>
#include "../ui_utils.h"

static void switch_to_register(GtkButton *button, GtkStack *stack) {
    gtk_stack_set_visible_child_name(stack, "register");
}

static void switch_to_chat(GtkButton *button, GtkStack *stack) {
    gtk_stack_set_visible_child_name(stack, "chat");
}

GtkWidget* create_connect_page(GtkWidget *stack) {
    // Main centered container
    GtkWidget *center_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(center_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(center_box, GTK_ALIGN_CENTER);

    // Main container with background
    GtkWidget *main_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(main_container, "connect-container");
    gtk_widget_set_hexpand(main_container, TRUE);
    gtk_widget_set_vexpand(main_container, TRUE);

    // Form box
    GtkWidget *form_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(form_box, "connect-box");
    gtk_widget_set_halign(form_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(form_box, GTK_ALIGN_CENTER);

    // "Connect" title
    GtkWidget *title = gtk_label_new("Connect");
    gtk_widget_add_css_class(title, "text-14");
    gtk_widget_set_margin_bottom(title, 25);
    
    // Login fields
    GtkWidget *pseudo = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(pseudo), "Pseudo");
    gtk_widget_set_margin_bottom(pseudo, 15);
    gtk_widget_add_css_class(pseudo, "input-field");
    
    GtkWidget *password = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(password), "Password");
    gtk_entry_set_visibility(GTK_ENTRY(password), FALSE);
    gtk_widget_set_margin_bottom(password, 25);
    gtk_widget_add_css_class(password, "input-field");
    
    // Buttons
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 40);
    gtk_widget_set_halign(button_box, GTK_ALIGN_CENTER);

    GtkWidget *register_btn = gtk_button_new_with_label("Register");
    gtk_widget_add_css_class(register_btn, "back-4");
    g_signal_connect(register_btn, "clicked", G_CALLBACK(switch_to_register), stack);

    GtkWidget *connect_btn = gtk_button_new_with_label("Connect");
    gtk_widget_add_css_class(connect_btn, "enter-2");
    g_signal_connect(connect_btn, "clicked", G_CALLBACK(switch_to_chat), stack);
    
    // Assembly - Removed logo line
    gtk_box_append(GTK_BOX(center_box), form_box);
    gtk_box_append(GTK_BOX(form_box), title);
    gtk_box_append(GTK_BOX(form_box), pseudo);
    gtk_box_append(GTK_BOX(form_box), password);
    gtk_box_append(GTK_BOX(button_box), register_btn);
    gtk_box_append(GTK_BOX(button_box), connect_btn);
    gtk_box_append(GTK_BOX(form_box), button_box);

    gtk_box_append(GTK_BOX(main_container), center_box);
    
    // Add to stack
    gtk_stack_add_named(GTK_STACK(stack), main_container, "connect");
    
    return main_container;
}