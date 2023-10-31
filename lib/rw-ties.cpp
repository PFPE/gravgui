#include <string>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <map>
#include "tie_structs.h"
#include "grav-constants.h"
#include "time-functions.h"

// write a gravtie struct to a TOML-compliant output file at given path
void tie_to_toml(const std::string& filepath, tie* gravtie) {
    // Open the file for writing
    std::ofstream outputFile(filepath);

    if (!outputFile.is_open()) {
        std::cerr << "Failed to open the file: " << filepath << std::endl;
        return;
    }
    // go through all the (known) components of tie, write them out
    outputFile << "[SHIP]" << std::endl;
    outputFile << "ship_name=\"" << gravtie->shinfo.ship << "\""<< std::endl;
    outputFile << "alt_ship_name=\"" << gravtie->shinfo.alt_ship << "\""<< std::endl;
    outputFile << std::endl;

    
    outputFile << "[STATION]" << std::endl;
    outputFile << "station_name=\"" << gravtie->stinfo.station << "\""<< std::endl;
    outputFile << "alt_station_name=\"" << gravtie->stinfo.alt_station << "\""<< std::endl;
    outputFile << "station_gravity=" << std::fixed << std::setprecision(2) << gravtie->stinfo.station_gravity << std::endl;
    outputFile << std::endl;

    outputFile << "[LANDMETER]" << std::endl;
    outputFile << "landtie=" << std::boolalpha << gravtie->lminfo.landtie << std::endl;
    outputFile << "meter=\"" << gravtie->lminfo.meter << "\""<< std::endl;
    outputFile << "alt_meter=\"" << gravtie->lminfo.alt_meter << "\""<< std::endl;
    outputFile << "cal_file_path=\"" << gravtie->lminfo.cal_file_path << "\""<< std::endl;
    outputFile << "ship_lon=" << std::fixed << std::setprecision(3) << gravtie->lminfo.ship_lon << std::endl;
    outputFile << "ship_lat=" << std::fixed << std::setprecision(3) << gravtie->lminfo.ship_lat << std::endl;
    outputFile << "ship_elev=" << std::fixed << std::setprecision(3) << gravtie->lminfo.ship_elev << std::endl;
    outputFile << "meter_temp=" << std::fixed << std::setprecision(3) << gravtie->lminfo.meter_temp << std::endl;
    outputFile << "land_tie_value=" << std::fixed << std::setprecision(2) << gravtie->lminfo.land_tie_value << std::endl;
    outputFile << "drift=" << std::fixed << std::setprecision(2) << gravtie->drift<< std::endl;
    // all the counts and milligals etc
    std::vector<std::vector<val_time> > allcounts{gravtie->acounts, gravtie->bcounts, gravtie->ccounts};
    char letter = 'a';
    int itm = 0;
    for (const std::vector<val_time>& thesecounts : allcounts) {
        int num = 1;
        for (const val_time& thisone : thesecounts) {
            outputFile << letter << num << ".c=" << std::fixed << std::setprecision(2) << thisone.h1 << std::endl;
            struct tm* timeinfo;
            timeinfo = gmtime(&thisone.t1);
            char buffer[20];
            strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", timeinfo);
            outputFile << letter << num << ".t=" << buffer << "Z" << std::endl;
            outputFile << letter << num << ".m=" << std::fixed << std::setprecision(2) << thisone.m1 << std::endl;
            num++;
        }
        struct tm* timeinfo;
        timeinfo = gmtime(&gravtie->t_averages[itm]);
        char buffer[20];
        strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", timeinfo);
        outputFile << letter << letter <<  ".t_avg=" << buffer << "Z" << std::endl;
        outputFile << letter << letter <<  ".m_avg=" << std::fixed << std::setprecision(3) << gravtie->mgal_averages[itm] << std::endl;
        itm++;

        letter++;
    }
    outputFile << std::endl;

    outputFile << "[TIE]" << std::endl;
    outputFile << "personnel=\"" << gravtie->prinfo.personnel << "\"" << std::endl;
    outputFile << "bias=" << std::fixed << std::setprecision(2) << gravtie->bias<< std::endl;
    outputFile << "water_grav=" << std::fixed << std::setprecision(2) << gravtie->water_grav<< std::endl;
    outputFile << "avg_dgs_grav=" << std::fixed << std::setprecision(2) << gravtie->avg_dgs_grav<< std::endl;
    outputFile << "avg_height=" << std::fixed << std::setprecision(2) << gravtie->avg_height << std::endl;

    int num = 1;
    for (const val_time& thisone : gravtie->heights) {
        outputFile << "h" << num << ".h=" << std::fixed << std::setprecision(2) << thisone.h1 << std::endl;
        struct tm* timeinfo;
        timeinfo = gmtime(&thisone.t1);
        char buffer[20];
        strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", timeinfo);
        outputFile << "h" << num << ".t=" << buffer << "Z" << std::endl;
    num++;
    }
    
    outputFile << std::endl;

    outputFile.close();
}

