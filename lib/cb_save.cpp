#include <gtk/gtk.h>
#include <string>
#include <ctime>
#include <iostream>
#include "rw-general.h"
#include "tie_structs.h"

// callback function for clicking a button and saving a value+timestamp
void on_timestamp_button(GtkWidget *widget, gpointer data) {
    // Cast data back to a pointer to MyData
    val_time* hval = static_cast<val_time*>(data);

    // Get the FLOAT entered in the entry field
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(hval->en1));
    if (text != NULL && text[0] != '\0') {
        float num = atof(text);
        char buffer[5];
        snprintf(buffer, sizeof(buffer), "%.2f", num);
        gtk_entry_set_text(GTK_ENTRY(hval->en1), buffer);  // in case of invalid entry -> 0

        // Get the current timestamp
        time_t measuredtime;
        std::time ( &measuredtime ); // seconds since epoch, in UTC automatically
        //struct tm * thistime;
        //thistime = std::gmtime(&measuredtime);
        //std::cout << std::asctime(thistime) << std::endl;

        // Update the struct with the text and timestamp
        hval->h1 = std::abs(num); // always save as positive number bc -999 is flag for no value
        hval->t1 = measuredtime; //timestamp;  // using time_t instead of timestamp here

        // lock the field and the save button so things don't change
        gtk_widget_set_sensitive(GTK_WIDGET(hval->en1), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(hval->bt1), FALSE);
    }
}

// callback function for ship save button
void on_ship_save(GtkWidget *widget, gpointer data) {
    // Cast data back to a pointer
    ship_info* shinfo = static_cast<ship_info*>(data);

    if (shinfo->ship != "") { // make sure something has been selected
        if (shinfo->ship != "Other") {
            // if it is not other, lock save button and combo box and fill (locked) entry field
            gtk_entry_set_text(GTK_ENTRY(shinfo->en1),shinfo->ship.c_str());
            gtk_widget_set_sensitive(GTK_WIDGET(shinfo->bt1), FALSE);
            gtk_widget_set_sensitive(GTK_WIDGET(shinfo->cb1), FALSE);
        } else {
        // find out if the ship value is "Other"
            // if it is, read the entry field 
            const gchar *text = gtk_entry_get_text(GTK_ENTRY(shinfo->en1));
            // if there is text in the entry, save it, lock entry and save button
            std::string othertext = text;  // cast to string
            if (othertext != "") {  // there is something in the box!
                shinfo->alt_ship = othertext;
                gtk_widget_set_sensitive(GTK_WIDGET(shinfo->en1), FALSE);
                gtk_widget_set_sensitive(GTK_WIDGET(shinfo->bt1), FALSE);
            } // if nothing in the box, save does nothing
        }
    }
}

// callback function for ship save button
void on_sta_save(GtkWidget *widget, gpointer data) {
    // Cast data back to a pointer
    sta_info* stinfo = static_cast<sta_info*>(data);

    if (stinfo->station != "") {
        if (stinfo->station != "Other") {
            // if it is not other, lock save button
            gtk_entry_set_text(GTK_ENTRY(stinfo->en1),stinfo->station.c_str());
            gtk_widget_set_sensitive(GTK_WIDGET(stinfo->bt1), FALSE);
            gtk_widget_set_sensitive(GTK_WIDGET(stinfo->cb1), FALSE);
        } else {
            // if it is other, read the entry field 
            const gchar *text = gtk_entry_get_text(GTK_ENTRY(stinfo->en1));
            // if there is text in the entry, save it, lock entry and save button
            std::string othertext = text;  // cast to string
            if (othertext != "") {  // there is something in the box!
                stinfo->alt_station = othertext;
                gtk_widget_set_sensitive(GTK_WIDGET(stinfo->en1), FALSE);
                gtk_widget_set_sensitive(GTK_WIDGET(stinfo->bt1), FALSE);
            }  // if there's nothing in the box this does nothing
        }
        if (stinfo->station != "Other" || stinfo->alt_station != "") {
            // loop key/value pairs in station db map and find this one
            for (const auto& station : stinfo->station_db) {
                for (const auto& kv : station.second) {
                    if (kv.first == "NAME" && kv.second == stinfo->station) {
                        stinfo->this_station = station.second;
                        //std::cout << stinfo->this_station.at("GRAVITY") << std::endl;
                        if (station.second.at("GRAVITY") == "") {
                            gtk_widget_set_sensitive(GTK_WIDGET(stinfo->en2), TRUE);
                            gtk_widget_set_sensitive(GTK_WIDGET(stinfo->bt3), TRUE);
                            gtk_widget_set_sensitive(GTK_WIDGET(stinfo->bt4), TRUE);
                        } else {
                            stinfo->station_gravity = std::stof(station.second.at("GRAVITY"));
                            char buffer[20]; // Adjust size?
                            snprintf(buffer, sizeof(buffer), "%.2f", stinfo->station_gravity);
                            gtk_entry_set_text(GTK_ENTRY(stinfo->en2), buffer);
                        }
                    }
                }
            }
        }
    }
}

// callback function for absolute grav save button
void on_grav_save(GtkWidget *widget, gpointer data) {
    // Cast data back to a pointer
    sta_info* stinfo = static_cast<sta_info*>(data);

    // get text entry
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(stinfo->en2));
    try {
        float floatValue = std::stof(text);
        stinfo->station_gravity = floatValue;
        gtk_widget_set_sensitive(GTK_WIDGET(stinfo->en2), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(stinfo->bt3), FALSE);
    } catch (const std::invalid_argument& e) {
        gtk_entry_set_text(GTK_ENTRY(stinfo->en2), "");
    }
}

