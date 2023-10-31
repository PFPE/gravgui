#include <gtk/gtk.h>
#include <iostream>
#include <fstream>
#include <ctime>
#include "lib/tie_structs.h"        // nested structs for ties etc
#include "lib/rw-general.h"         // i/o database files and DGS files
#include "lib/rw-ties.h"            // i/o toml and /o reports
#include "lib/grav-constants.h"     // faa factor etc
#include "lib/cb_filebrowse.h"      // filebrowse callback functions
#include "lib/cb_change.h"          // menu change callback functions
#include "lib/cb_save.h"            // save callback functions
#include "lib/cb_reset.h"           // reset/clear callback functions
#include "lib/actual_computations.h" // computing land ties and biases

// Libraries used outside of main //////////////////////////////////////
//#include <iomanip>
//#include <sstream>
//#include <vector>
//#include <string>
//#include <map>
//#include <numeric>
//#include <algorithm>
//#include "lib/filt.h"               // constructs for applying FIR filter(s)
//#include "lib/window_functions.h"   // calculate windows for filters
//#include "lib/time-functions.h"     // converting between time_t and tm
////////////////////////////////////////////////////////////////////////
//#include <cairo.h>  // will need later for plotting if we try to do that
////////////////////////////////////////////////////////////////////////

// This program is for a gravity tie GUI using GTK
// Database files (database/*) must be in the dir where the program is run
// By Hannah F. Mark, October 2023
// with help from GPT-3.5 via OpenAI
// for PFPE

