# Makefile for COMP 117 End-to-End assignment
#   NEW PROGRAMS: fileclient and fileserver
#
#
# Useful sample program targets:
#
#  Maintenance targets:
#
#    Make sure these clean up and build your code too
#
#    clean       - clean out all compiled object and executable files
#    all         - (default target) make sure everything's compiled
#

# Do all C++ compies with g++
CPP = g++
CPPFLAGS = -g -Wall -Werror -I$(C150LIB)

# Where the COMP 150 shared utilities live, including c150ids.a and userports.csv
# Note that environment variable COMP117 must be set for this to work!

C150LIB = $(COMP117)/files/c150Utils/
C150AR = $(C150LIB)c150ids.a

LDFLAGS = 
INCLUDES = $(C150LIB)c150dgmsocket.h $(C150LIB)c150nastydgmsocket.h $(C150LIB)c150network.h $(C150LIB)c150exceptions.h $(C150LIB)c150debug.h $(C150LIB)c150utility.h

all: filehelper.o fileclient fileserver

#X.o: X.cpp X.h
#    g++ -c -o X.o X.cpp  # or $(CXX) $(CXXFLAGS) -c -o ...

# filehelper.o :filehelper.cpp filehelper.h $(C150AR) $(INCLUDES)
# 	$(CPP) -o filehelper.o  $(C150AR) -lssl -lcrypto

#
# To get any .o, compile the corresponding .cpp
# #
# %.o:%.cpp   $(C150AR) $(INCLUDES)
# 	$(CPP) -c $(C150AR) $(CPPFLAGS)  $< 

filehelper.o: $(C150AR)  $(INCLUDES)
	$(CPP) $(CPPFLAGS) -c filehelper.cpp $(C150AR)  -lssl -lcrypto

# filehelper.o: filehelper.cpp filehelper.h   $(C150AR)  $(INCLUDES)
# 	$(CPP) -c filehelper.o filehelper.cpp  filehelper.h $(C150AR)  -lssl -lcrypto

# %.o: %.cpp  $(C150AR)  $(INCLUDES)
# 	$(CPP) -c $< -o $@  $(C150AR)  -lssl -lcrypto

fileclient:fileclient.o  $(C150AR) $(INCLUDES)
	$(CPP) -o fileclient fileclient.o filehelper.o $(C150AR) -lssl -lcrypto 

fileserver: fileserver.o  $(C150AR) $(INCLUDES)
	$(CPP) -o fileserver fileserver.o filehelper.o $(C150AR) -lssl -lcrypto



#
# Delete all compiled code in preparation
# for forcing complete rebuild#

clean:
	 rm -f fileclient fileserver *.o 


