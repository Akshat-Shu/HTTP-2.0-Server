CXX = g++
CXXFLAGS = -std=c++17 -Wall -Werror -I.
LDFLAGS = -pthread -lssl -lcrypto

ifdef DEBUG
    CXXFLAGS += -g -O0 -DDEBUG
else
    CXXFLAGS += -O2 -DNDEBUG
endif

SRC_DIRS = Client http2/headers http2/protocol http2/protocol/hpack \
           Multithreading Networking/Epoller Networking/Socket \
           Utils WebBinder Response Frame-Handler

EXCLUDE_DIRS = Messages Note

BUILD_DIR = build
BIN_DIR = bin

SRCS = $(shell find $(SRC_DIRS) \( -name "*.cpp" -o -name "*.cc" \))
SRCS := $(SRCS) main.cpp
OBJS = $(patsubst %.cc,$(BUILD_DIR)/%.o,$(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRCS)))

DEPS = $(OBJS:.o=.d)

SERVER_TARGET = $(BIN_DIR)/server

all: $(SERVER_TARGET)

$(SERVER_TARGET): $(OBJS) | $(BIN_DIR)
	$(CXX) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/%.o: %.cc | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

$(BIN_DIR):
	mkdir -p $@

-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: debug
debug:
	@echo "This is a debug message"
	@echo "SRCS = $(SRCS)"