#include <gtk/gtk.h>
#include "../ui_utils.h"

static void switch_to_connect(GtkButton *button, GtkStack *stack) {
    gtk_stack_set_visible_child_name(stack, "connect");
}

GtkWidget* create_register_page(GtkWidget *stack) {
    // Main container that fills the window
    GtkWidget *main_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(main_container, TRUE);
    gtk_widget_set_vexpand(main_container, TRUE);

    // Centered form container
    GtkWidget *form_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_add_css_class(form_box, "login-box"); 
    gtk_widget_set_halign(form_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(form_box, GTK_ALIGN_CENTER);

    // "Register" title
    GtkWidget *title = gtk_label_new("Register");
    gtk_widget_add_css_class(title, "text-14");
    gtk_widget_set_margin_bottom(title, 25);

    // All input fields with appropriate CSS class
    GtkWidget *firstname = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(firstname), "First name");
    gtk_widget_set_margin_bottom(firstname, 15);
    gtk_widget_add_css_class(firstname, "input-field");  

    GtkWidget *pseudo = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(pseudo), "Pseudo");
    gtk_widget_set_margin_bottom(pseudo, 15);
    gtk_widget_add_css_class(pseudo, "input-field");  

    GtkWidget *email = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(email), "Email");
    gtk_widget_set_margin_bottom(email, 15);
    gtk_widget_add_css_class(email, "input-field");  

    GtkWidget *password = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(password), "Password");
    gtk_entry_set_visibility(GTK_ENTRY(password), FALSE);
    gtk_widget_set_margin_bottom(password, 25);
    gtk_widget_add_css_class(password, "input-field");  

    // Buttons
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 40);
    gtk_widget_set_halign(button_box, GTK_ALIGN_CENTER);

    GtkWidget *back_btn = gtk_button_new_with_label("Connect");  
    gtk_widget_add_css_class(back_btn, "back-4");
    g_signal_connect(back_btn, "clicked", G_CALLBACK(switch_to_connect), stack);

    GtkWidget *enter_btn = gtk_button_new_with_label("Enter");
    gtk_widget_add_css_class(enter_btn, "enter-2");

    // Assembly - Add all widgets in order
    gtk_box_append(GTK_BOX(form_box), title);
    gtk_box_append(GTK_BOX(form_box), firstname);
    gtk_box_append(GTK_BOX(form_box), pseudo);
    gtk_box_append(GTK_BOX(form_box), email);
    gtk_box_append(GTK_BOX(form_box), password);
    gtk_box_append(GTK_BOX(button_box), back_btn);
    gtk_box_append(GTK_BOX(button_box), enter_btn);
    gtk_box_append(GTK_BOX(form_box), button_box);

    // Final assembly
    gtk_box_append(GTK_BOX(main_container), form_box);
    
    gtk_stack_add_named(GTK_STACK(stack), main_container, "register");
    
    return main_container;
}