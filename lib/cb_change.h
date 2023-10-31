#ifndef CB_CHANGE_H
#define CB_CHANGE_H

#include <gtk/gtk.h>

// callback function for ship dropdown list to store selected ship
void on_ship_changed(GtkComboBox *widget, gpointer data);
// callback function for station dropdown list to store selected station
void on_sta_changed(GtkComboBox *widget, gpointer data);
// callback function for meter dropdown list to store selected meter
void on_lm_changed(GtkComboBox *widget, gpointer data);

#endif
