# Compiler and flags (-MMD -MP emit header deps so .o files rebuild
# when an included .h changes, not only when the .cpp does)
CXX = g++
# -pthread is required on Linux (std::async, httplib); harmless on macOS.
CXXFLAGS = -Wall -std=c++17 -O2 -pthread -I./src -MMD -MP

# Executable name
TARGET = poker_solver

# Source files (wasm_api.cpp is emscripten-only; see build-wasm.sh)
SRCS = $(filter-out src/wasm_api.cpp,$(wildcard src/*.cpp))

# Object files (replace .cpp with .o)
OBJS = $(SRCS:.cpp=.o)

# Default target
all: $(TARGET)

# Link the object files to create executable
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -pthread -o $(TARGET)

# Compile .cpp to .o
src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Pull in the generated header dependency files
-include $(OBJS:.o=.d)

# Clean up build files
clean:
	rm -f $(TARGET) src/*.o src/*.d