////////////////////////////////////////////////////////////////////////
// main!
////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
    // Initialize GTK
    gtk_init(&argc, &argv);

    // initialize the tie
    tie gravtie;

    // load custom style info
    GtkCssProvider* cssProvider = gtk_css_provider_new();
    GError* error = nullptr;
    if (!gtk_css_provider_load_from_path(cssProvider, "style.css", &error)) {
        g_printerr("Error loading CSS file: %s\n", error->message);
        g_error_free(error);
    }

    // Create the main window
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "gravgui v1.0");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);

    // Create a grid and put it in the window
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), gboolean TRUE);
    gtk_grid_set_row_homogeneous(GTK_GRID(grid), gboolean TRUE);
    gtk_container_add(GTK_CONTAINER(window), grid);

    // SHIP SELECT /////////////////////////////////////////////////////////
    GtkWidget *ship_label = gtk_label_new("Ship: ");      // label for ship dropdown
    GtkWidget *ship_menu;       // ship dropdown
    GtkListStore *ship_list;    // list of ships
    GtkCellRenderer *ship_render;
    GtkTreeIter ship_iter;

    gtk_label_set_line_wrap(GTK_LABEL(ship_label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(ship_label), 1.0);  // right-justify the text
    gtk_grid_attach(GTK_GRID(grid), ship_label, 0, 0, 1, 1);

    // Create a combo box for the list of ships
    ship_menu = gtk_combo_box_new();
    ship_list = GTK_LIST_STORE(gtk_list_store_new(1, G_TYPE_STRING));
    gtk_combo_box_set_model(GTK_COMBO_BOX(ship_menu), GTK_TREE_MODEL(ship_list));

    // read list of ships from database file and put into a dropdown
    // TODO this could be a function like the stations.db read?
    std::ifstream inputFile("database/ships.db"); // Replace with the path to your file
    if (!inputFile) { // make sure the database file is inplace
        std::cerr << "Error: Could not open the file." << std::endl;
        return 1;
    }
    std::string line;  // read line by line
    while (std::getline(inputFile, line)) {
        // Find the position of the equal sign
        size_t equalPos = line.find('=');

        if (equalPos != std::string::npos) {
            // Extract the ship name on the other side of the equals sign (must be in double quotes)
            size_t quote1Pos = line.find('"', equalPos);
            size_t quote2Pos = line.find('"', quote1Pos + 1);

            if (quote1Pos != std::string::npos && quote2Pos != std::string::npos) {
                std::string value = line.substr(quote1Pos + 1, quote2Pos - quote1Pos - 1);
                //std::cout << "Extracted value: " << value << std::endl;
                // add this ship to the list store for the combo box
                gtk_list_store_append(ship_list, &ship_iter);
                gtk_list_store_set(ship_list, &ship_iter, 0, value.c_str(), -1);
            }
        }
    }
    inputFile.close();  // cleanup

    // add the combobox to the tie stuff so we can turn it on and off as we like
    gravtie.shinfo.cb1 = ship_menu;

    // Create a cell renderer and associate it with the combo box
    ship_render = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(ship_menu), ship_render, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(ship_menu), ship_render, "text", 0, NULL);

    // Connect the ship menu choice event to the callback function, add to grid
    g_signal_connect(G_OBJECT(ship_menu), "changed", G_CALLBACK(on_ship_changed), &gravtie.shinfo);
    gtk_grid_attach(GTK_GRID(grid), ship_menu, 1,0,3,1);

    // add style context to the ship menu?
    GtkStyleContext* context = gtk_widget_get_style_context(GTK_WIDGET(ship_menu));
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(cssProvider), GTK_STYLE_PROVIDER_PRIORITY_USER);

    // box & buttons for entering name if ship is "other"
    GtkWidget *e_othership = gtk_entry_new();  // other ship
    gtk_grid_attach(GTK_GRID(grid), e_othership, 4, 0, 2, 1);
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_othership),"Ship name");
    GtkWidget *b_saveship = gtk_button_new_with_label("Save");
    g_signal_connect(b_saveship, "clicked", G_CALLBACK(on_ship_save), &gravtie.shinfo);
    gtk_grid_attach(GTK_GRID(grid), b_saveship, 6, 0, 1, 1);
    GtkWidget *b_resetship = gtk_button_new_with_label("Reset");
    g_signal_connect(b_resetship, "clicked", G_CALLBACK(on_ship_reset), &gravtie.shinfo);
    gtk_grid_attach(GTK_GRID(grid), b_resetship, 7, 0, 1, 1);

    // add relevant things to gravtie.shinfo for callbacks
    gravtie.shinfo.en1 = e_othership;
    gravtie.shinfo.bt1 = b_saveship;
    gravtie.shinfo.bt2 = b_resetship;
    // set entry to insensitive to start with
    gtk_widget_set_sensitive(GTK_WIDGET(e_othership), FALSE);

    // STATION SELECT /////////////////////////////////////////////////////////
    GtkWidget *sta_label = gtk_label_new("Station: ");  // label for stations dropdown
    GtkWidget *sta_menu;  // stations dropdown
    GtkListStore *sta_list;  // list of stations
    GtkCellRenderer *sta_render;
    GtkTreeIter sta_iter;

    gtk_label_set_line_wrap(GTK_LABEL(sta_label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(sta_label), 1.0);
    gtk_grid_attach(GTK_GRID(grid), sta_label, 0, 1, 1, 1);

    // read in all the stations, make a dropdown for them as well
    std::string filePath = "database/stations.db"; // fixed path to stations db file
    std::map<std::string, std::map<std::string, std::string> > stationData = readStationFile(filePath);
    gravtie.stinfo.station_db = stationData;

    // Create a combo box for the list of stations
    sta_menu = gtk_combo_box_new();
    sta_list = GTK_LIST_STORE(gtk_list_store_new(1, G_TYPE_STRING));
    gtk_combo_box_set_model(GTK_COMBO_BOX(sta_menu), GTK_TREE_MODEL(sta_list));

    // fill in the list with the station "NAME" keys
    for (const auto& station : stationData) {
        for (const auto& kv : station.second) {  // TODO key NAME directly?
            if (kv.first == "NAME") {
                gtk_list_store_append(sta_list, &sta_iter);
                gtk_list_store_set(sta_list, &sta_iter, 0, kv.second.c_str(), -1);
                //std::cout << kv.first << ": " << kv.second << std::endl;
            }
        }
    }

    // Create a cell renderer and associate it with the combo box
    sta_render = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(sta_menu), sta_render, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(sta_menu), sta_render, "text", 0, NULL);

    // Connect the station menu choice event to the callback function
    g_signal_connect(G_OBJECT(sta_menu), "changed", G_CALLBACK(on_sta_changed), &gravtie.stinfo);
    gtk_grid_attach(GTK_GRID(grid), sta_menu, 1, 1, 3, 1);

    // box & button for when station is "other"
    GtkWidget *e_othersta = gtk_entry_new();  // other station
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_othersta),"Station name");
    gtk_grid_attach(GTK_GRID(grid), e_othersta, 4, 1, 2, 1);
    GtkWidget *b_savesta = gtk_button_new_with_label("Save");
    g_signal_connect(b_savesta, "clicked", G_CALLBACK(on_sta_save), &gravtie.stinfo);
    gtk_grid_attach(GTK_GRID(grid), b_savesta, 6, 1, 1, 1);
    GtkWidget *b_resetsta = gtk_button_new_with_label("Reset");
    g_signal_connect(b_resetsta, "clicked", G_CALLBACK(on_sta_reset), &gravtie.stinfo);
    gtk_grid_attach(GTK_GRID(grid), b_resetsta, 7, 1, 1, 1);

    // box and button for absolute gravity when we need it
    GtkWidget *e_absgrav = gtk_entry_new();  // other station
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_absgrav),"Absolute gravity");
    gtk_widget_set_sensitive(GTK_WIDGET(e_absgrav), FALSE);
    gtk_grid_attach(GTK_GRID(grid), e_absgrav, 8, 1, 2, 1);
    GtkWidget *b_savegrav = gtk_button_new_with_label("Save");
    gtk_widget_set_sensitive(GTK_WIDGET(b_savegrav), FALSE);
    g_signal_connect(b_savegrav, "clicked", G_CALLBACK(on_grav_save), &gravtie.stinfo);
    gtk_grid_attach(GTK_GRID(grid), b_savegrav, 10, 1, 1, 1);
    GtkWidget *b_resetgrav = gtk_button_new_with_label("Reset");
    g_signal_connect(b_resetgrav, "clicked", G_CALLBACK(on_grav_reset), &gravtie.stinfo);
    gtk_grid_attach(GTK_GRID(grid), b_resetgrav, 11, 1, 1, 1);
    gtk_widget_set_sensitive(GTK_WIDGET(b_resetgrav), FALSE);

    // add things to the tie for callbacks
    gravtie.stinfo.en1 = e_othersta;
    gravtie.stinfo.bt1 = b_savesta;
    gravtie.stinfo.bt2 = b_resetsta;
    gravtie.stinfo.cb1 = sta_menu;
    gravtie.stinfo.en2 = e_absgrav;
    gravtie.stinfo.bt3 = b_savegrav;
    gravtie.stinfo.bt4 = b_resetgrav;
    gtk_widget_set_sensitive(GTK_WIDGET(e_othersta), FALSE);

    // LAND METER SELECT ///////////////////////////////////////////////////
    GtkWidget *lm_label = gtk_label_new("Land meter: ");
    GtkWidget *lm_menu;  // meters dropdown
    GtkListStore *lm_list;  // list of meters
    GtkCellRenderer *lm_render;
    GtkTreeIter lm_iter;

    gtk_label_set_line_wrap(GTK_LABEL(lm_label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(lm_label), 1.0);
    gtk_grid_attach(GTK_GRID(grid), lm_label, 0, 4, 1, 1);

    // read in all the meters, make a dropdown for them as well
    std::string filePath1 = "database/landmeters.db"; // fixed path to meters db file
    std::map<std::string, std::map<std::string, std::string> > meterData = readMeterFile(filePath1);
    gravtie.lminfo.landmeter_db = meterData;

    // Create a combo box for the list of stations
    lm_menu = gtk_combo_box_new();
    lm_list = GTK_LIST_STORE(gtk_list_store_new(1, G_TYPE_STRING));
    gtk_combo_box_set_model(GTK_COMBO_BOX(lm_menu), GTK_TREE_MODEL(lm_list));

    // fill in the list with the meter "SN" keys
    for (const auto& meter : meterData) {
        for (const auto& kv : meter.second) {  // TODO key SN directly?
            if (kv.first == "SN") {
                gtk_list_store_append(lm_list, &lm_iter);
                gtk_list_store_set(lm_list, &lm_iter, 0, kv.second.c_str(), -1);
            }
        }
    }

    // Create a cell renderer and associate it with the combo box
    lm_render = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(lm_menu), lm_render, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(lm_menu), lm_render, "text", 0, NULL);

    // Connect the station menu choice event to the callback function
    g_signal_connect(G_OBJECT(lm_menu), "changed", G_CALLBACK(on_lm_changed), &gravtie.lminfo);
    gtk_grid_attach(GTK_GRID(grid), lm_menu, 1, 4, 3, 1);

    // box & button for when meter is "other"
    GtkWidget *e_otherlm = gtk_entry_new();  // other station
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_otherlm),"Meter name");
    gtk_grid_attach(GTK_GRID(grid), e_otherlm, 4, 4, 2, 1);
    GtkWidget *b_savelm = gtk_button_new_with_label("Save");
    g_signal_connect(b_savelm, "clicked", G_CALLBACK(on_lm_save), &gravtie.lminfo);
    gtk_grid_attach(GTK_GRID(grid), b_savelm, 6, 4, 1, 1);
    GtkWidget *b_resetlm = gtk_button_new_with_label("Reset");
    g_signal_connect(b_resetlm, "clicked", G_CALLBACK(on_lm_reset), &gravtie.lminfo);
    gtk_grid_attach(GTK_GRID(grid), b_resetlm, 7, 4, 1, 1);
    GtkWidget *b_lmcalfile = gtk_button_new_with_label("Other cal. file");
    gtk_grid_attach(GTK_GRID(grid), b_lmcalfile, 8, 4, 2, 1);
    g_signal_connect(b_lmcalfile, "clicked", G_CALLBACK(on_lm_filebrowse_clicked), &gravtie.lminfo);
    GtkWidget *cal_label = gtk_label_new("0 calibration lines read");
    gtk_label_set_line_wrap(GTK_LABEL(cal_label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(cal_label), 0.0);
    gtk_grid_attach(GTK_GRID(grid), cal_label, 10, 4, 2, 1);
    gravtie.lminfo.cal_label = cal_label;

    gravtie.lminfo.en1 = e_otherlm;
    gravtie.lminfo.bt1 = b_savelm;
    gravtie.lminfo.bt2 = b_resetlm;
    gravtie.lminfo.bt_cal_file = b_lmcalfile;
    gravtie.lminfo.cb1 = lm_menu;
    gtk_widget_set_sensitive(GTK_WIDGET(lm_menu), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(e_otherlm), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(b_savelm), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(b_resetlm), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(b_lmcalfile), FALSE);

    // PERSONNEL ENTRY /////////////////////////////////////////////////////////
    GtkWidget *e_pers = gtk_entry_new();  // personnel
    gtk_grid_attach(GTK_GRID(grid), e_pers, 1, 2, 3, 1);
    GtkWidget *b_persave = gtk_button_new_with_label("Save");
    gtk_grid_attach(GTK_GRID(grid), b_persave, 4, 2, 1, 1);
    g_signal_connect(b_persave, "clicked", G_CALLBACK(on_pers_save), &gravtie.prinfo);
    GtkWidget *b_perreset = gtk_button_new_with_label("Reset");
    gtk_grid_attach(GTK_GRID(grid), b_perreset, 5, 2, 1, 1);
    g_signal_connect(b_perreset, "clicked", G_CALLBACK(on_pers_reset), &gravtie.prinfo);
    gravtie.prinfo.en1 = e_pers;
    gravtie.prinfo.bt1 = b_persave;
    gravtie.prinfo.bt2 = b_perreset;

    GtkWidget *personnel = gtk_label_new("Personnel: ");
    gtk_label_set_line_wrap(GTK_LABEL(personnel), TRUE);
    gtk_label_set_xalign(GTK_LABEL(personnel), 1.0);
    gtk_grid_attach(GTK_GRID(grid), personnel, 0, 2, 1, 1);

    // HEIGHTS /////////////////////////////////////////////////////////
    GtkWidget *e_h1 = gtk_entry_new(); // heights 1
    GtkWidget *e_h2 = gtk_entry_new(); // heights 2
    GtkWidget *e_h3 = gtk_entry_new(); // heights 3
    // put in placeholder text for entries
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_h1),"pier water height 1 (m)");
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_h2),"pier water height 2 (m)");
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_h3),"pier water height 3 (m)");
    // attach entries to the grid
    gtk_grid_attach(GTK_GRID(grid), e_h1, 4, 5, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), e_h2, 4, 6, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), e_h3, 4, 7, 2, 1);
    // add heights and counts entries to the tie struct so we can manipulate them in callbacks
    gravtie.heights[0].en1 = e_h1;
    gravtie.heights[1].en1 = e_h2;
    gravtie.heights[2].en1 = e_h3;

    // buttons
    GtkWidget *b_h1 = gtk_button_new_with_label("save");
    GtkWidget *b_h2 = gtk_button_new_with_label("save");
    GtkWidget *b_h3 = gtk_button_new_with_label("save");
    gtk_grid_attach(GTK_GRID(grid), b_h1, 6, 5, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_h2, 6, 6, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_h3, 6, 7, 1, 1);
    GtkWidget *b_rh1 = gtk_button_new_with_label("reset");
    GtkWidget *b_rh2 = gtk_button_new_with_label("reset");
    GtkWidget *b_rh3 = gtk_button_new_with_label("reset");
    gtk_grid_attach(GTK_GRID(grid), b_rh1, 7, 5, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_rh2, 7, 6, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_rh3, 7, 7, 1, 1);
    // connect save and reset buttons to callbacks, for water heights
    g_signal_connect(b_h1, "clicked", G_CALLBACK(on_timestamp_button), &gravtie.heights[0]);
    g_signal_connect(b_h2, "clicked", G_CALLBACK(on_timestamp_button), &gravtie.heights[1]);
    g_signal_connect(b_h3, "clicked", G_CALLBACK(on_timestamp_button), &gravtie.heights[2]);
    g_signal_connect(b_rh1, "clicked", G_CALLBACK(on_reset_button), &gravtie.heights[0]);
    g_signal_connect(b_rh2, "clicked", G_CALLBACK(on_reset_button), &gravtie.heights[1]);
    g_signal_connect(b_rh3, "clicked", G_CALLBACK(on_reset_button), &gravtie.heights[2]);
    // add save buttons to gravtie to manipulate in callbacks
    gravtie.heights[0].bt1 = b_h1;
    gravtie.heights[1].bt1 = b_h2;
    gravtie.heights[2].bt1 = b_h3;


    // COUNTS (LAND TIE) /////////////////////////////////////////////////////////
    // entries
    GtkWidget *e_a1 = gtk_entry_new();  // counts ship 1.1
    GtkWidget *e_a2 = gtk_entry_new();  // counts ship 1.2
    GtkWidget *e_a3 = gtk_entry_new();  // counts ship 1.3
    GtkWidget *e_b1 = gtk_entry_new();  // counts land 1.1
    GtkWidget *e_b2 = gtk_entry_new();  // counts land 1.2
    GtkWidget *e_b3 = gtk_entry_new();  // counts land 1.3
    GtkWidget *e_c1 = gtk_entry_new();  // counts ship 2.1
    GtkWidget *e_c2 = gtk_entry_new();  // counts ship 2.2
    GtkWidget *e_c3 = gtk_entry_new();  // counts ship 2.3
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_a1),"ship counts 1.1");
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_a2),"ship counts 1.2");
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_a3),"ship counts 1.3");
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_b1),"land counts 1.1");
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_b2),"land counts 1.2");
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_b3),"land counts 1.3");
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_c1),"ship counts 2.1");
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_c2),"ship counts 2.2");
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_c3),"ship counts 2.3");
    gtk_grid_attach(GTK_GRID(grid), e_a1, 0, 5, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), e_a2, 0, 6, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), e_a3, 0, 7, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), e_b1, 0, 8, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), e_b2, 0, 9, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), e_b3, 0, 10, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), e_c1, 0, 11, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), e_c2, 0, 12, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), e_c3, 0, 13, 2, 1);
    // set landtie entry boxes to "off" to start
    gtk_widget_set_sensitive(GTK_WIDGET(e_a1), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(e_a2), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(e_a3), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(e_b1), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(e_b2), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(e_b3), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(e_c1), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(e_c2), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(e_c3), FALSE);
    // add to tie
    gravtie.acounts[0].en1 = e_a1;
    gravtie.acounts[1].en1 = e_a2;
    gravtie.acounts[2].en1 = e_a3;
    gravtie.bcounts[0].en1 = e_b1;
    gravtie.bcounts[1].en1 = e_b2;
    gravtie.bcounts[2].en1 = e_b3;
    gravtie.ccounts[0].en1 = e_c1;
    gravtie.ccounts[1].en1 = e_c2;
    gravtie.ccounts[2].en1 = e_c3;

    // buttons
    GtkWidget *b_a1 = gtk_button_new_with_label("save");
    GtkWidget *b_a2 = gtk_button_new_with_label("save");
    GtkWidget *b_a3 = gtk_button_new_with_label("save");
    GtkWidget *b_b1 = gtk_button_new_with_label("save");
    GtkWidget *b_b2 = gtk_button_new_with_label("save");
    GtkWidget *b_b3 = gtk_button_new_with_label("save");
    GtkWidget *b_c1 = gtk_button_new_with_label("save");
    GtkWidget *b_c2 = gtk_button_new_with_label("save");
    GtkWidget *b_c3 = gtk_button_new_with_label("save");
    gtk_grid_attach(GTK_GRID(grid), b_a1, 2, 5, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_a2, 2, 6, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_a3, 2, 7, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_b1, 2, 8, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_b2, 2, 9, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_b3, 2, 10, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_c1, 2, 11, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_c2, 2, 12, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_c3, 2, 13, 1, 1);
    GtkWidget *b_ra1 = gtk_button_new_with_label("reset");
    GtkWidget *b_ra2 = gtk_button_new_with_label("reset");
    GtkWidget *b_ra3 = gtk_button_new_with_label("reset");
    GtkWidget *b_rb1 = gtk_button_new_with_label("reset");
    GtkWidget *b_rb2 = gtk_button_new_with_label("reset");
    GtkWidget *b_rb3 = gtk_button_new_with_label("reset");
    GtkWidget *b_rc1 = gtk_button_new_with_label("reset");
    GtkWidget *b_rc2 = gtk_button_new_with_label("reset");
    GtkWidget *b_rc3 = gtk_button_new_with_label("reset");
    gtk_grid_attach(GTK_GRID(grid), b_ra1, 3, 5, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_ra2, 3, 6, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_ra3, 3, 7, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_rb1, 3, 8, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_rb2, 3, 9, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_rb3, 3, 10, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_rc1, 3, 11, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_rc2, 3, 12, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_rc3, 3, 13, 1, 1);
    // connect save and reset buttons to callbacks, for land tie counts
    g_signal_connect(b_a1, "clicked", G_CALLBACK(on_timestamp_button), &gravtie.acounts[0]);
    g_signal_connect(b_a2, "clicked", G_CALLBACK(on_timestamp_button), &gravtie.acounts[1]);
    g_signal_connect(b_a3, "clicked", G_CALLBACK(on_timestamp_button), &gravtie.acounts[2]);
    g_signal_connect(b_b1, "clicked", G_CALLBACK(on_timestamp_button), &gravtie.bcounts[0]);
    g_signal_connect(b_b2, "clicked", G_CALLBACK(on_timestamp_button), &gravtie.bcounts[1]);
    g_signal_connect(b_b3, "clicked", G_CALLBACK(on_timestamp_button), &gravtie.bcounts[2]);
    g_signal_connect(b_c1, "clicked", G_CALLBACK(on_timestamp_button), &gravtie.ccounts[0]);
    g_signal_connect(b_c2, "clicked", G_CALLBACK(on_timestamp_button), &gravtie.ccounts[1]);
    g_signal_connect(b_c3, "clicked", G_CALLBACK(on_timestamp_button), &gravtie.ccounts[2]);
    g_signal_connect(b_ra1, "clicked", G_CALLBACK(on_reset_button), &gravtie.acounts[0]);
    g_signal_connect(b_ra2, "clicked", G_CALLBACK(on_reset_button), &gravtie.acounts[1]);
    g_signal_connect(b_ra3, "clicked", G_CALLBACK(on_reset_button), &gravtie.acounts[2]);
    g_signal_connect(b_rb1, "clicked", G_CALLBACK(on_reset_button), &gravtie.bcounts[0]);
    g_signal_connect(b_rb2, "clicked", G_CALLBACK(on_reset_button), &gravtie.bcounts[1]);
    g_signal_connect(b_rb3, "clicked", G_CALLBACK(on_reset_button), &gravtie.bcounts[2]);
    g_signal_connect(b_rc1, "clicked", G_CALLBACK(on_reset_button), &gravtie.ccounts[0]);
    g_signal_connect(b_rc2, "clicked", G_CALLBACK(on_reset_button), &gravtie.ccounts[1]);
    g_signal_connect(b_rc3, "clicked", G_CALLBACK(on_reset_button), &gravtie.ccounts[2]);
    // set landtie buttons (save AND reset) to inactive initially
    gtk_widget_set_sensitive(GTK_WIDGET(b_a1), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(b_a2), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(b_a3), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(b_b1), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(b_b2), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(b_b3), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(b_c1), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(b_c2), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(b_c3), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(b_ra1), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(b_ra2), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(b_ra3), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(b_rb1), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(b_rb2), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(b_rb3), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(b_rc1), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(b_rc2), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(b_rc3), FALSE);
    // add save AND reset buttons to tie
    gravtie.acounts[0].bt1 = b_a1;
    gravtie.acounts[1].bt1 = b_a2;
    gravtie.acounts[2].bt1 = b_a3;
    gravtie.bcounts[0].bt1 = b_b1;
    gravtie.bcounts[1].bt1 = b_b2;
    gravtie.bcounts[2].bt1 = b_b3;
    gravtie.ccounts[0].bt1 = b_c1;
    gravtie.ccounts[1].bt1 = b_c2;
    gravtie.ccounts[2].bt1 = b_c3;
    gravtie.acounts[0].bt2 = b_ra1;
    gravtie.acounts[1].bt2 = b_ra2;
    gravtie.acounts[2].bt2 = b_ra3;
    gravtie.bcounts[0].bt2 = b_rb1;
    gravtie.bcounts[1].bt2 = b_rb2;
    gravtie.bcounts[2].bt2 = b_rb3;
    gravtie.ccounts[0].bt2 = b_rc1;
    gravtie.ccounts[1].bt2 = b_rc2;
    gravtie.ccounts[2].bt2 = b_rc3;

    // LAND TIE TOGGLE SWITCH //////////////////////////////////////////
    GtkWidget *landtie_switch = gtk_switch_new();
    GtkWidget *landtie_label = gtk_label_new("Land tie: ");
    gtk_label_set_xalign(GTK_LABEL(landtie_label), 1.0);  // right-justify the text
    gravtie.lminfo.lts = landtie_switch;
    g_signal_connect(G_OBJECT(landtie_switch), "state-set", G_CALLBACK(landtie_switch_callback), &gravtie);
    gtk_grid_attach(GTK_GRID(grid), landtie_label, 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), landtie_switch, 1, 3, 1, 1);

    GtkWidget *lt_lon = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(lt_lon), "Ship lon (deg)");
    GtkWidget *lt_lat = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(lt_lat), "Ship lat (deg)");
    GtkWidget *lt_elev = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(lt_elev), "Elevation (m)");
    GtkWidget *lt_temp = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(lt_temp), "Temperature (C)");
    gtk_grid_attach(GTK_GRID(grid), lt_lon, 2, 3, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), lt_lat, 4, 3, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), lt_elev, 6, 3, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), lt_temp, 8, 3, 2, 1);
    gtk_widget_set_sensitive(GTK_WIDGET(lt_lon), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(lt_lat), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(lt_elev), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(lt_temp), FALSE);
    GtkWidget *b_lt_coords = gtk_button_new_with_label("save");
    gtk_grid_attach(GTK_GRID(grid), b_lt_coords, 10, 3, 1, 1);
    gtk_widget_set_sensitive(GTK_WIDGET(b_lt_coords), FALSE);
    GtkWidget *br_coords = gtk_button_new_with_label("reset");
    gtk_grid_attach(GTK_GRID(grid), br_coords, 11, 3, 1, 1);
    gtk_widget_set_sensitive(GTK_WIDGET(br_coords), FALSE);
    gravtie.lminfo.en_lon = lt_lon;
    gravtie.lminfo.en_lat = lt_lat;
    gravtie.lminfo.en_elev = lt_elev;
    gravtie.lminfo.en_temp = lt_temp;
    gravtie.lminfo.bt_coords = b_lt_coords;
    gravtie.lminfo.br_coords = br_coords;
    g_signal_connect(b_lt_coords, "clicked", G_CALLBACK(on_lm_coordsave_button), &gravtie.lminfo);
    g_signal_connect(br_coords, "clicked", G_CALLBACK(on_lm_coordreset_button), &gravtie.lminfo);

    GtkWidget *ltval_label = gtk_label_new("Land tie value: ");
    gtk_label_set_xalign(GTK_LABEL(ltval_label), 0.0);  // right-justify the text
    gtk_label_set_line_wrap(GTK_LABEL(ltval_label), TRUE);
    gravtie.lminfo.lt_label = ltval_label;
    gtk_grid_attach(GTK_GRID(grid), ltval_label, 2, 14, 2, 1);

    // IMPERIAL/METRIC TOGGLE SWITCH TODO //////////////////////////////

    // DGS FILE LOAD/PLOT //////////////////////////////////////////////
    GtkWidget *b_dgschoose = gtk_button_new_with_label("Choose DGS file");
    gtk_grid_attach(GTK_GRID(grid), b_dgschoose, 8, 5, 2, 1);
    g_signal_connect(b_dgschoose, "clicked", G_CALLBACK(on_dgs_filebrowse_clicked), &gravtie.shinfo);
    GtkWidget *b_dgsclear = gtk_button_new_with_label("Clear grav data");
    gtk_grid_attach(GTK_GRID(grid), b_dgsclear, 8, 6, 2, 1);
    g_signal_connect(b_dgsclear, "clicked", G_CALLBACK(on_dgs_clear_clicked), &gravtie.shinfo);
    GtkWidget *dgs_label = gtk_label_new("  0 datapoints");
    gtk_label_set_xalign(GTK_LABEL(dgs_label), 0.0);  // left-justify the text
    gtk_grid_attach(GTK_GRID(grid), dgs_label, 10, 5, 2, 1);
    gravtie.shinfo.dgs_label = dgs_label;

    //GtkWidget *b_dgsplot = gtk_button_new_with_label("plot data"); // TODO plotting
    //gtk_grid_attach(GTK_GRID(grid), b_dgsplot, 8, 6, 2, 1);

    // COMPUTE LAND TIE ////////////////////////////////////////////////////
    GtkWidget *b_complandtie = gtk_button_new_with_label("compute land tie");
    gtk_grid_attach(GTK_GRID(grid), b_complandtie, 0, 14, 2, 1);
    g_signal_connect(b_complandtie, "clicked", G_CALLBACK(on_compute_landtie), &gravtie);
    gravtie.lt_button = b_complandtie;  // add to tie struct for callbacks
    gtk_widget_set_sensitive(GTK_WIDGET(b_complandtie), FALSE);

    // COMPUTE BIAS ////////////////////////////////////////////////////////
    GtkWidget *b_bias = gtk_button_new_with_label("compute bias");
    gtk_grid_attach(GTK_GRID(grid), b_bias, 8, 8, 2, 1);
    g_signal_connect(b_bias, "clicked", G_CALLBACK(on_compute_bias), &gravtie);
    GtkWidget *bias_label = gtk_label_new("Computed bias: ");
    gtk_label_set_xalign(GTK_LABEL(bias_label), 0.0);  // left-justify the text
    gtk_label_set_line_wrap(GTK_LABEL(bias_label), TRUE);
    gtk_grid_attach(GTK_GRID(grid), bias_label, 10, 8, 2, 1);
    gravtie.bias_label = bias_label;
    GtkWidget *b_biasclear = gtk_button_new_with_label("clear bias");
    gtk_grid_attach(GTK_GRID(grid), b_biasclear, 8, 9, 2, 1);
    g_signal_connect(b_biasclear, "clicked", G_CALLBACK(on_clear_bias), &gravtie);
    

    // SAVE/LOAD BUTTONS ///////////////////////////////////////////////////
    GtkWidget *button1 = gtk_button_new_with_label("Load saved tie");
    GtkWidget *button21 = gtk_button_new_with_label("save current tie");
    GtkWidget *button22 = gtk_button_new_with_label("output report");
    gtk_grid_attach(GTK_GRID(grid), button1, 8, 11, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), button21, 8, 12, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), button22, 8, 13, 2, 1);
    g_signal_connect(button21, "clicked", G_CALLBACK(on_savetie_clicked), &gravtie);
    g_signal_connect(button1, "clicked", G_CALLBACK(on_readtie_clicked), &gravtie);
    g_signal_connect(button22, "clicked", G_CALLBACK(on_write_report), &gravtie);

    ////////////////////////////////////////////////////////////////////////
    // final stages for cleanup
    ////////////////////////////////////////////////////////////////////////
    // Handle the window destroy event
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    // Show all widgets
    gtk_widget_show_all(window);
    // Start the GTK main loop
    gtk_main();

    return 0;
}

