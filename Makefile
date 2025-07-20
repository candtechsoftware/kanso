# Makefile wrapper for CMake
BUILD_TYPE ?= Debug
BUILD_DIR = build
BINARY_NAME = kanso

# Windowing option (Linux only)
WINDOWING ?= x11

.PHONY: all clean release run configure lint format help

# Default target - build debug
all: configure
	@cd $(BUILD_DIR) && $(MAKE)

# Configure CMake if needed
configure:
	@if [ ! -f $(BUILD_DIR)/Makefile ] || [ "$(FORCE_RECONFIGURE)" = "1" ]; then \
		echo "Configuring CMake (windowing=$(WINDOWING))..."; \
		mkdir -p $(BUILD_DIR); \
		cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DWINDOWING_SYSTEM=$(WINDOWING) ..; \
	fi

# Release build  
release:
	@BUILD_TYPE=Release FORCE_RECONFIGURE=1 $(MAKE) all

# Run the binary
run: all
	@if [ -f ./$(BUILD_DIR)/$(BINARY_NAME) ]; then \
		./$(BUILD_DIR)/$(BINARY_NAME); \
	elif [ -f ./$(BUILD_DIR)/debug/$(BINARY_NAME) ]; then \
		./$(BUILD_DIR)/debug/$(BINARY_NAME); \
	elif [ -f ./$(BUILD_DIR)/release/$(BINARY_NAME) ]; then \
		./$(BUILD_DIR)/release/$(BINARY_NAME); \
	else \
		echo "Error: Cannot find binary $(BINARY_NAME)"; \
		exit 1; \
	fi

# Clean build directory
clean:
	@echo "Cleaning build directory..."
	@rm -rf $(BUILD_DIR) compile_commands.json kanso.log

# Lint the code
lint:
	@echo "Running linter..."
	@find src -name "*.cpp" -o -name "*.h" | xargs clang-format --dry-run --Werror 2>/dev/null || \
		(echo "Code style issues found. Run 'make format' to fix." && exit 1)

# Format the code
format:
	@echo "Formatting code..."
	@find src -name "*.cpp" -o -name "*.h" | xargs clang-format -i

help:
	@echo "Kanso Build System"
	@echo ""
	@echo "Basic usage:"
	@echo "  make              - Build debug version"
	@echo "  make release      - Build release version"
	@echo "  make run          - Build and run"
	@echo "  make clean        - Remove build files"
	@echo ""
	@echo "Linux windowing option:"
	@echo "  WINDOWING=x11     - Use X11 windowing (default)"
	@echo "  WINDOWING=wayland - Use Wayland windowing"
	@echo ""
	@echo "Examples:"
	@echo "  make"
	@echo "  make run"
	@echo "  WINDOWING=wayland make run"
	@echo ""
	@echo "Other commands:"
	@echo "  make lint         - Check code formatting"
	@echo "  make format       - Auto-format code"
