#ifndef RW_TIES_H
#define RW_TIES_H

#include <string>
#include "tie_structs.h"

// write a gravtie struct to a TOML-compliant output file at given path
void tie_to_toml(const std::string& filepath, tie* gravtie);

// read a TOML file and put values into a gravtie struct
// note that we will ignore the TOML sections like [SHIP] and go by key=value only
// the section headers are there just to make the files easier for humans to read but
// all the keys should be unique so we can get away with not caring about them here
void toml_to_tie(const std::string& filePath, tie* gravtie);

void write_report(const std::string& filepath, tie* gravtie);

#endif
