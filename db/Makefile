CC_FLAGS = -Wall -Wextra -Wpedantic
SRC = main.c readelf.c
TEST_SRCS = $(wildcard tests/*.c)
TEST_BINS = $(TEST_SRCS:.c=)

ydb: $(SRC) $(TEST_BINS)
	gcc $(CC_FLAGS) -g -O0 $(SRC) -o ydb

# Pattern rule — compiles each test binary from its own .c file
$(TEST_BINS): %: %.c
	gcc $(CC_FLAGS) -g -static $< -o $@ 
	objdump -D $@ > $@.disas