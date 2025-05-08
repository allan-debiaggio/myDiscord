#include <gtk/gtk.h>
#include "../ui_utils.h"

static void show_add_friend_dialog(GtkButton *button, GtkWidget *parent_window) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Add Friend",
        GTK_WINDOW(gtk_widget_get_root(parent_window)),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Cancel",
        GTK_RESPONSE_CANCEL,
        "Add Friend",
        GTK_RESPONSE_ACCEPT,
        NULL
    );

    // Main dialog container
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_widget_add_css_class(content_area, "add-friend-content");
    
    // "Add Friend" title
    GtkWidget *title = gtk_label_new("Add Friend");
    gtk_widget_add_css_class(title, "dialog-title");
    
    // Description
    GtkWidget *description = gtk_label_new("You can add friends with their Dispute Username.");
    gtk_widget_add_css_class(description, "dialog-description");
    
    // Input field
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Enter a Username");
    gtk_widget_add_css_class(entry, "dialog-entry");

    gtk_box_append(GTK_BOX(content_area), title);
    gtk_box_append(GTK_BOX(content_area), description);
    gtk_box_append(GTK_BOX(content_area), entry);

    gtk_widget_add_css_class(dialog, "add-friend-dialog");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 400, 200);

    gtk_widget_show(dialog);
}

static void switch_to_server(GtkButton *button, GtkStack *stack) {
    gtk_stack_set_visible_child_name(stack, "server");
}

GtkWidget* create_chat_friends_page(GtkWidget *stack) {
    // Conteneur principal
    GtkWidget *main_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(main_container, TRUE);
    
    // Panneau gauche (Friends list)
    GtkWidget *left_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(left_panel, 200, -1);
    gtk_widget_add_css_class(left_panel, "left-panel");
    
    // Titre Friends list avec alignement à gauche
    GtkWidget *friends_title = gtk_label_new("Friends list");
    gtk_widget_add_css_class(friends_title, "friends-title");
    gtk_widget_set_halign(friends_title, GTK_ALIGN_START);
    gtk_widget_set_margin_start(friends_title, 10);
    
    // User profile en bas à gauche
    GtkWidget *user_profile = gtk_button_new_with_label("User profile");
    gtk_widget_add_css_class(user_profile, "user-profile-btn");
    gtk_widget_set_halign(user_profile, GTK_ALIGN_START);
    gtk_widget_set_margin_start(user_profile, 10);
    gtk_widget_set_vexpand(user_profile, TRUE);
    gtk_widget_set_valign(user_profile, GTK_ALIGN_END);

    // Ajout des éléments au panneau gauche dans le bon ordre
    gtk_box_append(GTK_BOX(left_panel), friends_title);
    
    // Liste des amis (5 profils)
    for(int i = 0; i < 5; i++) {
        GtkWidget *friend_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_add_css_class(friend_row, "friend-row");
        
        GtkWidget *avatar = gtk_image_new_from_file("client/src/GTK4/model/profil.png");
        gtk_widget_set_size_request(avatar, 40, 40);
        
        gtk_box_append(GTK_BOX(friend_row), avatar);
        gtk_box_append(GTK_BOX(left_panel), friend_row);
    }
    
    // Ajouter le bouton user profile en dernier
    gtk_box_append(GTK_BOX(left_panel), user_profile);
    
    // Panneau central (Chat)
    GtkWidget *center_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(center_panel, TRUE);
    
    // Boutons en haut
    GtkWidget *top_buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(top_buttons, 10);
    gtk_widget_set_margin_top(top_buttons, 10);
    
    // Bouton Server (ancien bouton Back)
    GtkWidget *server_btn = gtk_button_new_with_label("Server");
    gtk_widget_add_css_class(server_btn, "back-4");
    g_signal_connect(server_btn, "clicked", G_CALLBACK(switch_to_server), stack);
    
    // Image inbox
    GtkWidget *inbox_img = gtk_image_new_from_file("client/src/GTK4/model/inbox.png");
    gtk_widget_set_size_request(inbox_img, 50, 50); 
    GtkWidget *inbox_btn = gtk_button_new();
    gtk_button_set_child(GTK_BUTTON(inbox_btn), inbox_img);
    gtk_widget_add_css_class(inbox_btn, "inbox-btn");
    
    // Image add friend
    GtkWidget *add_friend_icon_img = gtk_image_new_from_file("client/src/GTK4/model/add_friend_icon.png");
    gtk_widget_set_size_request(add_friend_icon_img, 50, 50);
    GtkWidget *add_friend_icon_btn = gtk_button_new();
    gtk_button_set_child(GTK_BUTTON(add_friend_icon_btn), add_friend_icon_img);
    gtk_widget_add_css_class(add_friend_icon_btn, "inbox-btn");
    g_signal_connect(add_friend_icon_btn, "clicked", G_CALLBACK(show_add_friend_dialog), main_container);
    
    gtk_box_append(GTK_BOX(top_buttons), server_btn);
    gtk_box_append(GTK_BOX(top_buttons), inbox_btn);
    gtk_box_append(GTK_BOX(top_buttons), add_friend_icon_btn);
    
    // Zone de message
    GtkWidget *message_area = gtk_text_view_new();
    gtk_widget_add_css_class(message_area, "message-area");
    gtk_widget_set_vexpand(message_area, TRUE);
    
    // Panneau droit (Friend Profile)
    GtkWidget *right_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(right_panel, 200, -1);
    
    GtkWidget *friend_profile_title = gtk_label_new("Friend Profile");
    gtk_widget_add_css_class(friend_profile_title, "friend-profile-title");
    gtk_widget_set_halign(friend_profile_title, GTK_ALIGN_CENTER); 

    GtkWidget *friend_avatar = gtk_image_new_from_file("client/src/GTK4/model/profil.png");
    gtk_widget_set_size_request(friend_avatar, 80, 80);
    gtk_widget_set_halign(friend_avatar, GTK_ALIGN_CENTER);  
    
    // Assemblage
    gtk_box_append(GTK_BOX(left_panel), friends_title);
    gtk_box_append(GTK_BOX(left_panel), user_profile);
    
    gtk_box_append(GTK_BOX(center_panel), top_buttons);
    gtk_box_append(GTK_BOX(center_panel), message_area);
    
    gtk_box_append(GTK_BOX(right_panel), friend_profile_title);
    gtk_box_append(GTK_BOX(right_panel), friend_avatar);
    
    gtk_box_append(GTK_BOX(main_container), left_panel);
    gtk_box_append(GTK_BOX(main_container), center_panel);
    gtk_box_append(GTK_BOX(main_container), right_panel);
    
    gtk_stack_add_named(GTK_STACK(stack), main_container, "chat");
    
    return main_container;
}