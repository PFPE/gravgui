#include <gtk/gtk.h>
#include <vector>
#include <sstream>
#include <iomanip>
#include "tie_structs.h"

// callback function for ship reset button
void on_ship_reset(GtkWidget *widget, gpointer data) {
    // Cast data back to a pointer
    ship_info* shinfo = static_cast<ship_info*>(data);
    // clear entry field if not already clear
    gtk_entry_set_text(GTK_ENTRY(shinfo->en1), "");
    // turn off entry field
    gtk_widget_set_sensitive(GTK_WIDGET(shinfo->en1), FALSE);
    // turn on combobox and reset to unselected
    gtk_widget_set_sensitive(GTK_WIDGET(shinfo->cb1), TRUE);
    gtk_combo_box_set_active(GTK_COMBO_BOX(shinfo->cb1), -1);
    // clear shinfo.ship
    shinfo->ship = "";
    shinfo->alt_ship = "";
    // reset buttons to on
    gtk_widget_set_sensitive(GTK_WIDGET(shinfo->bt1), TRUE);
}

// callback function for station reset button
void on_sta_reset(GtkWidget *widget, gpointer data) {
    // Cast data back to a pointer
    sta_info* stinfo = static_cast<sta_info*>(data);
    // clear entry field if not already clear
    gtk_entry_set_text(GTK_ENTRY(stinfo->en1), "");
    // turn off entry field
    gtk_widget_set_sensitive(GTK_WIDGET(stinfo->en1), FALSE);
    // turn on combobox and reset to unselected
    gtk_widget_set_sensitive(GTK_WIDGET(stinfo->cb1), TRUE);
    gtk_combo_box_set_active(GTK_COMBO_BOX(stinfo->cb1), -1);
    // clear stinfo saved values
    stinfo->station = "";
    stinfo->alt_station = "";
    // reset buttons to on
    gtk_widget_set_sensitive(GTK_WIDGET(stinfo->bt1), TRUE);
    // reset grav entry stuff to off and clear it
    gtk_widget_set_sensitive(GTK_WIDGET(stinfo->bt3), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(stinfo->bt4), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(stinfo->en2), FALSE);
    gtk_entry_set_text(GTK_ENTRY(stinfo->en2), "");
    stinfo->station_gravity = -999;
}

// callback function for absolute grav reset button
void on_grav_reset(GtkWidget *widget, gpointer data) {
    // Cast data back to a pointer
    sta_info* stinfo = static_cast<sta_info*>(data);
    // clear entry field if not already clear
    gtk_entry_set_text(GTK_ENTRY(stinfo->en2), "");
    gtk_widget_set_sensitive(GTK_WIDGET(stinfo->en2), TRUE);
    // clear stinfo saved values
    stinfo->station_gravity = -999;
    // reset buttons to on
    gtk_widget_set_sensitive(GTK_WIDGET(stinfo->bt3), TRUE);
}

// callback function for personnel reset button
void on_pers_reset(GtkWidget *widget, gpointer data) {
    // Cast data back to a pointer
    pers_info* prinfo = static_cast<pers_info*>(data);
    // clear entry field if not already clear
    gtk_entry_set_text(GTK_ENTRY(prinfo->en1), "");
    // clear saved personnel name if any
    prinfo->personnel = "";
    // reset buttons and entry field to on
    gtk_widget_set_sensitive(GTK_WIDGET(prinfo->en1), TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(prinfo->bt1), TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(prinfo->bt2), TRUE);
}

// callback function for meter reset button
void on_lm_reset(GtkWidget *widget, gpointer data) {
    // Cast data back to a pointer
    lm_info* lminfo = static_cast<lm_info*>(data);
    // clear entry field if not already clear
    gtk_entry_set_text(GTK_ENTRY(lminfo->en1), "");
    // turn off entry field
    gtk_widget_set_sensitive(GTK_WIDGET(lminfo->en1), FALSE);
    // turn on combobox and reset to unselected
    gtk_widget_set_sensitive(GTK_WIDGET(lminfo->cb1), TRUE);
    gtk_combo_box_set_active(GTK_COMBO_BOX(lminfo->cb1), -1);
    // clear lminfo saved values
    lminfo->meter = "";
    lminfo->alt_meter = "";
    lminfo->cal_file_path = "";
    lminfo->calib.brackets.clear();
    lminfo->calib.factors.clear();
    lminfo->calib.mgvals.clear();
    // reset buttons to on and off as needed
    gtk_widget_set_sensitive(GTK_WIDGET(lminfo->bt1), TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(lminfo->bt_cal_file), FALSE);
    // reset text in grid
    gtk_label_set_text(GTK_LABEL(lminfo->cal_label), "0 calibration lines read");
}

