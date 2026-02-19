CXX := g++
# CXXFLAGS := -Wall -Wextra -std=c++17
CXXFLAGS := -std=c++17 -D__linux__ -DLORAWAN_SX1262

#make DEBUG=1	
ifeq ($(DEBUG),1)
    CXXFLAGS += -g -O0 -Wall -Wextra
else
    CXXFLAGS += -O2
endif

INCLUDE := include Ducks utils wifi radio routing config /usr/local/include/RadioLib /usr/local/include/RadioLib/hal/
INCLUDE += /usr/local/include 
CPPFLAGS := $(addprefix -I,$(INCLUDE))

TARGET := MamaDuck

# Libraries

CPPFLAGS += -I/usr/local/include
LDFLAGS  += -L/usr/local/lib
LDLIBS   += -lRadioLib -llgpio -lpthread

# ---- Exclusions ----
EXCLUDE_DIRS := include/boards include/rLib
EXCLUDE_FILES := DuckWifi.cpp lilygo_t_beam_sx1262.cpp

# Convert exclusions into find arguments
EXCLUDE_DIR_ARGS := $(foreach dir,$(EXCLUDE_DIRS),! -path "*/$(dir)/*")
EXCLUDE_FILE_ARGS := $(foreach file,$(EXCLUDE_FILES),! -name "$(file)")

# Original recursive source search, now with exclusions
SRC := $(shell find . -name "*.cpp" \
	$(EXCLUDE_DIR_ARGS) \
	$(EXCLUDE_FILE_ARGS))

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(TARGET)
