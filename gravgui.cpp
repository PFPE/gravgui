#include <gtk/gtk.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <numeric>
#include <algorithm>
#include <ctime>
#include "lib/filt.h"       // constructs for applying FIR filter(s)
#include "lib/window_functions.h"   // calculate windows for filters
//#include <cairo.h>  // will need later for plotting if we try to do that


// This program is for a gravity tie GUI using GTK
// Database files (database/*) must be in the dir where the program is run
// By Hannah F. Mark, October 2023
// with help from GPT-3.5 via OpenAI
// for PFPE

// the current setup does not allow manual timestamping, so to test the bias calc, there is a
// debug option:
bool debug_dgs = true;
// this will set up a kludge-y thing where the timestamps for entered water heights
// are replaced with values pulled from whatever DGS file is imported
// it assumes that dgs file has at least 4000 values (preferably lots more)

////////////////////////////////////////////////////////////////////////
// constants, global bc why not
////////////////////////////////////////////////////////////////////////
double faafactor = 0.3086;
double GravCal = 414125;
float otherfactor = 8388607;
float g0 = 10000;

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

////////////////////////////////////////////////////////////////////////
// functions for reading files
////////////////////////////////////////////////////////////////////////

time_t my_timegm(struct tm * t)
/* struct tm to seconds since Unix epoch */
{
    long year;
    time_t result;
#define MONTHSPERYEAR   12      /* months per calendar year */
    static const int cumdays[MONTHSPERYEAR] =
        { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

    /*@ +matchanyintegral @*/
    year = 1900 + t->tm_year + t->tm_mon / MONTHSPERYEAR;
    result = (year - 1970) * 365 + cumdays[t->tm_mon % MONTHSPERYEAR];
    result += (year - 1968) / 4;
    result -= (year - 1900) / 100;
    result += (year - 1600) / 400;
    if ((year % 4) == 0 && ((year % 100) != 0 || (year % 400) == 0) &&
        (t->tm_mon % MONTHSPERYEAR) < 2)
        result--;
    result += t->tm_mday - 1;
    result *= 24;
    result += t->tm_hour;
    result *= 60;
    result += t->tm_min;
    result *= 60;
    result += t->tm_sec;
    if (t->tm_isdst == 1)
        result -= 3600;
    /*@ -matchanyintegral @*/
    return (result);
}

std::tm str_to_tm(const char* datestr, int tflag) {
    std::tm t = {};

    if (tflag == 1) {
        if (std::sscanf(datestr, "%d/%d/%d-%d:%d:%d", &t.tm_mon, &t.tm_mday, &t.tm_year, &t.tm_hour, &t.tm_min, &t.tm_sec) != 6) {
            return t;  // TODO more consequences here?
        }
    }
    if (tflag == 2) {
        if (std::sscanf(datestr, "%d-%d-%dT%d:%d:%dZ", &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec) != 6) {
            return t;  // TODO more consequences here?
        }
    }

    t.tm_year -= 1900;
    --t.tm_mon;
    t.tm_isdst = -1;
    return t;
}


// read stations.db and save key="VALUE" pairs in a map of maps
std::map<std::string, std::map<std::string, std::string> > readStationFile(const std::string& filePath) {
    std::map<std::string, std::map<std::string, std::string> > stationData;
    std::ifstream inputFile(filePath);

    if (!inputFile.is_open()) {
        std::cerr << "Error: Unable to open file " << filePath << std::endl;
        return stationData; // Return an empty map on error
    }

    std::string line;
    std::string currentStationKey;
    int counter;

    counter = 0;
    while (std::getline(inputFile, line)) {
        if (line.empty()) {
            continue; // Skip empty lines
        }

        if (line[0] == '[' && line[line.length() - 1] == ']') {
            // Found a section header
            std::string sectionHeader = line.substr(1, line.length() - 2);
            if (sectionHeader.find("STATION_") == 0) {
                // This is a [STATION_] header, process it
                //currentStationKey = sectionHeader;
                currentStationKey = std::to_string(counter);
                currentStationKey.insert(0,2-currentStationKey.length(),'0');
                stationData[currentStationKey] = std::map<std::string, std::string>();
                counter++;
            } else {
                // Skip this section
                currentStationKey.clear();
            }
        } else if (!currentStationKey.empty()) {
            // Parse key-value pairs and add them to the current station's map
            size_t delimiterPos = line.find('=');
            if (delimiterPos != std::string::npos) {
                std::string key = line.substr(0, delimiterPos);
                std::string value = line.substr(delimiterPos + 1);
                if (value.front() == '"') {
                    value = value.substr(1,value.size());
                }
                if (value.back() == '"') {
                    value = value.substr(0,value.size() - 1);
                }
                stationData[currentStationKey][key] = value;
            }
        }
    }
    inputFile.close();
    return stationData;
}

// read landmeters.db and save meter names and cal table paths in a map
std::map<std::string, std::map<std::string, std::string> > readMeterFile(const std::string& filePath) {
    std::map<std::string, std::map<std::string, std::string> > meterData;
    std::ifstream inputFile(filePath);

    if (!inputFile.is_open()) {
        std::cerr << "Error: Unable to open file " << filePath << std::endl;
        return meterData; // Return an empty map on error
    }

    std::string line;
    std::string currentMeterKey;
    int counter;

    counter = 0;
    while (std::getline(inputFile, line)) {
        if (line.empty()) {
            continue; // Skip empty lines
        }

        if (line[0] == '[' && line[line.length() - 1] == ']') {
            // Found a section header
            std::string sectionHeader = line.substr(1, line.length() - 2);
            if (sectionHeader.find("LAND_METER_") == 0) {
                // This is a header, process it
                currentMeterKey = std::to_string(counter);
                currentMeterKey.insert(0,2-currentMeterKey.length(),'0');
                meterData[currentMeterKey] = std::map<std::string, std::string>();
                counter++;
            } else {
                // Skip this section
                currentMeterKey.clear();
            }
        } else if (!currentMeterKey.empty()) {  // we are in a section
            // Parse key-value pairs and add them to the current station's map
            size_t delimiterPos = line.find('=');
            if (delimiterPos != std::string::npos) {
                std::string key = line.substr(0, delimiterPos);
                std::string value = line.substr(delimiterPos + 1);
                if (value.front() == '"') {
                    value = value.substr(1,value.size());
                }
                if (value.back() == '"') {
                    value = value.substr(0,value.size() - 1);
                }
                meterData[currentMeterKey][key] = value;
            }
        }
    }
    inputFile.close();
    return meterData;
}

// read a calibration file for a landmeter
calibration read_lm_calib(const std::string& filePath) {
    calibration calib;  // def object for return
    std::ifstream inputFile(filePath); // calibration file path

    if (!inputFile.is_open()) {
        std::cerr << "Error: Unable to open file " << filePath << std::endl;
        return calib; // Return empty on error
    }

    std::string line;
    while (std::getline(inputFile, line)) {
        if (line.empty()) {
            continue; // Skip empty lines
        }
        std::istringstream iss(line); // stream the line
        std::vector<float> floats;
        std::string token;
        while (iss >> token) { // check to see if tokens are floats
            try {
                floats.push_back(std::stof(token));
            } catch (std::invalid_argument&) { // non-float-convertible things get skipped
            }
        }
        if (floats.size() == 3) { // read three floats from the line!
            calib.brackets.push_back(floats[0]);
            calib.mgvals.push_back(floats[1]);
            calib.factors.push_back(floats[2]);
        }
    }
    inputFile.close();
    return calib;
}

// function for reading a DGS laptop file and returning timestamps and grav values
std::pair<std::vector<float>, std::vector<std::time_t> > read_dat_dgs(const std::vector<std::string>& file_paths, const std::string& ship) {
    std::vector<float> rgrav;
    std::vector<time_t> stamps;

    if (ship == "R/V Atlantis" || ship == "R/V Revelle" || ship == "R/V Palmer" || ship == "R/V Ride" || ship == "R/V Thompson") {
    } else {
        std::cout << "this ship is not supported for DGS laptop file read" << std::endl;
        return std::make_pair(rgrav,stamps);
    }

    for (const std::string& file_path : file_paths) {  // loop file paths
        std::ifstream file(file_path);
        if (!file.is_open()) {  // try to open file and see if it works
            throw std::runtime_error("Failed to open file: " + file_path);
        }
        std::string line;
        while (std::getline(file, line)) {  // loop lines of the file
            if (line.empty()) {
                continue;  // Skip empty lines
            }
            // navigate through comma-separated string, tokenize
            std::istringstream iss(line);
            std::string token;
            std::vector<std::string> tokens;

            while (std::getline(iss, token, ',')) {
                tokens.push_back(token);
            }
            // ship-specific formats (need more info for this TODO)
            if (ship == "R/V Atlantis" || ship == "R/V Revelle" || ship == "R/V Palmer" || ship == "R/V Ride") {
                rgrav.push_back(std::stof(tokens[1]));

                int year = std::stoi(tokens[19]);
                int month = std::stoi(tokens[20]);
                int day = std::stoi(tokens[21]);
                int hour = std::stoi(tokens[22]);
                int minute = std::stoi(tokens[23]);
                int second = std::stoi(tokens[24]);

                std::tm timestamp = {};
                timestamp.tm_year = year - 1900;  // std::tm uses years since 1900
                timestamp.tm_mon = month - 1;     // and months start at 0
                timestamp.tm_mday = day;          // but days do start at 1
                timestamp.tm_hour = hour;
                timestamp.tm_min = minute;
                timestamp.tm_sec = second;
                time_t outtime = my_timegm(&timestamp); //std::mktime(&timestamp);
                stamps.push_back(outtime);

            } else if (ship == "R/V Thompson") {

                rgrav.push_back(std::stof(tokens[3]));
                std::string datetime_str = tokens[0] + "-" + tokens[1];

                std::tm timestamp = str_to_tm(datetime_str.c_str(), 1);
                //std::tm timestamp = {};
                //if (strptime(datetime_str.c_str(), "%m/%d/%Y-%H:%M:%S", &timestamp) != nullptr) {
                    time_t outtime = my_timegm(&timestamp);  //std::mktime(&timestamp);
                    stamps.push_back(outtime);
                //}
            } 
        }
        file.close();
    }
    return std::make_pair(rgrav,stamps);
}

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

    std::vector<double> mgal_a{3};
    std::vector<time_t> tmgal_a{3};

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
                if (value.front() == '"') {  // strip quotes from strings TODO check this for empty str
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
                if (key.substr(2,4)==".t"){
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
                        if (key.substr(0,2)=="bb") tmgal_a[0] = outtime;
                        if (key.substr(0,2)=="cc") tmgal_a[0] = outtime;
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
        outputFile << "Meter temperature: " << std::endl;  // TODO TODO TODO
        outputFile << std::endl;
        outputFile << "#New station A:" << std::endl;
        outputFile << "Name: " << gravtie->shinfo.ship << std::endl;
        outputFile << "Latitude (deg): " << std::fixed << std::setprecision(3) << gravtie->lminfo.ship_lat << std::endl;
        outputFile << "Longitude (deg): " << std::fixed << std::setprecision(3) << gravtie->lminfo.ship_lon << std::endl;
        outputFile << "Elevation (m): " << std::endl; // TODO TODO TODO

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

////////////////////////////////////////////////////////////////////////
// callback functions
////////////////////////////////////////////////////////////////////////

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
        // TODO clear entry text bc it is invalid?
    }
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

static void on_lm_filebrowse_clicked(GtkWidget *button, gpointer data) {
    lm_info* lminfo = static_cast<lm_info*>(data);
    //parent window for file chooser not strictly necessary though it is recommended
    GtkWidget *file_chooser = gtk_file_chooser_dialog_new("Select File",
                                                           NULL, //GTK_WINDOW(user_data), 
                                                           GTK_FILE_CHOOSER_ACTION_OPEN,
                                                           "_Cancel",
                                                           GTK_RESPONSE_CANCEL,
                                                           "_Open",
                                                           GTK_RESPONSE_ACCEPT,
                                                           NULL);

    if (gtk_dialog_run(GTK_DIALOG(file_chooser)) == GTK_RESPONSE_ACCEPT) {
        char* filename;
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_chooser));
        // read the file into three vectors
        std::string sf = filename;
        calibration calib2 = read_lm_calib(sf);
        // push back into the external calib, overwriting
        lminfo->calib.brackets.clear();
        lminfo->calib.factors.clear();
        lminfo->calib.mgvals.clear();
        lminfo->calib.brackets = calib2.brackets;
        lminfo->calib.factors = calib2.factors;
        lminfo->calib.mgvals = calib2.mgvals;

        lminfo->cal_file_path = sf;
    }
    gtk_widget_destroy(file_chooser);
    //std::cout << lminfo->calib.brackets.size() << std::endl;
    char buffer[40]; // Adjust size?
    snprintf(buffer, sizeof(buffer), "%i calibration lines read", (int) lminfo->calib.brackets.size());
    gtk_label_set_text(GTK_LABEL(lminfo->cal_label), buffer); 

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

static void on_dgs_filebrowse_clicked(GtkWidget *button, gpointer data) {
    ship_info* shinfo = static_cast<ship_info*>(data);
    if (shinfo->ship != "") {  // require that a ship be selected first
        //parent window for file chooser not strictly necessary though it is recommended
        GtkWidget *file_chooser = gtk_file_chooser_dialog_new("Select File",
                                                               NULL, //GTK_WINDOW(user_data), 
                                                               GTK_FILE_CHOOSER_ACTION_OPEN,
                                                               "_Cancel",
                                                               GTK_RESPONSE_CANCEL,
                                                               "_Open",
                                                               GTK_RESPONSE_ACCEPT,
                                                               NULL);
        // enable multiple-file selection
        gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(file_chooser), TRUE);

        if (gtk_dialog_run(GTK_DIALOG(file_chooser)) == GTK_RESPONSE_ACCEPT) {
            GSList *fileList = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(file_chooser));
            // Convert the GSList to a C++ vector
            std::vector<std::string> selectedFiles;
            for (GSList *files = fileList; files; files = g_slist_next(files)) {
                const gchar *filename = static_cast<const gchar *>(files->data);
                selectedFiles.push_back(filename);
            }
            // read the file into a pair of vectors
            std::pair<std::vector<float>, std::vector<time_t> > datadata = read_dat_dgs(selectedFiles,shinfo->ship);
            // push back into shinfo - can add to existing vector, will not ovrwrite
            shinfo->gravgrav.insert(shinfo->gravgrav.end(),datadata.first.begin(), datadata.first.end());
            shinfo->gravtime.insert(shinfo->gravtime.end(),datadata.second.begin(), datadata.second.end());
            g_slist_free_full(fileList, g_free);
        }
        gtk_widget_destroy(file_chooser);
//    } else { // no ship selected prior! remind:  // TODO (need to turn off highlighting with select)
//        GtkStyleContext *context;
//        context = gtk_widget_get_style_context(shinfo->cb1);
//        gtk_style_context_add_class(context, "highlighted");
        char dstring[30];
        sprintf(dstring,"  %i datapoints", (int) shinfo->gravgrav.size());
        gtk_label_set_text(GTK_LABEL(shinfo->dgs_label), dstring); 
    }
}

