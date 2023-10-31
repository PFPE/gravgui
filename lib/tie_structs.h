#ifndef TIE_STRUCTS_H
#define TIE_STRUCTS_H

#include <gtk/gtk.h>
#include <vector>
#include <string>
#include <map>

////////////////////////////////////////////////////////////////////////
// structs
////////////////////////////////////////////////////////////////////////

struct val_time { // struct for holding number(s) with timestamp
    time_t t1 = -999;
    double h1 = -999;
    double m1 = -999;  // mgal conversion, land ties only
    GtkWidget *en1;  // entry widget for getting value
    GtkWidget *bt1;  // save button
    GtkWidget *bt2;  // reset button (only here for counts, active/inactive)
};

struct ship_info { // struct for ship name, buttons, dgs data, etc
    std::string ship="";
    std::string alt_ship="";
    std::vector<float> gravgrav;
    std::vector<time_t> gravtime;
    GtkWidget *dgs_label;  // show #entries in vecs
    GtkWidget *en1;  // entry for "other" ship
    GtkWidget *bt1;  // save button
    GtkWidget *bt2;  // reset button
    GtkWidget *cb1;  // combo box ie dropdown
};

struct sta_info { // struct for station name, db, k/v info, buttons, etc
    std::string station="";
    std::string alt_station="";
    float station_gravity = -999;
    std::map<std::string, std::map<std::string, std::string> > station_db;  // save for key/values
    std::map<std::string, std::string> this_station; // k/v pairs for selected station
    GtkWidget *en1;  // entry for "other" station
    GtkWidget *bt1;  // save button
    GtkWidget *bt2;  // reset button
    GtkWidget *cb1;  // combo box ie dropdown
    GtkWidget *en2;  // entry for absolute gravity as needed
    GtkWidget *bt3;  // save button for abs grav
    GtkWidget *bt4;  // reset button for abs grav
};

struct pers_info { // struct for personnel name, entry, buttons
    std::string personnel="";
    GtkWidget *en1;  // entry for "other" station
    GtkWidget *bt1;  // save button
    GtkWidget *bt2;  // reset button
};

struct calibration { // calibration for a land meter
    std::vector<float> brackets;
    std::vector<float> mgvals;
    std::vector<float> factors;
};

struct lm_info { // all things land-tie
    std::string meter="";
    std::string alt_meter="";
    std::string cal_file_path="";
    float ship_lon = -999;
    float ship_lat = -999;
    float ship_elev = -999;
    float meter_temp = -999;
    bool landtie = FALSE;
    double land_tie_value = -999; // used instead of station_gravity if landtie
    std::map<std::string, std::map<std::string, std::string> > landmeter_db; // names+paths
    calibration calib; // three vectors in a struct
    GtkWidget *en1;  // entry for "other" meter
    GtkWidget *bt1;  // save button
    GtkWidget *bt2;  // reset button
    GtkWidget *bt_cal_file;  // file dialog button for other calibration file
    GtkWidget *cb1;  // combo box ie dropdown
    GtkWidget *lts;  // toggle switch
    GtkWidget *cal_label; // label for cal table read
    GtkWidget *lt_label; // label for calculated land tie val
    GtkWidget *en_lon;
    GtkWidget *en_lat;
    GtkWidget *en_elev;
    GtkWidget *en_temp;
    GtkWidget *bt_coords;
    GtkWidget *br_coords;
};

struct tie {  // struct for holding an entire tie!
    ship_info shinfo;
    sta_info stinfo;
    pers_info prinfo;
    lm_info lminfo;
    GtkWidget *landtoggle; 
    GtkWidget *lt_button;
    //bool metric;  // might not use this? can we make everyone use meters?
    std::vector<val_time> heights{3}; // pier water heights, timestamped
    std::vector<val_time> acounts{3}; // ship 1, land tie
    std::vector<val_time> bcounts{3}; // land 1, land tie
    std::vector<val_time> ccounts{3}; // ship 2, land tie
    std::vector<double> mgal_averages{-999,-999,-999};
    std::vector<time_t> t_averages{-999,-999,-999};
    double bias=-999;  // the thing we want to calculate in the end
    GtkWidget *bias_label;
    double avg_height=-999;
    double water_grav=-999; // byproduct of bias calc that we might want to write somewhere?
    double avg_dgs_grav=-99999; // filtered sliced average grav over h1/h2/h3 time window
    double drift=-999999; // byproduct of land tie calc
};

#endif
