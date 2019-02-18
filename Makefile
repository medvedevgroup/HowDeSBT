CXXFLAGS =  -Wall -std=c++11 -I$${HOME}/include
LDFLAGS  =  -L$${HOME}/lib -lroaring -lsdsl -ljellyfish-2.0 -lpthread

CPP_FILES := howdesbt.cc \
             cmd_make_bf.cc cmd_cluster.cc cmd_build_sbt.cc cmd_query.cc \
             cmd_version.cc \
             query.cc \
             bloom_tree.cc bloom_filter.cc bit_vector.cc file_manager.cc \
             bit_utilities.cc utilities.cc support.cc
OBJ_FILES := $(addprefix ./,$(notdir $(CPP_FILES:.cc=.o)))

all:   CXXFLAGS += -DNDEBUG -O3 

all:   clean howdesbt

howdesbt: $(OBJ_FILES)
	$(CXX) -o $@ $^ $(LDFLAGS)

%.o: %.cc
	$(CXX) -c -o $@ $^ $(CXXFLAGS)

clean: cleano
	rm -f howdesbt

cleano: 
	rm -f *.o
