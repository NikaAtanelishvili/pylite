# CC = gcc: Define compiler

# CFLAGS = ...: Passes vital configuration flags to the compiler:

# -std=c11: Enforces modern C11 standards.

# -Wall -Wextra: Turns on almost all compiler warnings.

# -g: Binds debugging symbols into the binary. This ensures that if your interpreter 
#     runs into a segmentation fault, tools like gdb or valgrind can tell you the exact file line number where it died.

# SRCS & TARGET: Tracks your source files and sets the final executable's name to pylite.
CC     = gcc
CFLAGS = -std=c11 -Wall -Wextra -g
SRCS   = src/main.c src/lexer.c src/parser.c src/eval.c src/env.c
TARGET = pylite

# build
# $@: Resolves to the name of the target being built. Here, it expands to -o pylite.

# $^: Resolves to the entire list of prerequisites (all your source files).

# -lm: Explicitly links C’s math library (libm).
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ -lm

clean:
	rm -f $(TARGET)

test: $(TARGET)
	@for f in tests/programs/*.pyl; do \
		echo "--- $$f ---"; ./$(TARGET) $$f; \
	done