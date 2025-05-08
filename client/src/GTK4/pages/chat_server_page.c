#include <gtk/gtk.h>
#include "../ui_utils.h"

GtkWidget* create_chat_server_page(GtkWidget *stack) {
    // Main container
    GtkWidget *main_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(main_container, TRUE);
    
    // Left panel (Friends list)
    GtkWidget *left_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(left_panel, 200, -1);
    gtk_widget_add_css_class(left_panel, "left-panel");
    
    // Friends list title
    GtkWidget *friends_title = gtk_label_new("Friends list");
    gtk_widget_add_css_class(friends_title, "friends-title");
    gtk_widget_set_halign(friends_title, GTK_ALIGN_START);
    gtk_widget_set_valign(friends_title, GTK_ALIGN_START);
    gtk_widget_set_margin_start(friends_title, 10);
    gtk_widget_set_margin_top(friends_title, 10);
    
    // Container for friends list
    GtkWidget *friends_list_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_top(friends_list_container, 10);
    
    // Friends list (5 profiles)
    for(int i = 0; i < 5; i++) {
        GtkWidget *friend_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_add_css_class(friend_row, "friend-row");
        
        GtkWidget *avatar = gtk_image_new_from_file("client/src/GTK4/model/profil.png");
        gtk_widget_set_size_request(avatar, 40, 40);
        
        gtk_box_append(GTK_BOX(friend_row), avatar);
        gtk_box_append(GTK_BOX(friends_list_container), friend_row);
    }
    
    // User profile button at bottom left
    GtkWidget *user_profile = gtk_button_new_with_label("User Profile");  
    gtk_widget_add_css_class(user_profile, "user-profile-btn");
    gtk_widget_set_halign(user_profile, GTK_ALIGN_START);
    gtk_widget_set_margin_start(user_profile, 10);
    gtk_widget_set_vexpand(user_profile, TRUE);
    gtk_widget_set_valign(user_profile, GTK_ALIGN_END);
    
    // Left panel assembly
    gtk_box_append(GTK_BOX(left_panel), friends_title);
    gtk_box_append(GTK_BOX(left_panel), friends_list_container);
    gtk_box_append(GTK_BOX(left_panel), user_profile);
    
    // Center panel (Chat)
    GtkWidget *center_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(center_panel, TRUE);
    
    // Top buttons
    GtkWidget *top_buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(top_buttons, 10);
    gtk_widget_set_margin_top(top_buttons, 10);
    
    // Back button and inbox
    GtkWidget *back_btn = gtk_button_new_with_label("Back");
    gtk_widget_add_css_class(back_btn, "back-4");
    
    GtkWidget *inbox_img = gtk_image_new_from_file("client/src/GTK4/model/inbox.png");
    gtk_widget_set_size_request(inbox_img, 24, 24);
    GtkWidget *inbox_btn = gtk_button_new();
    gtk_button_set_child(GTK_BUTTON(inbox_btn), inbox_img);
    gtk_widget_add_css_class(inbox_btn, "inbox-btn");
    
    // Message area
    GtkWidget *message_area = gtk_text_view_new();
    gtk_widget_add_css_class(message_area, "message-area");
    gtk_widget_set_vexpand(message_area, TRUE);
    
    // Right panel (Server users)
    GtkWidget *right_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(right_panel, 200, -1);
    gtk_widget_add_css_class(right_panel, "right-panel");
    
    // Server image
    GtkWidget *server_img = gtk_image_new_from_file("client/src/GTK4/model/pas_content.png");
    gtk_widget_set_size_request(server_img, 150, 150);
    gtk_widget_set_halign(server_img, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(server_img, 20);
    
    // Separator
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_add_css_class(separator, "server-separator");
    gtk_widget_set_margin_top(separator, 30);
    gtk_widget_set_margin_bottom(separator, 30);
    
    // User list in server title
    GtkWidget *user_list_title = gtk_label_new("User List in Server"); 
    gtk_widget_add_css_class(user_list_title, "user-list-title");
    gtk_widget_set_margin_top(user_list_title, 10);
    
    // Assembly
    gtk_box_append(GTK_BOX(top_buttons), back_btn);
    gtk_box_append(GTK_BOX(top_buttons), inbox_btn);
    gtk_box_append(GTK_BOX(center_panel), top_buttons);
    gtk_box_append(GTK_BOX(center_panel), message_area);
    
    gtk_box_append(GTK_BOX(right_panel), server_img);
    gtk_box_append(GTK_BOX(right_panel), separator);
    gtk_box_append(GTK_BOX(right_panel), user_list_title);
    
    gtk_box_append(GTK_BOX(main_container), left_panel);
    gtk_box_append(GTK_BOX(main_container), center_panel);
    gtk_box_append(GTK_BOX(main_container), right_panel);
    
    // Make sure this name matches the one used in switch_to_server
    gtk_stack_add_named(GTK_STACK(stack), main_container, "server");
    
    return main_container;
}