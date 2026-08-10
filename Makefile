# Makefile

NAME = naevi

ROOT := $(dir $(lastword $(MAKEFILE_LIST)))

SOURCE := $(ROOT)source
BUILD := $(ROOT)build

LOG := $(ROOT)debug.log

BOLD := \033[1m

RED := \033[1;31m
GREEN := \033[1;32m
CYAN := \033[1;36m

DIM := \033[2m

RESET := \033[0m

CFLAGS := -I$(SOURCE) -std=c89 -funsigned-char -Weverything -Wno-c++-keyword -Wno-c23-compat -Wno-comment -Wno-unsafe-buffer-usage -Wno-long-long -Werror -O3
LFLAGS :=

.PHONY: all clean build run compile_commands.json

all: build

clean:
	@printf "\n$(BOLD)Cleaning..$(RESET)\n"

	@printf "    $(CYAN)→$(RESET) Removing build directory..\n"
	@rm -rf "$(BUILD)"

	@printf "    $(GREEN)✓$(RESET) Removed $(BUILD)\n"

	@printf "\n    $(CYAN)→$(RESET) Removing logs..\n"
	@rm -f "$(LOG)"

	@printf "    $(GREEN)✓$(RESET) Removed $(LOG)\n"

	@printf "\n$(GREEN)$(BOLD)Done cleaning.$(RESET)\n"

build:
	@printf "\n$(BOLD)Building..$(RESET)\n"

	@mkdir -p "$(BUILD)/built"
	@printf "    $(GREEN)✓$(RESET) Created directories.\n"

	@printf "\n    $(CYAN)→$(RESET) Compiling..\n\n"
	clang $(CFLAGS) "$(SOURCE)/main.c" -o "$(BUILD)/$(NAME)" --save-temps=obj -fuse-ld=lld $(LFLAGS)
	@printf "\n    $(GREEN)✓$(RESET) Compiled.\n"

	@llvm-strip --strip-all "$(BUILD)/$(NAME)"
	@printf "\n    $(GREEN)✓$(RESET) Stripped.\n"

	@cp "$(BUILD)/$(NAME)" "$(BUILD)/built/$(NAME)"
	@printf "    $(GREEN)✓$(RESET) Staged.\n"

	@printf "\n$(GREEN)$(BOLD)Finished building.$(RESET)\n"

run: all
	@printf "\n$(BOLD)Launching..$(RESET)\n"
	@printf "    $(CYAN)→$(RESET) Running..\n"

	@mkdir -p "$(BUILD)/built"
	@"$(BUILD)/built/$(NAME)" 2>&1 | tee "$(LOG)"

	@printf "\n    $(GREEN)✓$(RESET) Exited.\n"
	@printf "\n$(GREEN)$(BOLD)Exited.$(RESET) Debug log: $(DIM)$(LOG)$(RESET)\n"

compile_commands.json:
	@printf '[\n  {\n    "directory": "%s",\n    "file": "%s",\n    "command": "clang %s"\n  }\n]\n' "$(patsubst %/,%,$(ROOT))" "$(SOURCE)/main.c" "$(CFLAGS)" > $@
