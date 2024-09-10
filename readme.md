# Gravity tie GUI for PFPE

**This repository was archived in September 2024. While the code is probably still functional, it has been superseded by two other GUI options for gravity ties: an electron app coded in javascript, and a MATLAB program. Both of the new GUIs can be downloaded from the PFPE github page.**

This repository contains software for computing gravity ties. The primary way to do this is with a GUI written in C++/GTK, which enables users to enter timestamped measurements of water height at the pier, upload a gravimeter data file, and compute bias relative to known station gravity. Land ties can also be computed given calibration files for the meter being used.

## Installation instructions
### C++ executable
Pre-compiled binaries for Windows, MacOS, and Ubuntu Linux are available on the Releases page for this repository. They will need to be run within a terminal, but fear not, that's easy! Ubuntu and windows programs have been tested (for windows, use MSYS2 UCRT64 terminal and install gtk dependencies with pacman). The mac version has not been tested bc I do not have a mac.

The GUI can also be compiled on your local machine using gcc/g++ provided you have the requisite gtk3 libraries installed. There is a makefile provided that you can use if you have `make` installed (and also `pkg-config`).

If you're starting with a fresh install of ubuntu 22.04, you will need to install libgtk-3-dev at minimum (probably also make, maybe also gcc and g++).

## Usage
Run the compiled program from a terminal.

## Documentation
Feel free to dive into the code comments, for now! A full manual for the GUI is in the works and will be added here once it exists.
The file `example.toml` shows the format of a tie that has been written out from the GUI (just to a file, not to a report). The format is TOML-compliant (v1.0.0). 

## TODO
docs and docs action for build\
releases (with chmod?)\
mac test exec - does it need anything fancier than brew?

### would be nice someday
clean up filter + window function libraries\
somehow the whole thing seems like too many lines of code\
grav plotting? with filtering outside of bias calc?\
metric/imperial switch?\
raw data read (vs laptop)\
manual timestamp entry vs toml editing?\
styling via css and logic for it

# Python command line tie utility
Some Python (3+) scripts for the same gravity tie calculations are also included in this repository in the `py/` directory. The dependencies are `numpy` and `scipy`. `tie_functions.py` contains a custom `tie` class with lots of methods for computing bias etc, and `gravity_tie_cmd.py` runs several of those methods in sequence, with command line prompts for the user to enter pier water heights, counts, etc. Note that neither Python file is complete. The contents could, however, be used as building blocks for anyone who wants to do ties in Python and also these things are easier to read than C++ if you want to look at how the calculations are done. No guarantees no warranties etc etc.
