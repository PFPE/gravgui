#ifndef GRAV_CONSTANTS_H
#define GRAV_CONSTANTS_H

// the current setup does not allow manual timestamping, so to test the bias calc, there is a
// debug option:
const bool debug_dgs = false;
// this will set up a kludge-y thing where the timestamps for entered water heights
// are replaced with values pulled from whatever DGS file is imported
// it assumes that dgs file has at least 4000 values (preferably lots more)

const double faafactor = 0.3086;
const double GravCal = 414125;
const float otherfactor = 8388607;
const float g0 = 10000;

#endif
