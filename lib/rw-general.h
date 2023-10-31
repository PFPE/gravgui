#ifndef RW_GENERAL_H
#define RW_GENERAL_H

#include <vector>
#include <string>
#include <map>
#include "tie_structs.h"

////////////////////////////////////////////////////////////////////////
// functions for reading files (database, dgs)
////////////////////////////////////////////////////////////////////////

// read stations.db and save key="VALUE" pairs in a map of maps
std::map<std::string, std::map<std::string, std::string> > readStationFile(const std::string& filePath);

// read landmeters.db and save meter names and cal table paths in a map
std::map<std::string, std::map<std::string, std::string> > readMeterFile(const std::string& filePath);

// read a calibration file for a landmeter
calibration read_lm_calib(const std::string& filePath);

// function for reading a DGS laptop file and returning timestamps and grav values
std::pair<std::vector<float>, std::vector<std::time_t> > read_dat_dgs(const std::vector<std::string>& file_paths, const std::string& ship);

#endif
