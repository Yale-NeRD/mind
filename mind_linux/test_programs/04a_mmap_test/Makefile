APP_NAME := test_protocol
BIN_DIR := bin
CXX := g++
CXXFLAGS := -D __TEST__
INCLUDE := ../../include/disagg/

obj-y += allocator.cpp

build: $(obj-y)
	@mkdir -p $(BIN_DIR)
	$(CXX) -I$(INCLUDE) -o $(BIN_DIR)/$(APP_NAME) $^ $(CXXFLAGS)

run: build
	$(BIN_DIR)/$(APP_NAME) $(INIT_DIR) $(RUN_DIR)

clean:
	@rm -rf $(BIN_DIR)
