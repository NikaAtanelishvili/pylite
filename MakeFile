CC     = gcc
CFLAGS = -std=c11 -Wall -Wextra -g
SRCS   = src/main.c src/lexer.c src/parser.c src/eval.c src/env.c
TARGET = pylite

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ -lm

clean:
	rm -f $(TARGET)

test: $(TARGET)
	@for f in tests/programs/*.pyl; do \
		echo "--- $$f ---"; ./$(TARGET) $$f; \
	done