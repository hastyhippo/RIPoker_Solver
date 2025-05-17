# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -std=c++17 -I./src

# Executable name
TARGET = poker_solver

# Source files
SRCS = $(wildcard src/*.cpp)

# Object files (replace .cpp with .o)
OBJS = $(SRCS:.cpp=.o)

# Default target
all: $(TARGET)

# Link the object files to create executable
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET)

# Compile .cpp to .o
src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(TARGET) src/*.o