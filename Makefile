# Project and compiler information
NAME := zps
CFLAGS := -s -O3 -Wall -Wextra -pedantic
ifeq ($(CC),)
    CC := gcc
endif
all: clean build
# Build the project
build:
	mkdir -p build
	$(CC) $(CFLAGS) src/$(NAME).c -o build/$(NAME)
	cp -prf .application/$(NAME).desktop build/$(NAME).desktop
# Make the installation
install:
	# Create directories if doesn't exist
	mkdir -p $(TARGET)/usr/bin
	mkdir -p $(TARGET)/usr/share/applications
	# Install
	install build/$(NAME) $(TARGET)/usr/bin/$(NAME)
	install build/$(NAME).desktop $(TARGET)/usr/share/applications/$(NAME).desktop
# Clean
clean:
	rm -rf build

.PHONY: all build install clean
