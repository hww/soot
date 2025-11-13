CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -I./src
TARGET = z80-lisp
SOURCES = src/main.cpp src/tokenizer.cpp src/ast.cpp src/compiler.cpp \
	 src/source-info.cpp src/crc32.cpp

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET)

test: $(TARGET)
	./$(TARGET) test/test.lisp

.PHONY: clean test