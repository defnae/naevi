# Makefile

NAME := naevi

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

CFLAGS ?= -I$(SOURCE) -std=iso9899:199409 -funsigned-char -fomit-frame-pointer -O3 -Weverything -Wno-gcc-compat -Wno-implicit-int -Wno-deprecated-non-prototype -Wno-reserved-identifier -Wno-disabled-macro-expansion -Wno-comment -Wno-unsafe-buffer-usage -Wno-long-long -Werror
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

	LTO="$$(clang -flto $(CFLAGS) -Wno-Weverything "$(SOURCE)/$(NAME)/main.c" -o /dev/null -ferror-limit=1 -fuse-ld=lld $(LFLAGS) 2>/dev/null >/dev/null && echo -flto)"
	STATIC="$$(clang -static $(CFLAGS) -Wno-Weverything "$(SOURCE)/$(NAME)/main.c" -o /dev/null -ferror-limit=1 -fuse-ld=lld $(LFLAGS) 2>/dev/null >/dev/null && echo -static)"

	CFLAGS="$(CFLAGS) $(WFLAGS) $$LTO"
	LFLAGS="$(LFLAGS) $$STATIC"

	set -x; set -x; clang $$CFLAGS "$(SOURCE)/$(NAME)/main.c" -o "$(BUILD)/$(NAME)/$(NAME)" -ferror-limit=0 -fuse-line-directives --save-temps=obj -fuse-ld=lld -gdwarf-4 $$LFLAGS || { \
	    code=$$?; \
	    printf "\n$(RED)✘$(RESET) $(BOLD)Compilation Failed. Exit $$code$(RESET)\n\n"; \
	    exit $$code; \
	}

	set +x

	@printf "\n    $(GREEN)✓$(RESET) Compiled.\n"

	@cp "$(BUILD)/$(NAME)/$(NAME)" "$(BUILD)/$(NAME)/$(NAME)-debug" || true
	@mv "$(BUILD)/$(NAME)/$(NAME)-debug.exe" "$(BUILD)/$(NAME)/$(NAME)-debug" 2>/dev/null || true

	@llvm-strip --strip-all "$(BUILD)/$(NAME)/$(NAME)" || { \
	    code=$$?; \
	    printf "\n$(RED)✘$(RESET) $(BOLD)Stripping Failed. Exit $$code$(RESET)\n\n"; \
	    exit $$code; \
	}

	@printf "\n    $(GREEN)✓$(RESET) Stripped.\n"

	@cp "$(BUILD)/$(NAME)/$(NAME)" "$(BUILD)/built/$(NAME)" || { \
	    code=$$?; \
	    printf "\n$(RED)✘$(RESET) $(BOLD)Staging Failed. Exit $$code$(RESET)\n\n"; \
	    exit $$code; \
	}

	@printf "\n    $(GREEN)✓$(RESET) Staged binary.\n"

	@printf "\n$(GREEN)$(BOLD)Finished building.$(RESET)\n"

run: all
	@printf "\n$(BOLD)Launching..$(RESET)\n"
	@printf "    $(CYAN)→$(RESET) Running..\n\n"

	file=$$(mktemp); \
	(set -x; set -x; "$(BUILD)/built/$(NAME)"; code=$$?; set +x; printf '%s\n' "$$code" > "$$file") 2>&1 | tee "$(LOG)"; \
	code=$$(cat "$$file"); \
	rm -f "$$file"; \
	if [ "$$code" -ne 0 ]; then \
	    printf "\n    $(RED)✘$(RESET) Exited with error $$code.\n"; \
	else \
	    printf "\n    $(GREEN)✓$(RESET) Exited cleanly.\n"; \
	fi

	@printf "\n$(GREEN)$(BOLD)Exited.$(RESET) Debug log: $(DIM)$(LOG)$(RESET)\n"
