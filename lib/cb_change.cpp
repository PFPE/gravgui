#include <gtk/gtk.h>
#include <string>
#include "tie_structs.h"

// callback function for ship dropdown list to store selected ship
void on_ship_changed(GtkComboBox *widget, gpointer data) {
    GtkTreeIter iter;
    GtkTreeModel *model;
    gchar *value;
    ship_info* shinfo = static_cast<ship_info*>(data);

    // Get the selected item from the combo box
    model = gtk_combo_box_get_model(widget);
    if (gtk_combo_box_get_active_iter(widget, &iter)) {
        gtk_tree_model_get(model, &iter, 0, &value, -1);
        std::string selected_value = value;  // cast gchar to string
        if (selected_value == "Other") {  // need to enter ship in box
            gtk_widget_set_sensitive(GTK_WIDGET(shinfo->en1), TRUE);
        } else {
            gtk_widget_set_sensitive(GTK_WIDGET(shinfo->en1), FALSE); // just in case
        }
        shinfo->ship = value;  // save in all cases, will deal with save callback separately
                               // and that will overwrite if needed
        g_free(value); // Free the allocated memory for value
    }
}

// callback function for station dropdown list to store selected station
void on_sta_changed(GtkComboBox *widget, gpointer data) {
    GtkTreeIter iter;
    GtkTreeModel *model;
    gchar *value;

    sta_info* stinfo = static_cast<sta_info*>(data);

    // Get the selected item from the combo box
    model = gtk_combo_box_get_model(widget);
    if (gtk_combo_box_get_active_iter(widget, &iter)) {
        gtk_tree_model_get(model, &iter, 0, &value, -1);
        std::string selected_value = value;  // cast gchar to string
        if (selected_value == "Other") {  // need to enter ship in box
            gtk_widget_set_sensitive(GTK_WIDGET(stinfo->en1), TRUE);
            //std::cout << "Selected item: " << value << std::endl;
        } else {
            gtk_widget_set_sensitive(GTK_WIDGET(stinfo->en1), FALSE); // just in case
        }
        stinfo->station = value;  // save in all cases, will deal with save callback separately
                               // and that will overwrite if needed
        g_free(value); // Free the allocated memory for value
    }
}

// callback function for meter dropdown list to store selected meter
void on_lm_changed(GtkComboBox *widget, gpointer data) {
    GtkTreeIter iter;
    GtkTreeModel *model;
    gchar *value;

    lm_info* lminfo = static_cast<lm_info*>(data);

    // Get the selected item from the combo box
    model = gtk_combo_box_get_model(widget);
    if (gtk_combo_box_get_active_iter(widget, &iter)) {
        gtk_tree_model_get(model, &iter, 0, &value, -1);
        std::string selected_value = value;  // cast gchar to string
        if (selected_value == "Other") {  // need to enter alt name in box
            gtk_widget_set_sensitive(GTK_WIDGET(lminfo->en1), TRUE);
            //std::cout << "Selected item: " << value << std::endl;
        } else {
            gtk_widget_set_sensitive(GTK_WIDGET(lminfo->en1), FALSE); // just in case
        }
        lminfo->meter = value;  // save in all cases, will deal with save callback separately
                               // and that will overwrite if needed
        g_free(value); // Free the allocated memory for value
    }
}


