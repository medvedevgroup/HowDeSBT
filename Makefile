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

query_fp_rate_compiled: scripts/query_fp_rate_compiled.so

scripts/query_fp_rate_compiled.so:
	cd scripts ; python query_fp_rate_compiled.setup.py build_ext --inplace

%.o: %.cc
	$(CXX) -c -o $@ $^ $(CXXFLAGS)

clean: cleano
	rm -f howdesbt
	rm -f scripts/query_fp_rate_compiled.so

cleano: 
	rm -f *.o
	rm -f  scripts/query_fp_rate_compiled.c
	rm -rf scripts/build
	rm -f  scripts/*.pyc