// clear any DGS data read into shinfo vectors (don't clear ship though)
void on_dgs_clear_clicked(GtkWidget *button, gpointer data) {
    ship_info* shinfo = static_cast<ship_info*>(data);
    shinfo->gravgrav.clear();
    shinfo->gravtime.clear();
    gtk_label_set_text(GTK_LABEL(shinfo->dgs_label), "  0 datapoints"); 
}

void on_compute_bias(GtkWidget *button, gpointer data) {
    tie* gravtie = static_cast<tie*>(data);  // we def need the whole tie for this one
    std::vector<float> heights;  // get all heights and timestamps for water grav calc
    std::vector<time_t> height_stamps;
    double water_grav=-999;
    double avg_dgs_grav=-99999;
    double pier_grav;
    double avg_height=-999;

    if (debug_dgs && gravtie->shinfo.gravgrav.size() == 0) return;

    if (gravtie->heights[0].h1 != -999) {
        heights.push_back(-1*std::abs(gravtie->heights[0].h1));  // all heights should be negative numbers
        if (debug_dgs) {
            height_stamps.push_back(gravtie->shinfo.gravtime[2000]); // kludge for timestamps from file
        } else {
            height_stamps.push_back(gravtie->heights[0].t1);
        }
    }
    if (gravtie->heights[1].h1 != -999) {
        heights.push_back(-1*std::abs(gravtie->heights[1].h1));
        if (debug_dgs) {
            height_stamps.push_back(gravtie->shinfo.gravtime[3000]);
        } else {
            height_stamps.push_back(gravtie->heights[1].t1);
        }
    }
    if (gravtie->heights[2].h1 != -999) {
        heights.push_back(-1*std::abs(gravtie->heights[2].h1));
        if (debug_dgs) {
            height_stamps.push_back(gravtie->shinfo.gravtime[4000]);
        } else {
            height_stamps.push_back(gravtie->heights[2].t1);
        }
    }
    // check for land tie if we are using one
    if (gravtie->lminfo.landtie && gravtie->lminfo.land_tie_value > 0) { // bool, and have a value
        pier_grav = gravtie->lminfo.land_tie_value;
    } else {
        pier_grav = gravtie->stinfo.station_gravity;
    }
    if (heights.size() > 0) { // make sure there is at least one height measurement provided
        if (pier_grav > 0) {// check to make sure we have an absolute grav value
            auto const count = static_cast<float>(heights.size());
            avg_height = std::accumulate(heights.begin(), heights.end(), 0.0) / count;
            water_grav = pier_grav + faafactor*avg_height;
        }
    } else {
        return;  // if no heights, no point in trying to calc things
    }
    // check to make sure we have DGS data *and* it covers the heights time window
    if (gravtie->shinfo.gravgrav.size() > 0) { // we have some meter data
        // sort the gravity values and timestamps from whatever files were read into shinfo
        std::vector<time_t> gtime = gravtie->shinfo.gravtime;
        std::vector<size_t> indices(gtime.size());  // vector of indices for sorting
        std::iota(indices.begin(), indices.end(), 0);
        // Sort indices based on timestamps
        std::sort(indices.begin(), indices.end(), [&gtime](size_t i, size_t j) {
            return gtime[i] < gtime[j];
        });
        // Use sorted indices to reorder the grav and time vectors
        std::vector<float> sortedgrav;
        std::vector<time_t> sortedtime;
        for (size_t i = 0; i < indices.size(); i++) {
            sortedgrav.push_back(gravtie->shinfo.gravgrav[indices[i]]);
            sortedtime.push_back(gtime[indices[i]]);
        }

        time_t result1 = *std::min_element(sortedtime.begin(), sortedtime.end());
        time_t result2 = *std::min_element(height_stamps.begin(), height_stamps.end());

        time_t result3 = *std::max_element(sortedtime.begin(), sortedtime.end());
        time_t result4 = *std::max_element(height_stamps.begin(), height_stamps.end());

        // now that things are sorted, make sure gravtime covers heights timespan
        if (result1 < result2 && result3 > result4) {
            //std::cout << "data coverage!" << std::endl;

            // with sufficient data coverage, apply a filter to the whole grav time series
            // set filter parameters based on length of *sliced* grav around heights times
            auto start = std::lower_bound(sortedtime.begin(), sortedtime.end(), result2);
            auto end = std::upper_bound(sortedtime.begin(), sortedtime.end(), result4);
            int lower_index = std::distance(sortedtime.begin(), start);
            int upper_index = std::distance(sortedtime.begin(), end);
            int ntaps = std::round((upper_index - lower_index)/10);
            // design Blackman window with ntaps based on eventual length of data
            Filter *blackman_filt;
            double out_val;
            blackman_filt = new Filter(Blackman, ntaps, 1, 0.1, 0.2); // only name and ntaps matter, TODO
            // apply filter with zero padding
            std::vector<double> firstpass;
            for (int i=0; i<ntaps; i++) {  // zero-pad front
                out_val = blackman_filt->do_sample( (double) 0);
                firstpass.push_back(out_val);
            }
            for (int i=0; i<sortedgrav.size(); i++) {
                out_val = blackman_filt->do_sample( (double) sortedgrav[i]);
                firstpass.push_back(out_val);
            }
            for (int i=0; i<ntaps; i++) {  // zero-pad end
                out_val = blackman_filt->do_sample( (double) 0);
                firstpass.push_back(out_val);
            }
            // trim firstpass and repeat for secondpass
            std::vector<double> firsttrim(firstpass.begin()+ntaps, firstpass.begin()+ntaps+sortedgrav.size());
            std::vector<double> secondpass;
            for (int i=0; i<ntaps; i++) {  // zero-pad front
                out_val = blackman_filt->do_sample( (double) 0);
                secondpass.push_back(out_val);
            }
            for (int i=0; i<sortedgrav.size(); i++) {
                out_val = blackman_filt->do_sample( (double) firsttrim[i]);
                secondpass.push_back(out_val);
            }
            for (int i=0; i<ntaps; i++) {  // zero-pad end
                out_val = blackman_filt->do_sample( (double) 0);
                secondpass.push_back(out_val);
            }
            // trim secondpass padding and reverse the vector
            std::vector<double> secondtrim(secondpass.begin()+ntaps, secondpass.begin()+ntaps+firsttrim.size());
            std::reverse(secondtrim.begin(), secondtrim.end());

            // now from filtered series, get the slice of grav data that we will average here
            // (indices were caluclated before to figure out ntaps)
            std::vector<double> result(secondtrim.begin() + lower_index, secondtrim.begin() + upper_index);

            // average the filtered and sliced data to get the avg gravity for the bias calc
            double gravsum = 0.0;
            for (const double& value : result) {
                gravsum += value;
            }
            avg_dgs_grav = gravsum/result.size();

        } else {  // TODO hightlight something? DGS load button?
            //std::cout << "no data coverage!" << std::endl;
        }
    } else {
        return;  // if no grav data loaded, can't do anything else
    }
    // calculate! put value in a label so it is visible!
    double bias = water_grav - avg_dgs_grav;
    gravtie->bias = bias;
    gravtie->avg_dgs_grav = avg_dgs_grav;
    gravtie->water_grav = water_grav;
    gravtie->avg_height = avg_height;

    std::stringstream strm;
    strm << std::fixed << std::setprecision(2) << bias;
    std::string test = strm.str();
    char bstring[test.length()+16];
    sprintf(bstring,"Computed bias: %.2f", bias);
    gtk_label_set_text(GTK_LABEL(gravtie->bias_label), bstring);
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

// convert counts to mgals for one timestamped thing and given calibration table
double convert_counts_mgals (val_time c1, calibration calib) {
    double mgals = 0.;
    int cind = 0;
    double min_diff = std::abs(c1.h1 - calib.brackets[cind]);

    for (int i=1; i<calib.brackets.size(); i++) {
        double diff = std::abs(c1.h1 - calib.brackets[i]);
        if (diff < min_diff && calib.brackets[i] <= c1.h1) {
            cind = i;
            min_diff = diff;
        }  // TODO catch if we fall off the end of the table
    }
    double residual_reading = c1.h1 - calib.brackets[cind];  // subtract bracket from counts
    mgals = residual_reading*calib.factors[cind] + calib.mgvals[cind]; // calibrate to mgals!
    return mgals;
}

void on_compute_landtie(GtkWidget *button, gpointer data) {
    tie* gravtie = static_cast<tie*>(data);  // we def need the whole tie for this one

    // first check if we have a calibration table loaded
    if (gravtie->lminfo.calib.brackets.size() == 0) {  // no calibration table read
        return; // we can't do counts conversion so no point here
    }
    float ref_g = gravtie->stinfo.station_gravity;
    if (ref_g < 0) {
        return;  // no station gravity to reference to -> nothing to tie our land tie to
    }

    // for each set of counts measurements, check for values, convert to mgals, and get avgs
    std::vector<double> mgal_averages;
    std::vector<time_t> t_averages;

    // un-vectorize this a bit bc otherwise we can't save mgal values to tie
    // two layers of vectors was too much for referencing for some reason I do not understand
    double mgal_sum = 0;
    time_t t_sum = 0;
    int icounts = 0;
    for (val_time& thisone : gravtie->acounts) {
        if (thisone.h1 > 0) {
            double mgalval = convert_counts_mgals(thisone,gravtie->lminfo.calib);
            thisone.m1 = mgalval;
            mgal_sum += mgalval;
            t_sum += thisone.t1;
            icounts += 1;
        }
    }
    if (icounts == 0) return;  // no a1 counts
    mgal_averages.push_back(mgal_sum/icounts);
    t_averages.push_back(t_sum/icounts);

    mgal_sum = 0;
    t_sum = 0;
    icounts = 0;
    for (val_time& thisone : gravtie->bcounts) {
        if (thisone.h1 > 0) {
            double mgalval = convert_counts_mgals(thisone,gravtie->lminfo.calib);
            thisone.m1 = mgalval;
            mgal_sum += mgalval;
            t_sum += thisone.t1;
            icounts += 1;
        }
    }
    if (icounts == 0) return;  // no b1 counts
    mgal_averages.push_back(mgal_sum/icounts);
    t_averages.push_back(t_sum/icounts);

    mgal_sum = 0;
    t_sum = 0;
    icounts = 0;
    for (val_time& thisone : gravtie->ccounts) {
        if (thisone.h1 > 0) {
            double mgalval = convert_counts_mgals(thisone,gravtie->lminfo.calib);
            thisone.m1 = mgalval;
            mgal_sum += mgalval;
            t_sum += thisone.t1;
            icounts += 1;
        }
    }
    if (icounts == 0) return;  // no a2 counts
    mgal_averages.push_back(mgal_sum/icounts);
    t_averages.push_back(t_sum/icounts);

    // use averaged times and mgals to do drift and tie calc
    double AA_timedelta = difftime(t_averages[2], t_averages[0]);
    double AB_timedelta = difftime(t_averages[1], t_averages[0]);
    double drift = (mgal_averages[2] - mgal_averages[0])/AA_timedelta;

    double dc_avg_mgals_B = mgal_averages[1] - AB_timedelta*drift;
    double gdiff = mgal_averages[0] - dc_avg_mgals_B;
    double land_tie_value = ref_g + gdiff;
    gravtie->lminfo.land_tie_value = land_tie_value;
    gravtie->drift = drift;
    gravtie->mgal_averages = mgal_averages;
    gravtie->t_averages = t_averages;

    char buffer[40]; // Adjust size?
    snprintf(buffer, sizeof(buffer), "Land tie value: %.2f", land_tie_value);
    gtk_label_set_text(GTK_LABEL(gravtie->lminfo.lt_label), buffer); 
}

// callback function for land tie toggle switch: bool, active/inactive
static void landtie_switch_callback(GtkSwitch *switch_widget, gboolean state, gpointer data) {
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


static void on_savetie_clicked(GtkWidget *button, gpointer data) {
    tie* gravtie = static_cast<tie*>(data);
    //parent window for file chooser not strictly necessary though it is recommended
    GtkWidget *file_chooser = gtk_file_chooser_dialog_new("Save tie to file",
                                                           NULL, //GTK_WINDOW(user_data), 
                                                           GTK_FILE_CHOOSER_ACTION_SAVE,
                                                           "_Cancel",
                                                           GTK_RESPONSE_CANCEL,
                                                           "_Save",
                                                           GTK_RESPONSE_ACCEPT,
                                                           NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(file_chooser), true);
    time_t rawtime;
    struct tm * cookedtime;
    char buffer[30];
    time (&rawtime);
    cookedtime = localtime(&rawtime);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d.toml", cookedtime);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(file_chooser), buffer);
    if (gtk_dialog_run(GTK_DIALOG(file_chooser)) == GTK_RESPONSE_ACCEPT) {
        char *savename;
        savename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_chooser));
        // TODO actually serialize/save tie to that file
        tie_to_toml(savename, gravtie);
    }

    gtk_widget_destroy(file_chooser);
    //char dstring[30];
    //sprintf(dstring,"  %i datapoints", (int) shinfo->gravgrav.size());
    //gtk_label_set_text(GTK_LABEL(shinfo->dgs_label), dstring); 
}

