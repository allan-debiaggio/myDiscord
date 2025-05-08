#include <gtk/gtk.h>
#include <unistd.h>

// Structure pour stocker les widgets principaux
typedef struct {
    GtkWidget *stack;
    GtkWidget *login_page;
    GtkWidget *register_page;
    GtkWidget *main_page;
} AppWindows;

static void create_login_page(GtkWidget *stack) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(box, "login-box");
    
    // Logo avec vérification
    char absolute_path[1024];
    getcwd(absolute_path, sizeof(absolute_path));
    g_print("Dossier courant : %s\n", absolute_path);
    
    char logo_path[1024];
    snprintf(logo_path, sizeof(logo_path), "%s/%s", absolute_path, "../client/src/GTK4/model/logo_dispute.png");
    GtkWidget *logo = gtk_image_new_from_file(logo_path);
    if (gtk_image_get_storage_type(GTK_IMAGE(logo)) == GTK_IMAGE_EMPTY) {
        g_print("Erreur: Logo non trouvé à l'emplacement: client/src/GTK4/model/logo_dispute.png\n");
        // Essayer un autre chemin
        logo = gtk_image_new_from_file("../client/src/GTK4/model/logo_dispute.png");
        if (gtk_image_get_storage_type(GTK_IMAGE(logo)) == GTK_IMAGE_EMPTY) {
            g_print("Erreur: Logo non trouvé au second emplacement\n");
        }
    }
    
    // Champs de connexion
    GtkWidget *firstname = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(firstname), "First name");
    gtk_widget_set_margin_bottom(firstname, 10);
    
    GtkWidget *pseudo = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(pseudo), "Pseudo");
    gtk_widget_set_margin_bottom(pseudo, 10);
    
    GtkWidget *email = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(email), "Email");
    gtk_widget_set_margin_bottom(email, 10);
    
    GtkWidget *password = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(password), "Password");
    gtk_entry_set_visibility(GTK_ENTRY(password), FALSE);
    gtk_widget_set_margin_bottom(password, 20);
    
    // Boutons
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(button_box, GTK_ALIGN_CENTER);
    
    GtkWidget *back_btn = gtk_button_new_with_label("Back");
    GtkWidget *enter_btn = gtk_button_new_with_label("Enter");
    
    // Assemblage
    gtk_box_append(GTK_BOX(box), logo);
    gtk_box_append(GTK_BOX(box), firstname);
    gtk_box_append(GTK_BOX(box), pseudo);
    gtk_box_append(GTK_BOX(box), email);
    gtk_box_append(GTK_BOX(box), password);
    gtk_box_append(GTK_BOX(button_box), back_btn);
    gtk_box_append(GTK_BOX(button_box), enter_btn);
    gtk_box_append(GTK_BOX(box), button_box);
    
    gtk_stack_add_named(GTK_STACK(stack), box, "login");
}

static void create_register_page(GtkWidget *stack) {
    // Conteneur principal centré
    GtkWidget *center_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(center_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(center_box, GTK_ALIGN_CENTER);

    // Conteneur pour le formulaire
    GtkWidget *form_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_add_css_class(form_box, "frame-register-1");  // Changé ici

    // Logo avec vérification
    char absolute_path[1024];
    getcwd(absolute_path, sizeof(absolute_path));
    g_print("Dossier courant : %s\n", absolute_path);
    
    char logo_path[1024];
    snprintf(logo_path, sizeof(logo_path), "%s/%s", absolute_path, "../client/src/GTK4/model/logo_dispute.png");
    GtkWidget *logo = gtk_image_new_from_file(logo_path);
    if (gtk_image_get_storage_type(GTK_IMAGE(logo)) == GTK_IMAGE_EMPTY) {
        g_print("Erreur: Logo non trouvé à l'emplacement: client/src/GTK4/model/logo_dispute.png\n");
        // Essayer un autre chemin
        logo = gtk_image_new_from_file("../client/src/GTK4/model/logo_dispute.png");
        if (gtk_image_get_storage_type(GTK_IMAGE(logo)) == GTK_IMAGE_EMPTY) {
            g_print("Erreur: Logo non trouvé au second emplacement\n");
        }
    }

    // Titre "Register"
    GtkWidget *title = gtk_label_new("Register");
    gtk_widget_add_css_class(title, "text-14");
    gtk_widget_set_margin_bottom(title, 25);

    // Tous les champs de saisie avec la classe CSS appropriée
    GtkWidget *firstname = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(firstname), "First name");
    gtk_widget_set_margin_bottom(firstname, 15);
    gtk_widget_add_css_class(firstname, "input-field");  // Ajouté

    GtkWidget *pseudo = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(pseudo), "Pseudo");
    gtk_widget_set_margin_bottom(pseudo, 15);
    gtk_widget_add_css_class(pseudo, "input-field");  // Ajouté

    GtkWidget *email = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(email), "Email");
    gtk_widget_set_margin_bottom(email, 15);
    gtk_widget_add_css_class(email, "input-field");  // Ajouté

    GtkWidget *password = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(password), "Password");
    gtk_entry_set_visibility(GTK_ENTRY(password), FALSE);
    gtk_widget_set_margin_bottom(password, 25);
    gtk_widget_add_css_class(password, "input-field");  // Ajouté

    // Boutons
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 40);
    gtk_widget_set_halign(button_box, GTK_ALIGN_CENTER);

    GtkWidget *back_btn = gtk_button_new_with_label("Back");
    gtk_widget_add_css_class(back_btn, "back-4");

    GtkWidget *enter_btn = gtk_button_new_with_label("Enter");
    gtk_widget_add_css_class(enter_btn, "enter-2");

    // Assemblage - Ajout de tous les widgets dans l'ordre
    gtk_box_append(GTK_BOX(form_box), logo);
    gtk_box_append(GTK_BOX(form_box), title);
    gtk_box_append(GTK_BOX(form_box), firstname);
    gtk_box_append(GTK_BOX(form_box), pseudo);
    gtk_box_append(GTK_BOX(form_box), email);
    gtk_box_append(GTK_BOX(form_box), password);
    gtk_box_append(GTK_BOX(button_box), back_btn);
    gtk_box_append(GTK_BOX(button_box), enter_btn);
    gtk_box_append(GTK_BOX(form_box), button_box);

    gtk_box_append(GTK_BOX(center_box), form_box);
    gtk_stack_add_named(GTK_STACK(stack), center_box, "register");
}

static void load_css(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    
    gtk_css_provider_load_from_path(provider, "myDiscord/client/src/GTK4/style.css");
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
    gtk_window_set_title(GTK_WINDOW(window), "Dispute");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

    stack = gtk_stack_new();
    create_register_page(stack);

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