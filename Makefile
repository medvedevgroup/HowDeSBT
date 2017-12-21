CXXFLAGS =  -Wall -std=c++11 -I/Users/rsharris/darwin-i386/include
LDFLAGS  =  -L/Users/rsharris/darwin-i386/lib -lroaring -lsdsl -ljellyfish-2.0 -lpthread
#CXXFLAGS += -DuseJellyHash

CPP_FILES := $(wildcard *.cc)
OBJ_FILES := $(addprefix ./,$(notdir $(CPP_FILES:.cc=.o)))

all:   CXXFLAGS += -DNDEBUG -O3 

all:   clean sabutan

sabutan: $(OBJ_FILES)
	$(CXX) -o $@ $^ $(LDFLAGS)

%.o: %.cc
	$(CXX) -c -o $@ $^ $(CXXFLAGS)

clean: cleano
	rm -f sabutan *.o

cleano: 
	rm -f *.o
