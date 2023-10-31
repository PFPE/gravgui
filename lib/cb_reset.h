#ifndef CB_RESET_H
#define CB_RESET_H

#include <gtk/gtk.h>

// callback function for ship reset button
void on_ship_reset(GtkWidget *widget, gpointer data);
// callback function for station reset button
void on_sta_reset(GtkWidget *widget, gpointer data);
// callback function for absolute grav reset button
void on_grav_reset(GtkWidget *widget, gpointer data);
// callback function for personnel reset button
void on_pers_reset(GtkWidget *widget, gpointer data);
// callback function for meter reset button
void on_lm_reset(GtkWidget *widget, gpointer data);
// callback function for reseting ship coordinates for a land tie
void on_lm_coordreset_button(GtkWidget *widget, gpointer data);
// callback function for clicking a *reset* button for a timestamp+value set
void on_reset_button(GtkWidget *widget, gpointer data);
// clear any DGS data read into shinfo vectors (don't clear ship though)
void on_dgs_clear_clicked(GtkWidget *button, gpointer data);
// clear bias calculated (not super necessary, can always recalculate?)
void on_clear_bias(GtkWidget *button, gpointer data);
// callback function for land tie toggle switch: bool, active/inactive
void landtie_switch_callback(GtkSwitch *switch_widget, gboolean state, gpointer data);

#endif
