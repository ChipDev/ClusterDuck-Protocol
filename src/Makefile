CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17

INCLUDE := include
CPPFLAGS := -I$(INCLUDE)

TARGET = TestDuck
SRC = TestDuck.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(TARGET) $(SRC) 

clean:
	rm -f $(TARGET)