static void on_readtie_clicked(GtkWidget *button, gpointer data) {
    tie* gravtie = static_cast<tie*>(data);
    //parent window for file chooser not strictly necessary though it is recommended
    GtkWidget *file_chooser = gtk_file_chooser_dialog_new("Select File",
                                                           NULL, //GTK_WINDOW(user_data), 
                                                           GTK_FILE_CHOOSER_ACTION_OPEN,
                                                           "_Cancel",
                                                           GTK_RESPONSE_CANCEL,
                                                           "_Open",
                                                           GTK_RESPONSE_ACCEPT,
                                                           NULL);

    if (gtk_dialog_run(GTK_DIALOG(file_chooser)) == GTK_RESPONSE_ACCEPT) {
        char* filename;
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_chooser));
        toml_to_tie(filename, gravtie);
    }
    gtk_widget_destroy(file_chooser);

}

static void on_write_report(GtkWidget *button, gpointer data) {
    tie* gravtie = static_cast<tie*>(data);
    //parent window for file chooser not strictly necessary though it is recommended
    GtkWidget *file_chooser = gtk_file_chooser_dialog_new("Save report to file",
                                                           NULL, //GTK_WINDOW(user_data), 
                                                           GTK_FILE_CHOOSER_ACTION_SAVE,
                                                           "_Cancel",
                                                           GTK_RESPONSE_CANCEL,
                                                           "_Save",
                                                           GTK_RESPONSE_ACCEPT,
                                                           NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(file_chooser), true);
    time_t rawtime;
    struct tm * cookedtime;
    char buffer[30];
    time (&rawtime);
    cookedtime = localtime(&rawtime);
    strftime(buffer, sizeof(buffer), "tie_%Y%m%d.txt", cookedtime);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(file_chooser), buffer);
    if (gtk_dialog_run(GTK_DIALOG(file_chooser)) == GTK_RESPONSE_ACCEPT) {
        char *savename;
        savename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_chooser));
        write_report(savename, gravtie);
    }

    gtk_widget_destroy(file_chooser);
    //char dstring[30];
    //sprintf(dstring,"  %i datapoints", (int) shinfo->gravgrav.size());
    //gtk_label_set_text(GTK_LABEL(shinfo->dgs_label), dstring); 
}

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
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 600);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);

    // Create a grid and put it in the window
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), gboolean FALSE);
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
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_othership),"If 'other', enter ship name here");
    GtkWidget *b_saveship = gtk_button_new_with_label("Save ship");
    g_signal_connect(b_saveship, "clicked", G_CALLBACK(on_ship_save), &gravtie.shinfo);
    gtk_grid_attach(GTK_GRID(grid), b_saveship, 6, 0, 1, 1);
    GtkWidget *b_resetship = gtk_button_new_with_label("Reset ship");
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
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_othersta),"If 'other', enter station name here");
    gtk_grid_attach(GTK_GRID(grid), e_othersta, 4, 1, 2, 1);
    GtkWidget *b_savesta = gtk_button_new_with_label("Save station");
    g_signal_connect(b_savesta, "clicked", G_CALLBACK(on_sta_save), &gravtie.stinfo);
    gtk_grid_attach(GTK_GRID(grid), b_savesta, 6, 1, 1, 1);
    GtkWidget *b_resetsta = gtk_button_new_with_label("Reset station");
    g_signal_connect(b_resetsta, "clicked", G_CALLBACK(on_sta_reset), &gravtie.stinfo);
    gtk_grid_attach(GTK_GRID(grid), b_resetsta, 7, 1, 1, 1);

    // box and button for absolute gravity when we need it
    GtkWidget *e_absgrav = gtk_entry_new();  // other station
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_absgrav),"enter absolute gravity");
    gtk_widget_set_sensitive(GTK_WIDGET(e_absgrav), FALSE);
    gtk_grid_attach(GTK_GRID(grid), e_absgrav, 8, 1, 2, 1);
    GtkWidget *b_savegrav = gtk_button_new_with_label("Save grav");
    gtk_widget_set_sensitive(GTK_WIDGET(b_savegrav), FALSE);
    g_signal_connect(b_savegrav, "clicked", G_CALLBACK(on_grav_save), &gravtie.stinfo);
    gtk_grid_attach(GTK_GRID(grid), b_savegrav, 10, 1, 1, 1);
    GtkWidget *b_resetgrav = gtk_button_new_with_label("Reset grav");
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
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_otherlm),"If 'other', enter meter name here");
    gtk_grid_attach(GTK_GRID(grid), e_otherlm, 4, 4, 2, 1);
    GtkWidget *b_savelm = gtk_button_new_with_label("Save meter");
    g_signal_connect(b_savelm, "clicked", G_CALLBACK(on_lm_save), &gravtie.lminfo);
    gtk_grid_attach(GTK_GRID(grid), b_savelm, 6, 4, 1, 1);
    GtkWidget *b_resetlm = gtk_button_new_with_label("Reset meter");
    g_signal_connect(b_resetlm, "clicked", G_CALLBACK(on_lm_reset), &gravtie.lminfo);
    gtk_grid_attach(GTK_GRID(grid), b_resetlm, 7, 4, 1, 1);
    GtkWidget *b_lmcalfile = gtk_button_new_with_label("Other cal. file");
    gtk_grid_attach(GTK_GRID(grid), b_lmcalfile, 8, 4, 1, 1);
    g_signal_connect(b_lmcalfile, "clicked", G_CALLBACK(on_lm_filebrowse_clicked), &gravtie.lminfo);
    GtkWidget *cal_label = gtk_label_new("0 calibration lines read");
    gtk_label_set_line_wrap(GTK_LABEL(cal_label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(cal_label), 0.0);
    gtk_grid_attach(GTK_GRID(grid), cal_label, 9, 4, 2, 1);
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
    GtkWidget *b_persave = gtk_button_new_with_label("Save pers.");
    gtk_grid_attach(GTK_GRID(grid), b_persave, 4, 2, 1, 1);
    g_signal_connect(b_persave, "clicked", G_CALLBACK(on_pers_save), &gravtie.prinfo);
    GtkWidget *b_perreset = gtk_button_new_with_label("Reset pers.");
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
    GtkWidget *b_h1 = gtk_button_new_with_label("save h1");
    GtkWidget *b_h2 = gtk_button_new_with_label("save h2");
    GtkWidget *b_h3 = gtk_button_new_with_label("save h3");
    gtk_grid_attach(GTK_GRID(grid), b_h1, 6, 5, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_h2, 6, 6, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_h3, 6, 7, 1, 1);
    GtkWidget *b_rh1 = gtk_button_new_with_label("reset h1");
    GtkWidget *b_rh2 = gtk_button_new_with_label("reset h2");
    GtkWidget *b_rh3 = gtk_button_new_with_label("reset h3");
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
    GtkWidget *b_a1 = gtk_button_new_with_label("save a1");
    GtkWidget *b_a2 = gtk_button_new_with_label("save a2");
    GtkWidget *b_a3 = gtk_button_new_with_label("save a3");
    GtkWidget *b_b1 = gtk_button_new_with_label("save b1");
    GtkWidget *b_b2 = gtk_button_new_with_label("save b2");
    GtkWidget *b_b3 = gtk_button_new_with_label("save b3");
    GtkWidget *b_c1 = gtk_button_new_with_label("save c1");
    GtkWidget *b_c2 = gtk_button_new_with_label("save c2");
    GtkWidget *b_c3 = gtk_button_new_with_label("save c3");
    gtk_grid_attach(GTK_GRID(grid), b_a1, 2, 5, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_a2, 2, 6, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_a3, 2, 7, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_b1, 2, 8, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_b2, 2, 9, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_b3, 2, 10, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_c1, 2, 11, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_c2, 2, 12, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), b_c3, 2, 13, 1, 1);
    GtkWidget *b_ra1 = gtk_button_new_with_label("reset a1");
    GtkWidget *b_ra2 = gtk_button_new_with_label("reset a2");
    GtkWidget *b_ra3 = gtk_button_new_with_label("reset a3");
    GtkWidget *b_rb1 = gtk_button_new_with_label("reset b1");
    GtkWidget *b_rb2 = gtk_button_new_with_label("reset b2");
    GtkWidget *b_rb3 = gtk_button_new_with_label("reset b3");
    GtkWidget *b_rc1 = gtk_button_new_with_label("reset c1");
    GtkWidget *b_rc2 = gtk_button_new_with_label("reset c2");
    GtkWidget *b_rc3 = gtk_button_new_with_label("reset c3");
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

    //std::cout << "val1: " << gravtie.h1.h1 << std::endl;
    //std::cout << "Timestamp: " << std::ctime(&gravtie.h1.t1) << std::endl;
    //std::cout << gravtie.shinfo.gravgrav[0]  << std::endl;

    return 0;
}

