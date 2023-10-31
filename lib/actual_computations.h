#ifndef ACTUAL_COMPUTATIONS_H
#define ACTUAL_COMPUTATIONS_H

#include <gtk/gtk.h>
#include "tie_structs.h"

// compute bias
void on_compute_bias(GtkWidget *button, gpointer data);
// convert counts to mgals for one timestamped thing and given calibration table
double convert_counts_mgals (val_time c1, calibration calib);
// do the land tie
void on_compute_landtie(GtkWidget *button, gpointer data);

#endif
