.PHONY: release

# Define output and bin directories
SRC_DIR := .
OUTPUT_DIR := output
BIN_DIR := bin
RELEASE_DIR := release/c-framework-service

# List of source files
SRCS := Server.c request/Request.c response/Response.c logger/Logger.c error/Error.c
HDRS := $(wildcard *.h) $(wildcard request/*.h) $(wildcard response/*.h) $(wildcard logger/*.h) $(wildcard error/*.h)

# Create object files for each source
OBJS := $(patsubst %.c,$(OUTPUT_DIR)/%.o,$(SRCS))

# Default target
all: $(BIN_DIR)/Server

# Ensure output and bin directories are created
$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Link the executable
$(BIN_DIR)/Server: $(OUTPUT_DIR) $(BIN_DIR) $(OBJS)
	gcc -o $@ $(OBJS)

# Compile source files into object files
$(OUTPUT_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	gcc -c $< -o $@

# Clean rule
clean:
	rm -rf $(OUTPUT_DIR) $(BIN_DIR) $(RELEASE_DIR)

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