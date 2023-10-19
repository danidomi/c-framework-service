.PHONY: all clean release

# Define output and bin directories
SRC_DIR := src
OUTPUT_DIR := output
BIN_DIR := bin
RELEASE_DIR := release/c-framework-service
# Determine the OS
UNAME := $(shell uname)

# Set the platform-specific release folder
ifeq ($(UNAME), Linux)
	RELEASE_DIR := $(RELEASE_DIR)/Linux
else ifeq ($(UNAME), Darwin) # macOS
	RELEASE_DIR := $(RELEASE_DIR)/macOS
else ifeq ($(UNAME), Windows_NT)
	RELEASE_DIR := $(RELEASE_DIR)/Windows
endif

# List of source files
SRCS := $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/**/*.c)
HDRS := $(wildcard $(SRC_DIR)/*.h) $(wildcard $(SRC_DIR)/**/*.h)

# Create object files for each source
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OUTPUT_DIR)/%.o,$(SRCS))

CC = cc
CFLAGS = -Wall -O2

# Default target
all: $(BIN_DIR)/server

# Ensure output and bin directories are created
$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Link the executable
$(BIN_DIR)/server: $(OUTPUT_DIR) $(BIN_DIR) $(OBJS) $(OUTPUT_DIR)/main.o
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(OUTPUT_DIR)/main.o -lpthread

# Compile source files into object files
$(OUTPUT_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@ -w

$(OUTPUT_DIR)/main.o: main.c
	$(CC) $(CFLAGS) -c main.c -o $(OUTPUT_DIR)/main.o

# Clean rule
clean:
	rm -rf $(OUTPUT_DIR) $(BIN_DIR) $(RELEASE_DIR)

test:
	echo $(RELEASE_DIR)

# Create a release target
release: $(HDRS)
	rm -rf $(RELEASE_DIR)
	mkdir -p $(RELEASE_DIR)
	ld -r -o $(RELEASE_DIR)/c-framework-service.o $(shell find $(OUTPUT_DIR) -name '*.o')
	find $(SRC_DIR) -type f -name '*.h' | while read -r file; do \
		rel_path=$$(echo "$$file" | sed "s|^$(SRC_DIR)/||"); \
		dest_file="$(RELEASE_DIR)/$$rel_path"; \
		mkdir -p $$(dirname $$dest_file); \
		cp "$$file" "$$dest_file"; \
	done
	(cd release && zip -r c-framework-service.zip *)
