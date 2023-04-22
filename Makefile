CFLAGS = -Wall -g -Werror -Wno-error=unused-variable -I ./src/include

SRC_DIR := ./src
BUILD_DIR := ./build

all: $(BUILD_DIR) server subscriber

server: $(BUILD_DIR)/server.o $(BUILD_DIR)/common.o
	$(CXX) $(CPPFLAGS) $(CFLAGS) $^ -o $@

subscriber: $(BUILD_DIR)/subscriber.o $(BUILD_DIR)/common.o
	$(CXX) $(CPPFLAGS) $(CFLAGS) $^ -o $@

$(BUILD_DIR):
	mkdir $(BUILD_DIR)

$(BUILD_DIR)/common.o: $(SRC_DIR)/common.cpp
	$(CXX) $(CPPFLAGS) $(CFLAGS) -c $^ -o $@

$(BUILD_DIR)/server.o: $(SRC_DIR)/server.cpp
	$(CXX) $(CPPFLAGS) $(CFLAGS) -c $^ -o $@

$(BUILD_DIR)/subscriber.o: $(SRC_DIR)/subscriber.cpp
	$(CXX) $(CPPFLAGS) $(CFLAGS) -c $^ -o $@

.PHONY: clean

clean:
	rm -rf server subscriber $(BUILD_DIR)
