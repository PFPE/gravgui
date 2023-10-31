#ifndef CB_SAVE_H
#define CB_SAVE_H

#include <gtk/gtk.h>
// callback function for clicking a button and saving a value+timestamp
void on_timestamp_button(GtkWidget *widget, gpointer data);
// callback function for ship save button
void on_ship_save(GtkWidget *widget, gpointer data);
// callback function for ship save button
void on_sta_save(GtkWidget *widget, gpointer data);
// callback function for absolute grav save button
void on_grav_save(GtkWidget *widget, gpointer data);
// callback function for personnel save button
void on_pers_save(GtkWidget *widget, gpointer data);
// callback function for meter save button
void on_lm_save(GtkWidget *widget, gpointer data);
// callback function for saving ship coordinates for a land tie
void on_lm_coordsave_button(GtkWidget *widget, gpointer data);
#endif
