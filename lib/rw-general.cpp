#include <vector>
#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include "time-functions.h"
#include "tie_structs.h"

////////////////////////////////////////////////////////////////////////
// functions for reading files
////////////////////////////////////////////////////////////////////////

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

