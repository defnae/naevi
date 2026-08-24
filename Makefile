# Makefile

NAME ?= naevi

ROOT ?= $(dir $(lastword $(MAKEFILE_LIST)))

SOURCE ?= $(ROOT)source
BUILD ?= $(ROOT)build

LOG ?= $(ROOT)debug.log

BOLD := \033[1m

RED := \033[1;31m
GREEN := \033[1;32m
CYAN := \033[1;36m

DIM := \033[2m

RESET := \033[0m

WFLAGS ?= -Weverything -Wno-gcc-compat -Wno-deprecated-non-prototype -Wno-implicit-int -Wno-comment -Wno-unsafe-buffer-usage -Wno-long-long
CFLAGS ?= -I$(SOURCE) -std=iso9899:199409 -funsigned-char $(WFLAGS) -fomit-frame-pointer -O3
LFLAGS ?=

.ONESHELL:
.PHONY: all clean build run compile_commands.json

all: build

compile_commands.json:
	@printf '[\n  {\n    "directory": "%s",\n    "file": "%s",\n    "command": "clang %s"\n  }\n]\n' "$(patsubst %/,%,$(ROOT))" "$(SOURCE)/$(NAME)/main.c" "$(CFLAGS)" > $@

clean:
	@printf "\n$(BOLD)Cleaning..$(RESET)\n"

	@printf "    $(CYAN)→$(RESET) Removing build directory..\n"
	@rm -rf "$(BUILD)"
	@printf "    $(GREEN)✓$(RESET) Removed $(BUILD)\n"

	@printf "\n    $(CYAN)→$(RESET) Removing logs..\n"
	@rm -f "$(LOG)"
	@printf "    $(GREEN)✓$(RESET) Removed $(LOG)\n"

	@printf "\n$(GREEN)$(BOLD)Done cleaning.$(RESET)\n"

build: clean
	@printf "\n$(BOLD)Building..$(RESET)\n"

	@mkdir -p "$(BUILD)/$(NAME)" "$(BUILD)/built"
	@printf "    $(GREEN)✓$(RESET) Created directories.\n"

	@printf "\n    $(CYAN)→$(RESET) Compiling..\n\n"
	@set -x; set -x; clang $(CFLAGS) "$(SOURCE)/$(NAME)/main.c" -o "$(BUILD)/$(NAME)/$(NAME)" -ferror-limit=0 --save-temps=obj -fuse-ld=lld -gdwarf-4 $(LFLAGS) || { \
		code=$$?; \
		printf "\n$(RED)✘$(RESET) $(BOLD)Compiled Failed. ? $$code$(RESET)\n\n"; \
		exit $$code; \
	}

	@set +x;

	@printf "\n    $(GREEN)✓$(RESET) Compiled.\n"

	@cp "$(BUILD)/$(NAME)/$(NAME)" "$(BUILD)/$(NAME)/$(NAME)-debug" || true
	@mv "$(BUILD)/$(NAME)/$(NAME)-debug.exe" "$(BUILD)/$(NAME)/$(NAME)-debug" 2>/dev/null || true

	@llvm-strip --strip-all "$(BUILD)/$(NAME)/$(NAME)" || { \
		code=$$?; \
		printf "\n$(RED)✘$(RESET) $(BOLD)Stripping Failed. ? $$code$(RESET)\n\n"; \
		exit $$code; \
	}

	@printf "\n    $(GREEN)✓$(RESET) Stripped.\n"

	@cp "$(BUILD)/$(NAME)/$(NAME)" "$(BUILD)/built/$(NAME)"

	@printf "\n$(GREEN)$(BOLD)Finished building.$(RESET)\n"

run: all
	@printf "\n$(BOLD)Launching..$(RESET)\n"
	@printf "    $(CYAN)→$(RESET) Running..\n\n"

	@file=$$(mktemp); \
	(set -x; set -x; "$(BUILD)/built/$(NAME)"; code=$$?; set +x; printf '%s\n' "$$code" > "$$file") 2>&1 | tee "$(LOG)"; \
	code=$$(cat "$$file"); \
	rm -f "$$file"; \
	if [ "$$code" -ne 0 ]; then \
		printf "\n    $(GREEN)✓$(RESET) Exited. $$code.\n"; \
	else \
		printf "\n    $(GREEN)✓$(RESET) Exited.\n"; \
	fi

	@printf "\n$(GREEN)$(BOLD)Exited.$(RESET) Debug log: $(DIM)$(LOG)$(RESET)\n"
