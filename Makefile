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

gravgui: $(filters) gravgui.cpp
	$(CXX) $(CXXFLAGS) $(filters) gravgui.cpp -lm $(xtraflags) -o gravgui

# objects
filt.o: $(LIB)/filt.cpp
	$(CXX) $(CXXFLAGS) -c $(LIB)/filt.cpp

window_functions.o: $(LIB)/window_functions.c
	$(CC) $(CCFLAGS) -c $(LIB)/window_functions.c

# clean

neat :
	rm -f *.o

clean :
	rm -f *.o
	rm -f gravgui

