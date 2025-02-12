# Detect the operating system
ifeq ($(OS),Windows_NT)
	# Native Windows settings
	CC = gcc
	CFLAGS = -Wall -Wextra -O2 -static -lm -lz
	RM = del /Q
	EXE_EXT = .exe
else
	# Unix settings
	CC = gcc
	CFLAGS = -Wall -Wextra -O2 -L/usr/lib -lm -lz
	RM = rm -f
	EXE_EXT =
endif

# Cross-compiler settings for Windows builds (Forcing static zlib)
WINDOWS_CC = x86_64-w64-mingw32-gcc
WINDOWS_CFLAGS = -Wall -Wextra -O2 -L/usr/x86_64-w64-mingw32/lib -Wl,-Bstatic -lz -Wl,-Bdynamic -static-libgcc -static-libstdc++
WINDOWS_TARGET = vita_apk_check.exe

# Source files
SRCS = unzip.c ioapi.c main.c

# Object files
OBJS = $(SRCS:.c=.o)

# Executable name
TARGET = vita_apk_check$(EXE_EXT)

# Default target
all: $(TARGET)

# Build specifically for Windows using cross-compiler with forced static zlib
windows: $(SRCS)
	$(WINDOWS_CC) $(SRCS) $(WINDOWS_CFLAGS) -o $(WINDOWS_TARGET)

# Link object files to create the executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(CFLAGS) -o $@

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build files
clean:
	$(RM) $(OBJS) $(TARGET) $(WINDOWS_TARGET)

.PHONY: all clean windows
