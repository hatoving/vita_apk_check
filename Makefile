# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -O2 -L/usr/lib -lm -lz

# Source files
SRCS = unzip.c ioapi.c main.c

# Object files
OBJS = $(SRCS:.c=.o)

# Executable name
TARGET = vita_apk_check

# Default target
all: $(TARGET)

# Link object files to create the executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(CFLAGS) -o $@

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
