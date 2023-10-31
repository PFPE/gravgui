# makefile for gravgui

# compilers and flags
CC = gcc
CCFLAGS = -I.

CXX = g++
CXXFLAGS = -O3 -I. -std=c++11
xtraflags = `pkg-config --cflags --libs gtk+-3.0`

# path things
LIB = lib

# mains
all: gravgui

filters = filt.o window_functions.o
others = time-functions.o rw-general.o rw-ties.o cb_filebrowse.o cb_change.o cb_save.o cb_reset.o actual_computations.o

gravgui: $(filters) $(others) gravgui.cpp
	$(CXX) $(CXXFLAGS) $(filters) $(others) gravgui.cpp -lm $(xtraflags) -o gravgui

# objects
filt.o: $(LIB)/filt.cpp
	$(CXX) $(CXXFLAGS) -c $(LIB)/filt.cpp

window_functions.o: $(LIB)/window_functions.c
	$(CC) $(CCFLAGS) -c $(LIB)/window_functions.c

time-functions.o: $(LIB)/time-functions.cpp
	$(CXX) $(CXXFLAGS) -c $(LIB)/time-functions.cpp

rw-general.o: $(LIB)/rw-general.cpp
	$(CXX) $(CXXFLAGS) -lm $(xtraflags) -c $(LIB)/rw-general.cpp

rw-ties.o: $(LIB)/rw-ties.cpp
	$(CXX) $(CXXFLAGS) -lm $(xtraflags) -c $(LIB)/rw-ties.cpp

cb_filebrowse.o: $(LIB)/cb_filebrowse.cpp
	$(CXX) $(CXXFLAGS) -lm $(xtraflags) -c $(LIB)/cb_filebrowse.cpp

cb_change.o: $(LIB)/cb_change.cpp
	$(CXX) $(CXXFLAGS) -lm $(xtraflags) -c $(LIB)/cb_change.cpp

cb_save.o: $(LIB)/cb_save.cpp
	$(CXX) $(CXXFLAGS) -lm $(xtraflags) -c $(LIB)/cb_save.cpp

cb_reset.o: $(LIB)/cb_reset.cpp
	$(CXX) $(CXXFLAGS) -lm $(xtraflags) -c $(LIB)/cb_reset.cpp

actual_computations.o: $(LIB)/actual_computations.cpp
	$(CXX) $(CXXFLAGS) -lm $(xtraflags) -c $(LIB)/actual_computations.cpp

# clean

neat :
	rm -f *.o

clean :
	rm -f *.o
	rm -f gravgui

