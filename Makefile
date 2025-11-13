CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -I./src 
CXXFLAGS += -DDEBUG -Wno-unused-parameter

TARGET = z80-lisp
SOURCES = src/main.cpp src/command-line.cpp src/crc32.cpp \
	src/reader.cpp src/object.cpp \
	src/source-info.cpp src/interpreter.cpp
	

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET)

test: $(TARGET)
	./$(TARGET) test/test.lisp

.PHONY: clean test