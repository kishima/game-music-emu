# Makefile for APU Binary Log Parser
# Standalone C implementation

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
TARGET = apu_bin_parser

# Source files
SRCS = apu_bin_parser.c
OBJS = $(SRCS:.c=.o)

# Default target
all: $(TARGET)

# Build the parser
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Clean build artifacts
clean:
	rm -f $(OBJS) $(TARGET)

# Install (optional)
install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

# Test target (requires a test binary file)
test: $(TARGET)
	@echo "To test, run: ./$(TARGET) apu_log_track0.bin"
	@echo "First generate a test file using the demo program"

.PHONY: all clean install test