// read a TOML file and put values into a gravtie struct
// note that we will ignore the TOML sections like [SHIP] and go by key=value only
// the section headers are there just to make the files easier for humans to read but
// all the keys should be unique so we can get away with not caring about them here
void toml_to_tie(const std::string& filePath, tie* gravtie) {
    std::ifstream inputFile(filePath);

    if (!inputFile.is_open()) {
        std::cerr << "Error: Unable to open file " << filePath << std::endl;
        return;
    }

    std::vector<double> mgal_a{-999,-999,-999};
    std::vector<time_t> tmgal_a{-999,-999,-999};

    std::string line;
    while (std::getline(inputFile, line)) {
        if (line.empty()) {
            continue; // Skip empty lines
        }

        if (line[0] == '[' && line[line.length() - 1] == ']') {
            // Found a section header, which we don't care about
            //std::string sectionHeader = line.substr(1, line.length() - 2);
        } else {
            // Parse key-value pairs and find where they belong
            size_t delimiterPos = line.find('=');
            if (delimiterPos != std::string::npos) { // ie there is a key *and* a value
                std::string key = line.substr(0, delimiterPos);
                std::string value = line.substr(delimiterPos + 1);
                if (value.front() == '"') {  // strip quotes from strings (doesn't matter if empty)
                    value = value.substr(1,value.size());
                }
                if (value.back() == '"') {
                    value = value.substr(0,value.size() - 1);
                }
                // key matchups: strings first bc they are easy
                if (key=="ship_name") gravtie->shinfo.ship = value;
                if (key=="alt_ship_name") gravtie->shinfo.alt_ship = value;
                if (key=="station_name") gravtie->stinfo.station = value;
                if (key=="alt_station_name") gravtie->stinfo.alt_station = value;
                if (key=="meter") gravtie->lminfo.meter = value;
                if (key=="alt_meter") gravtie->lminfo.alt_meter = value;
                if (key=="cal_file_path") gravtie->lminfo.cal_file_path = value;
                if (key=="personnel") gravtie->prinfo.personnel = value;

                // key matchups for bools
                if (key=="landtie") {
                    if (value=="true") gravtie->lminfo.landtie=true;
                    if (value=="false") gravtie->lminfo.landtie=false;
                }

                // key matchups for floats: stof-ing everything
                if (key=="station_gravity") gravtie->stinfo.station_gravity = std::stof(value);
                if (key=="land_tie_value") gravtie->lminfo.land_tie_value = std::stof(value);
                if (key=="ship_lon") gravtie->lminfo.ship_lon = std::stof(value);
                if (key=="ship_lat") gravtie->lminfo.ship_lat = std::stof(value);
                if (key=="ship_elev") gravtie->lminfo.ship_elev = std::stof(value);
                if (key=="meter_temp") gravtie->lminfo.meter_temp = std::stof(value);
                if (key=="drift") gravtie->drift = std::stof(value);
                if (key=="bias") gravtie->bias = std::stof(value);
                if (key=="water_grav") gravtie->water_grav = std::stof(value);
                if (key=="avg_dgs_grav") gravtie->avg_dgs_grav = std::stof(value);
                if (key=="avg_height") gravtie->avg_height = std::stof(value);
                if (key=="aa.m_avg") mgal_a[0] = std::stof(value);
                if (key=="bb.m_avg") mgal_a[1] = std::stof(value);
                if (key=="cc.m_avg") mgal_a[2] = std::stof(value);

                if (key=="a1.c") gravtie->acounts[0].h1 = std::stof(value);
                if (key=="a2.c") gravtie->acounts[1].h1 = std::stof(value);
                if (key=="a3.c") gravtie->acounts[2].h1 = std::stof(value);
                if (key=="b1.c") gravtie->bcounts[0].h1 = std::stof(value);
                if (key=="b2.c") gravtie->bcounts[1].h1 = std::stof(value);
                if (key=="b3.c") gravtie->bcounts[2].h1 = std::stof(value);
                if (key=="c1.c") gravtie->ccounts[0].h1 = std::stof(value);
                if (key=="c2.c") gravtie->ccounts[1].h1 = std::stof(value);
                if (key=="c3.c") gravtie->ccounts[2].h1 = std::stof(value);
                if (key=="a1.m") gravtie->acounts[0].m1 = std::stof(value);
                if (key=="a2.m") gravtie->acounts[1].m1 = std::stof(value);
                if (key=="a3.m") gravtie->acounts[2].m1 = std::stof(value);
                if (key=="b1.m") gravtie->bcounts[0].m1 = std::stof(value);
                if (key=="b2.m") gravtie->bcounts[1].m1 = std::stof(value);
                if (key=="b3.m") gravtie->bcounts[2].m1 = std::stof(value);
                if (key=="c1.m") gravtie->ccounts[0].m1 = std::stof(value);
                if (key=="c2.m") gravtie->ccounts[1].m1 = std::stof(value);
                if (key=="c3.m") gravtie->ccounts[2].m1 = std::stof(value);

                if (key=="h1.h") gravtie->heights[0].h1 = std::stof(value);
                if (key=="h2.h") gravtie->heights[1].h1 = std::stof(value);
                if (key=="h3.h") gravtie->heights[2].h1 = std::stof(value);


                // key matchups for timestamps: the most annoying part
                if (key.substr(2,2)==".t"){
                    std::tm timestamp = str_to_tm(value.c_str(), 2);
                    //std::tm timestamp = {};
                    //if (strptime(value.c_str(), "%Y-%m-%dT%H:%M:%SZ", &timestamp) != nullptr) {
                    time_t outtime = my_timegm(&timestamp); //std::mktime(&timestamp);
                    if (key.substr(0,2)=="a1") gravtie->acounts[0].t1 = outtime;
                    if (key.substr(0,2)=="a2") gravtie->acounts[1].t1 = outtime;
                    if (key.substr(0,2)=="a3") gravtie->acounts[2].t1 = outtime;
                    if (key.substr(0,2)=="b1") gravtie->bcounts[0].t1 = outtime;
                    if (key.substr(0,2)=="b2") gravtie->bcounts[1].t1 = outtime;
                    if (key.substr(0,2)=="b3") gravtie->bcounts[2].t1 = outtime;
                    if (key.substr(0,2)=="c1") gravtie->ccounts[0].t1 = outtime;
                    if (key.substr(0,2)=="c2") gravtie->ccounts[1].t1 = outtime;
                    if (key.substr(0,2)=="c3") gravtie->ccounts[2].t1 = outtime;
                    if (key.substr(0,2)=="h1") gravtie->heights[0].t1 = outtime;
                    if (key.substr(0,2)=="h2") gravtie->heights[1].t1 = outtime;
                    if (key.substr(0,2)=="h3") gravtie->heights[2].t1 = outtime;

                    if (key.substr(0,2)=="aa") tmgal_a[0] = outtime;
                    if (key.substr(0,2)=="bb") tmgal_a[1] = outtime;
                    if (key.substr(0,2)=="cc") tmgal_a[2] = outtime;
                    //}
                }
            }
        }
    }
    inputFile.close();

    gravtie->mgal_averages = mgal_a;
    gravtie->t_averages = tmgal_a;

    // now, set all the gui fields and buttons appropriately based on what got read into the tie
    // ship: cb, en, save button
    if (gravtie->shinfo.ship!="") {
        if (gravtie->shinfo.ship=="Other") {
            gtk_entry_set_text(GTK_ENTRY(gravtie->shinfo.en1),gravtie->shinfo.alt_ship.c_str());
        } else {
            gtk_entry_set_text(GTK_ENTRY(gravtie->shinfo.en1),gravtie->shinfo.ship.c_str());
        }
        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->shinfo.bt1), FALSE);

        GtkTreeModel *shipmodel = gtk_combo_box_get_model(GTK_COMBO_BOX(gravtie->shinfo.cb1));
        GtkTreeIter iter;
        gboolean valid;
        valid = gtk_tree_model_get_iter_first(shipmodel, &iter);
        int countlines = 0;
        while (valid) {
            gchar *str_data;
            gtk_tree_model_get(shipmodel, &iter, 0, &str_data, -1);
            if (str_data == gravtie->shinfo.ship) {
                break;
            }
            countlines++;
            g_free(str_data);
            valid = gtk_tree_model_iter_next(shipmodel, &iter);
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(gravtie->shinfo.cb1),countlines);
        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->shinfo.cb1), FALSE);
    }
    // station: cb, en, save, absgrav, save2
    if (gravtie->stinfo.station!="") {
        if (gravtie->stinfo.station=="Other") {
            gtk_entry_set_text(GTK_ENTRY(gravtie->stinfo.en1),gravtie->stinfo.alt_station.c_str());
        } else {
            gtk_entry_set_text(GTK_ENTRY(gravtie->stinfo.en1),gravtie->stinfo.station.c_str());
        }
        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->stinfo.bt1), FALSE);
        // reset this_station so we can get number and lat/lon as needed
        if (gravtie->stinfo.station != "Other" || gravtie->stinfo.alt_station != "") {
            for (const auto& station : gravtie->stinfo.station_db) {
                for (const auto& kv : station.second) {
                    if (kv.first == "NAME" && kv.second == gravtie->stinfo.station) {
                        gravtie->stinfo.this_station = station.second;
                    }
                }
            }
        }

        GtkTreeModel *stamodel = gtk_combo_box_get_model(GTK_COMBO_BOX(gravtie->stinfo.cb1));
        GtkTreeIter iter;
        gboolean valid;
        valid = gtk_tree_model_get_iter_first(stamodel, &iter);
        int countlines = 0;
        while (valid) {
            gchar *str_data;
            gtk_tree_model_get(stamodel, &iter, 0, &str_data, -1);
            if (str_data == gravtie->stinfo.station) {
                break;
            }
            countlines++;
            g_free(str_data);
            valid = gtk_tree_model_iter_next(stamodel, &iter);
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(gravtie->stinfo.cb1),countlines);
        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->stinfo.cb1), FALSE);
    }
    if (gravtie->stinfo.station_gravity>0) {
        std::stringstream strm;
        strm << std::fixed << std::setprecision(2) << gravtie->stinfo.station_gravity;
        std::string test = strm.str();
        char buffer[test.length()]; // Adjust size?
        snprintf(buffer, sizeof(buffer), "%.2f", gravtie->stinfo.station_gravity);
        gtk_entry_set_text(GTK_ENTRY(gravtie->stinfo.en2), buffer);
        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->stinfo.en2), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->stinfo.bt3), FALSE);
    }
    // personnel: en, save
    if (gravtie->prinfo.personnel!=""){
        gtk_entry_set_text(GTK_ENTRY(gravtie->prinfo.en1), gravtie->prinfo.personnel.c_str());
        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->prinfo.en1), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->prinfo.bt1), FALSE);
    }
    // land tie toggle (note that callback for buttons does NOT work here)
    if (gravtie->lminfo.landtie) gtk_switch_set_active(GTK_SWITCH(gravtie->lminfo.lts), TRUE);
    if (gravtie->lminfo.ship_lon > 0) {
        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->lminfo.en_lon), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->lminfo.en_lat), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->lminfo.en_elev), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->lminfo.en_temp), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->lminfo.bt_coords), FALSE);

        char buff1[7];
        snprintf(buff1, sizeof(buff1), "%.3f", gravtie->lminfo.ship_lon);
        char buff2[7];
        snprintf(buff2, sizeof(buff2), "%.3f", gravtie->lminfo.ship_lat);
        char buff3[7];
        snprintf(buff3, sizeof(buff3), "%.3f", gravtie->lminfo.ship_elev);
        char buff4[7];
        snprintf(buff4, sizeof(buff4), "%.3f", gravtie->lminfo.meter_temp);
        gtk_entry_set_text(GTK_ENTRY(gravtie->lminfo.en_lon), buff1);
        gtk_entry_set_text(GTK_ENTRY(gravtie->lminfo.en_lat), buff2);
        gtk_entry_set_text(GTK_ENTRY(gravtie->lminfo.en_elev), buff3);
        gtk_entry_set_text(GTK_ENTRY(gravtie->lminfo.en_temp), buff4);
    }
    // land meter
    if (gravtie->lminfo.meter!="") {
        if (gravtie->lminfo.meter=="Other") {
            gtk_entry_set_text(GTK_ENTRY(gravtie->lminfo.en1),gravtie->lminfo.alt_meter.c_str());
        } else {
            gtk_entry_set_text(GTK_ENTRY(gravtie->lminfo.en1),gravtie->lminfo.meter.c_str());
        }
        // don't freeze the save meter button bc saving will read the cal file?
        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->lminfo.bt1), TRUE);

        GtkTreeModel *metmodel = gtk_combo_box_get_model(GTK_COMBO_BOX(gravtie->lminfo.cb1));
        GtkTreeIter iter;
        gboolean valid;
        valid = gtk_tree_model_get_iter_first(metmodel, &iter);
        int countlines = 0;
        while (valid) {
            gchar *str_data;
            gtk_tree_model_get(metmodel, &iter, 0, &str_data, -1);
            if (str_data == gravtie->lminfo.meter) {
                break;
            }
            countlines++;
            g_free(str_data);
            valid = gtk_tree_model_iter_next(metmodel, &iter);
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(gravtie->lminfo.cb1),countlines);
        gtk_widget_set_sensitive(GTK_WIDGET(gravtie->lminfo.cb1), FALSE);
    }
    // a/b/c/h: values, saves
    std::vector<std::vector<val_time> > allcounts{gravtie->acounts, gravtie->bcounts, gravtie->ccounts, gravtie->heights};
    for (std::vector<val_time>& thesecounts : allcounts) {
        for (val_time& thisone : thesecounts) {
            if (thisone.h1 > 0) {
                std::stringstream strm;
                strm << std::fixed << std::setprecision(2) << thisone.h1;
                std::string test = strm.str();
                char buffbuff[test.length()];
                snprintf(buffbuff, sizeof(buffbuff), "%.2f", thisone.h1);
                gtk_entry_set_text(GTK_ENTRY(thisone.en1),buffbuff);
                gtk_widget_set_sensitive(GTK_WIDGET(thisone.en1), FALSE);
                gtk_widget_set_sensitive(GTK_WIDGET(thisone.bt1), FALSE);
            }
        }
    }
    // computed bias if there is any
    if (gravtie->bias>0) {
        std::stringstream strm;
        strm << std::fixed << std::setprecision(2) << gravtie->bias;
        std::string test = strm.str();
        char bstring[test.length()+16];
        sprintf(bstring,"Computed bias: %.2f", gravtie->bias);
        gtk_label_set_text(GTK_LABEL(gravtie->bias_label), bstring);
    }
    // and similar for land tie value if any
    if (gravtie->lminfo.land_tie_value>0) {
        std::stringstream strm;
        strm << std::fixed << std::setprecision(2) << gravtie->lminfo.land_tie_value;
        std::string test = strm.str();
        char bstring[test.length()+17];
        sprintf(bstring,"Land tie value: %.2f", gravtie->lminfo.land_tie_value);
        gtk_label_set_text(GTK_LABEL(gravtie->lminfo.lt_label), bstring);
    }

    return;
}

