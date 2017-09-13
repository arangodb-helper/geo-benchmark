CC=gcc
CXX=g++
RM=rm -f
CPPFLAGS=-g -Wall -Werror -std=c++11
LDFLAGS=-g


.DEFAULT : all

all : benchmark.o mmfiles-geo-index.o
	$(CXX) $(LDFLAGS) -o benchmark mmfiles-geo-index.o benchmark.o $(LDLIBS) 

benchmark.o: benchmark.cpp vptree/vptree.hpp
	$(CXX) $(CPPFLAGS) -c benchmark.cpp

mmfiles-geo-index.o: arango/mmfiles-geo-index.cpp arango/mmfiles-geo-index.h
	$(CXX) $(CPPFLAGS) -c arango/mmfiles-geo-index.cpp

clean:
	$(RM) mmfiles-geo-index.o benchmark.o

distclean: clean
	$(RM) benchmark