// callback function for personnel save button
void on_pers_save(GtkWidget *widget, gpointer data) {
    // Cast data back to a pointer
    pers_info* prinfo = static_cast<pers_info*>(data);

    const gchar *text = gtk_entry_get_text(GTK_ENTRY(prinfo->en1));
    std::string othertext = text;  // cast to string
    if (othertext != "") {  // there is something in the box!
        prinfo->personnel = othertext;
        gtk_widget_set_sensitive(GTK_WIDGET(prinfo->en1), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(prinfo->bt1), FALSE);
    }
}

// callback function for meter save button
void on_lm_save(GtkWidget *widget, gpointer data) {
    // Cast data back to a pointer
    lm_info* lminfo = static_cast<lm_info*>(data);

    if (lminfo->meter != "") {
        if (lminfo->meter != "Other") {
            // if it is not other, lock save button
            gtk_entry_set_text(GTK_ENTRY(lminfo->en1),lminfo->meter.c_str());
            gtk_widget_set_sensitive(GTK_WIDGET(lminfo->bt1), FALSE);
            gtk_widget_set_sensitive(GTK_WIDGET(lminfo->cb1), FALSE);
        } else {
            // if it is other, read the entry field 
            const gchar *text = gtk_entry_get_text(GTK_ENTRY(lminfo->en1));
            // if there is text in the entry, save it, lock entry and save button
            std::string othertext = text;  // cast to string
            if (othertext != "") {  // there is something in the box!
                lminfo->alt_meter = othertext;
                gtk_widget_set_sensitive(GTK_WIDGET(lminfo->en1), FALSE);
                gtk_widget_set_sensitive(GTK_WIDGET(lminfo->bt1), FALSE);
                gtk_entry_set_text(GTK_ENTRY(lminfo->en1), lminfo->alt_meter.c_str());
            }  // if there's nothing in the box this does nothing
        }
        if (lminfo->meter != "Other" || lminfo->alt_meter != "") {
            // loop key/value pairs in station db map and find this one
            for (const auto& meter : lminfo->landmeter_db) {
                for (const auto& kv : meter.second) {
                    if (kv.first == "SN" && kv.second == lminfo->meter) {
                        //std::cout << stinfo->this_station.at("GRAVITY") << std::endl;
                        if (meter.second.at("TABLE") == "") {
                            gtk_widget_set_sensitive(GTK_WIDGET(lminfo->bt_cal_file), TRUE);
                        } else {
                            lminfo->cal_file_path = meter.second.at("TABLE");
                            lminfo->calib = read_lm_calib("database/land-cal/"+meter.second.at("TABLE"));
                            //std::cout << lminfo->calib.mgvals[0] << std::endl;
                            char* buffer = new char[lminfo->cal_file_path.length() + 1];
                            strcpy(buffer, lminfo->cal_file_path.c_str());
                        }
                    }
                }
            }
        }
        char buffer[40]; // Adjust size?
        snprintf(buffer, sizeof(buffer), "%i calibration lines read", (int) lminfo->calib.brackets.size());
        gtk_label_set_text(GTK_LABEL(lminfo->cal_label), buffer); 
    }
}

// callback function for saving ship coordinates for a land tie
void on_lm_coordsave_button(GtkWidget *widget, gpointer data) {
    lm_info* lminfo = static_cast<lm_info*>(data);
    const gchar *lonlon = gtk_entry_get_text(GTK_ENTRY(lminfo->en_lon));
    const gchar *latlat = gtk_entry_get_text(GTK_ENTRY(lminfo->en_lat));
    const gchar *eleva = gtk_entry_get_text(GTK_ENTRY(lminfo->en_elev));
    const gchar *temper = gtk_entry_get_text(GTK_ENTRY(lminfo->en_temp));
    if (lonlon != NULL && lonlon[0] != '\0' && latlat != NULL && latlat[0] != '\0' && eleva != NULL && temper != NULL) {
        float nlon = atof(lonlon);
        float nlat = atof(latlat);
        float nele = atof(eleva);
        float ntem = atof(temper);
        lminfo->ship_lon = nlon;
        lminfo->ship_lat = nlat;
        lminfo->ship_elev = nele;
        lminfo->meter_temp = ntem;

        char buff1[7];
        snprintf(buff1, sizeof(buff1), "%.3f", nlon);
        char buff2[7];
        snprintf(buff2, sizeof(buff2), "%.3f", nlat);
        char buff3[7];
        snprintf(buff3, sizeof(buff3), "%.3f", nele);
        char buff4[7];
        snprintf(buff4, sizeof(buff4), "%.3f", ntem);
        gtk_entry_set_text(GTK_ENTRY(lminfo->en_lon), buff1);
        gtk_entry_set_text(GTK_ENTRY(lminfo->en_lat), buff2);
        gtk_entry_set_text(GTK_ENTRY(lminfo->en_elev), buff3);
        gtk_entry_set_text(GTK_ENTRY(lminfo->en_temp), buff4);

        gtk_widget_set_sensitive(GTK_WIDGET(lminfo->en_lon), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(lminfo->en_lat), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(lminfo->en_elev), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(lminfo->en_temp), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(lminfo->bt_coords), FALSE);
    }
}