// callback function for reseting ship coordinates for a land tie
void on_lm_coordreset_button(GtkWidget *widget, gpointer data) {
    lm_info* lminfo = static_cast<lm_info*>(data);
    lminfo->ship_lon = -999;
    lminfo->ship_lat = -999;
    lminfo->ship_elev = -999;
    lminfo->meter_temp = -999;
    gtk_entry_set_text(GTK_ENTRY(lminfo->en_lon), "");
    gtk_entry_set_text(GTK_ENTRY(lminfo->en_lat), "");
    gtk_entry_set_text(GTK_ENTRY(lminfo->en_elev), "");
    gtk_entry_set_text(GTK_ENTRY(lminfo->en_temp), "");
    gtk_widget_set_sensitive(GTK_WIDGET(lminfo->en_lon), TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(lminfo->en_lat), TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(lminfo->en_elev), TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(lminfo->en_temp), TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(lminfo->bt_coords), TRUE);
}

// callback function for clicking a *reset* button for a timestamp+value set
void on_reset_button(GtkWidget *widget, gpointer data) {
    // Cast data back to a pointer to MyData
    val_time* hval = static_cast<val_time*>(data);

    // re-activate the entry field and the button
    gtk_widget_set_sensitive(GTK_WIDGET(hval->en1), TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(hval->bt1), TRUE);

    // clear the entry text
    gtk_entry_set_text(GTK_ENTRY(hval->en1), "");

    // clear the values that were previously saved in the tie
    hval->h1 = -999;
    hval->t1 = -1;
}

// clear any DGS data read into shinfo vectors (don't clear ship though)
void on_dgs_clear_clicked(GtkWidget *button, gpointer data) {
    ship_info* shinfo = static_cast<ship_info*>(data);
    shinfo->gravgrav.clear();
    shinfo->gravtime.clear();
    gtk_label_set_text(GTK_LABEL(shinfo->dgs_label), "  0 datapoints"); 
}

// clear bias calculated (not super necessary, can always recalculate?)
void on_clear_bias(GtkWidget *button, gpointer data) {
    tie* gravtie = static_cast<tie*>(data);  // we def need the whole tie for this one

    gravtie->bias = -999;
    gravtie->avg_dgs_grav = -99999;
    gravtie->water_grav = -999;

    std::stringstream strm;
    strm << std::fixed << std::setprecision(2) << gravtie->bias;
    std::string test = strm.str();
    char bstring[test.length()+16];
    sprintf(bstring,"Computed bias: ");
    gtk_label_set_text(GTK_LABEL(gravtie->bias_label), bstring);
}

// callback function for land tie toggle switch: bool, active/inactive
void landtie_switch_callback(GtkSwitch *switch_widget, gboolean state, gpointer data) {
    tie* gravtie = static_cast<tie*>(data);
    if (state) {
        gravtie->lminfo.landtie = TRUE;

        if (gravtie->lminfo.ship_lon < 0) {
            gtk_widget_set_sensitive(GTK_WIDGET(gravtie->lminfo.en_lon), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(gravtie->lminfo.en_lat), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(gravtie->lminfo.en_elev), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(gravtie->lminfo.en_temp), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(gravtie->lminfo.bt_coords), TRUE);
        }
        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->lminfo.br_coords), TRUE);

        std::vector<std::vector<val_time> > allcounts{gravtie->acounts, gravtie->bcounts, gravtie->ccounts};
        for (std::vector<val_time>& thesecounts : allcounts) {
            for (val_time& thisone : thesecounts) {
                gtk_widget_set_sensitive(GTK_WIDGET(thisone.bt2), TRUE);  // turn on all reset buttons
                if (thisone.h1 < 0) { // only turn on entry field and save for un-filled fields
                    gtk_widget_set_sensitive(GTK_WIDGET(thisone.en1), TRUE);
                    gtk_widget_set_sensitive(GTK_WIDGET(thisone.bt1), TRUE);
                }
            }
        }

        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->lt_button), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->lminfo.bt2), TRUE);
        if (gravtie->lminfo.meter == ""){  // only set sens if no meter yet selected
            gtk_widget_set_sensitive(GTK_WIDGET(gravtie->lminfo.cb1), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(gravtie->lminfo.bt1), TRUE);
        }
    } else {
        gravtie->lminfo.landtie = FALSE;

        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->lminfo.en_lon), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->lminfo.en_lat), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->lminfo.en_elev), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->lminfo.en_temp), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->lminfo.bt_coords), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->lminfo.br_coords), FALSE);

        std::vector<std::vector<val_time> > allcounts{gravtie->acounts, gravtie->bcounts, gravtie->ccounts};
        for (std::vector<val_time>& thesecounts : allcounts) {
            for (val_time& thisone : thesecounts) {
                gtk_widget_set_sensitive(GTK_WIDGET(thisone.bt2), FALSE);
                gtk_widget_set_sensitive(GTK_WIDGET(thisone.en1), FALSE);
                gtk_widget_set_sensitive(GTK_WIDGET(thisone.bt1), FALSE);
            }
        }

        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->lt_button), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->lminfo.cb1), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->lminfo.bt1), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->lminfo.bt2), FALSE);
    }
}

