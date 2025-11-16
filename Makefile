CXX = g++
CC = gcc
CXXFLAGS = -std=c++17 -Wall -Wextra -g -I./src -I./src/libs/linenoise
CXXFLAGS += -DDEBUG -Wno-unused-parameter
CFLAGS = -Wall -Wextra -g -I./src/libs/linenoise

LINENOISE_SRC = src/libs/linenoise/linenoise.c
LINENOISE_OBJ = $(LINENOISE_SRC:.c=.o)

TARGET = z80-lisp
SOURCES = src/main.cpp src/command-line.cpp src/crc32.cpp \
	src/reader.cpp src/object.cpp \
	src/source_info.cpp src/interpreter.cpp
OBJS = $(SOURCES:.cpp=.o)

$(TARGET): $(OBJS) $(LINENOISE_OBJ)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LINENOISE_OBJ)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(LINENOISE_OBJ): $(LINENOISE_SRC)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS) $(LINENOISE_OBJ)

test: $(TARGET)
	./$(TARGET) test/test.lisp

.PHONY: clean test