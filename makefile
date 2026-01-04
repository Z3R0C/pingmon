# pingmon Makefile
# Compilation options for pingmon

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
LDFLAGS = -lm
TARGET = pingmon
SOURCES = pingmon.c
OBJECTS = $(SOURCES:.c=.o)

# Default target
all: $(TARGET)

# Main compilation
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

# Compile object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Install to /usr/local/bin
install: $(TARGET)
	cp $(TARGET) /usr/local/bin/$(TARGET)
	chmod 755 /usr/local/bin/$(TARGET)

# Uninstall
uninstall:
	rm -f /usr/local/bin/$(TARGET)

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: clean $(TARGET)

# Static analysis
check:
	cppcheck --enable=all --suppress=missingIncludeSystem $(SOURCES)

# Memory check
memcheck: debug
	valgrind --leak-check=full ./$(TARGET)

# Clean build files
clean:
	rm -f $(OBJECTS) $(TARGET)

# Run tests
test: $(TARGET)
	@echo "Testing with default parameters..."
	timeout 5 ./$(TARGET) || true
	@echo "Testing with custom parameters..."
	timeout 5 ./$(TARGET) 50 100 1.1.1.1 || true

# Show help
help:
	@echo "pingmon Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all       - Build pingmon (default)"
	@echo "  debug     - Build with debug symbols"
	@echo "  install   - Install to /usr/local/bin"
	@echo "  uninstall - Remove from /usr/local/bin"
	@echo "  check     - Static code analysis"
	@echo "  memcheck  - Run valgrind memory check"
	@echo "  test      - Run quick tests"
	@echo "  clean     - Remove build files"
	@echo "  help      - Show this help"

.PHONY: all install uninstall debug check memcheck clean test help