void write_report(const std::string& filepath, tie* gravtie) {
    // Open the file for writing
    std::ofstream outputFile(filepath);

    if (!outputFile.is_open()) {
        std::cerr << "Failed to open the file: " << filepath << std::endl;
        return;
    }
    outputFile << std::endl;  // empty line at top of file apparently
    // go through all the (known) components of tie, write them out
    if (gravtie->shinfo.ship == "Other") {
        outputFile << "Ship: " << gravtie->shinfo.alt_ship << std::endl;
    } else {
        outputFile << "Ship: " << gravtie->shinfo.ship << std::endl;
    }
    outputFile << "Personnel: " << gravtie->prinfo.personnel << std::endl;
    outputFile << std::endl;

    outputFile << "#Base Station:" << std::endl;
    if (gravtie->stinfo.station == "Other") {
        outputFile << "Name: " << gravtie->stinfo.alt_station << std::endl;
    } else {
        outputFile << "Name: " << gravtie->stinfo.station << std::endl;
    }
    outputFile << "Number: ";
    for (const auto& kv : gravtie->stinfo.this_station) {
        if (kv.first == "NUMBER") {
            outputFile << kv.second;
        }
    }
    outputFile << std::endl;
    outputFile << "Known absolute gravity (mGal): " << std::fixed << std::setprecision(3) << gravtie->stinfo.station_gravity << std::endl;

    outputFile << std::endl;
    outputFile << "------------------- Land tie ----------------" << std::endl;
    outputFile << std::endl;
    
    if (gravtie->lminfo.landtie) {
        outputFile << "Land meter #: " << gravtie->lminfo.meter << std::endl;
        outputFile << "Meter temperature: " << std::fixed << std::setprecision(3) << gravtie->lminfo.meter_temp << std::endl;
        outputFile << std::endl;
        outputFile << "#New station A:" << std::endl;
        outputFile << "Name: " << gravtie->shinfo.ship << std::endl;
        outputFile << "Latitude (deg): " << std::fixed << std::setprecision(3) << gravtie->lminfo.ship_lat << std::endl;
        outputFile << "Longitude (deg): " << std::fixed << std::setprecision(3) << gravtie->lminfo.ship_lon << std::endl;
        outputFile << "Elevation (m): " << std::fixed << std::setprecision(3) << gravtie->lminfo.ship_elev << std::endl;

        std::string whichone;
        for (int i=0; i<3; i++) {
            time_t rawtime = gravtie->t_averages[i];
            struct tm * cookedtime;
            char buffer[30];
            if (i == 0) whichone = "A1";
            if (i == 1) whichone = "B";
            if (i == 2) whichone = "A2";
            cookedtime = gmtime(&rawtime);
            strftime(buffer, sizeof(buffer), "%Y/%m/%d %H:%M:%S", cookedtime);
            outputFile << "UTC time and meter gravity (mGal) at " << whichone << ": ";
                outputFile << buffer << " " << std::fixed << std::setprecision(3) << gravtie->mgal_averages[i] << std::endl;
        }

        double AA_timedelta = difftime(gravtie->t_averages[2], gravtie->t_averages[0]);
        double AB_timedelta = difftime(gravtie->t_averages[1], gravtie->t_averages[0]);
        double dc_avg_mgals_B = gravtie->mgal_averages[1] - AB_timedelta*gravtie->drift;

        outputFile << "Delta_T_ab (s): " << std::fixed << std::setprecision(3) << AB_timedelta << std::endl;
        outputFile << "Delta_T_aa (s): " << std::fixed << std::setprecision(3) << AA_timedelta << std::endl;
        outputFile << "Drift (mGal): (" << std::fixed << std::setprecision(3) << gravtie->mgal_averages[2] << " - " << gravtie->mgal_averages[0] << ")/" << AA_timedelta << " = " << gravtie->drift << std::endl;
        outputFile << "Drift corrected meter gravity at B (mGal): " << std::fixed << std::setprecision(3) << gravtie->mgal_averages[1] << " - " << AB_timedelta << " * " << gravtie->drift << " = " << dc_avg_mgals_B << std::endl;
        outputFile << "Gravity at pier (mGal): " << gravtie->stinfo.station_gravity << " + " << gravtie->mgal_averages[0] << " - " << dc_avg_mgals_B << " = " << gravtie->lminfo.land_tie_value << std::endl;

    } else {
        outputFile << "N/A" << std::endl;
    }
    outputFile << std::endl;
    outputFile << "------------------- End of land tie ----------------" << std::endl;
    outputFile << std::endl;

    // gravity at pier is either land tie value or station grav, depending?    
    outputFile << "Gravity at pier (mGal): ";
    if (gravtie->lminfo.landtie) {
        outputFile << std::fixed << std::setprecision(3) << gravtie->lminfo.land_tie_value << std::endl;
    } else {
        outputFile << std::fixed << std::setprecision(3) << gravtie->stinfo.station_gravity << std::endl;
    }

    // UTC stamps and heights
    for (int i=0; i<3; i++) {
        time_t rawtime = gravtie->heights[i].t1;
        struct tm * cookedtime;
        char buffer[30];
        cookedtime = gmtime(&rawtime);
        strftime(buffer, sizeof(buffer), "%Y/%m/%d %H:%M:%S", cookedtime);
        outputFile << "UTC time and water height to pier (m) " << i+1 << ": ";
        if (gravtie->heights[i].h1 > 0) {
            outputFile << buffer << " " << std::fixed << std::setprecision(3) << gravtie->heights[i].h1 << std::endl;
        } else {
            outputFile << std::endl;
        }
    }
    outputFile << std::endl;

    // dgs average
    outputFile << "DgS meter gravity (mGal): " << std::fixed << std::setprecision(4) << gravtie->avg_dgs_grav << std::endl;
    // average water height
    outputFile << "Average water height to pier (m): " << std::fixed << std::setprecision(4) << gravtie->avg_height << std::endl;
    // water grav
    outputFile << "Gravity at water line (mGal): ";
    if (gravtie->lminfo.landtie) {
        outputFile << std::fixed << std::setprecision(3) << gravtie->lminfo.land_tie_value;
    } else {
        outputFile << std::fixed << std::setprecision(3) << gravtie->stinfo.station_gravity;
    }
    outputFile << " + " << std::fixed << std::setprecision(4) << faafactor;
    outputFile << " * " << std::fixed << std::setprecision(4) << gravtie->avg_height;
    outputFile << " = " << std::fixed << std::setprecision(3) << gravtie->water_grav << std::endl;
    // bias
    outputFile << "DgS meter bias (mGal): " << std::fixed << std::setprecision(3) << gravtie->water_grav;
    outputFile << " - " << std::fixed << std::setprecision(4) << gravtie->avg_dgs_grav;
    outputFile << " = " << std::fixed << std::setprecision(4) << gravtie->bias << std::endl;
    
    outputFile.close();
}

