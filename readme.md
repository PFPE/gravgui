# Gravity tie GUI for PFPE
This repository contains software for computing gravity ties. The primary way to do this is with a GUI written in C++/GTK, which enables users to enter timestamped measurements of water height at the pier, upload a meter gravity file, and compute bias relative to known station gravity. Land ties can also be computed given calibration files for the meter being used.

## Installation instructions
### C++ executable
Pre-compiled binaries for Windows, MacOS, and Linux are *not yet* available but they will be, once I figure out github actions.

In the meantime, everything can be compiled on your local machine using gcc/g++ provided you have the requisite gtk3 libraries installed. To do this with cmake, run the following:
`mkdir build`
`cd build`
`cmake ..`
`make`
To do this without cmake, you can run the provided `make_gui` script in your favorite shell.

## Usage
Run the compiled program from a terminal. Instructions for how to set this up with a launcher/icon in the more familiar way will be added if I ever figure out how to do that.

## Documentation
Feel free to dive into the code comments, for now! A full manual for the GUI is in the works and will be added here once it exists.
The file `example.toml` shows the format of a tie that has been written out from the GUI (just to a file, not to a report). The format is TOML-compliant (v1.0.0). 

## TODO
docs
actions build binaries
option for manual timestamp entry?
writing ties to report format (and/or to tex?)

### would be nice someday
modularity/header files
clean up filter + window function libraries
grav plotting? with filtering outside of bias calc?
metric/imperial switch
raw data read (vs laptop)
styling via css and logic for it

# Python command line tie utility
Some Python (3+) scripts for the same gravity tie calculations are also included in this repository. The dependencies are numpy and scipy. `tie_functions.py` contains a custom `tie` class with lots of methods for computing bias etc, and `gravity_tie_cmd.py` runs several of those methods in sequence, with command line prompts for the user to enter pier water heights, counts, etc. Note that neither Python file is complete. The contents could, however, be used as building blocks for anyone who wants to do ties in Python and also these things are easier to read than C++ if you want to look at how the calculations are done. No guarantees no warranties etc etc.
