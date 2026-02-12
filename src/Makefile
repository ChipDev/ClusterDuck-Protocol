CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17

INCLUDE := include Ducks utils wifi radio routing config
CPPFLAGS := $(addprefix -I,$(INCLUDE))

TARGET = MamaDuck
SRC := $(shell find . -name "*.cpp")


all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(TARGET) $(SRC) 


clean:
	rm -f $(TARGET)



