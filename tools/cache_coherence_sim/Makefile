# Makefile
IDIR = .
CXX=g++
CXXFLAGS=-I$(IDIR) -O2 -lpthread -g
LIBS=
PROGRAM_NAME=SIMULATOR

SRC := $(wildcard *.cpp)
OBJ := $(patsubst %.cpp,%.o,$(SRC))
DEP := $(patsubst %.cpp,%.d,$(SRC))
# simulator.o dyn_cache_block.o main.o
all: build

clean:
	@rm $(OBJ) $(DEP)
	@rm -f $(PROGRAM_NAME)

build: $(OBJ)
	@$(CXX) -o ./$(PROGRAM_NAME) $^ $(CXXFLAGS) $(LIBS)

-include $(DEP)
%.o: %.cpp Makefile
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@
