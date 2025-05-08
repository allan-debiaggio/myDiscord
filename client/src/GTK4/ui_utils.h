#ifndef UI_UTILS_H
#define UI_UTILS_H

#include <gtk/gtk.h>

// Function declarations
GtkWidget* create_connect_page(GtkWidget *stack);
GtkWidget* create_register_page(GtkWidget *stack);
GtkWidget* create_chat_friends_page(GtkWidget *stack);  
GtkWidget* create_chat_server_page(GtkWidget *stack);

#endif