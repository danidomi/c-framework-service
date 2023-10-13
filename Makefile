# Define output and bin directories
OUTPUT_DIR := output
BIN_DIR := bin

# List of source files
SRCS := Server.c request/Request.c response/Response.c logger/Logger.c error/Error.c

# List of object files
OBJS := $(SRCS:%.c=$(OUTPUT_DIR)/%.o)

# Default target
all: $(BIN_DIR)/Server

# Ensure output and bin directories are created
$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Link the executable
$(BIN_DIR)/Server: $(OUTPUT_DIR) $(BIN_DIR) $(OBJS)
	gcc -o $@ $(OBJS) -lmysqlclient

# Compile source files
$(OUTPUT_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	gcc -c $< -o $@

# Clean rule
clean:
	rm -rf $(OUTPUT_DIR) $(BIN_DIR)