CC_FLAGS = -Wall -Wextra -Wpedantic
TEST_SRCS = test.c loop.c hello.c write.c
TEST_BINS = $(TEST_SRCS:.c=)

ydb: main.c $(TEST_BINS)
	gcc $(CC_FLAGS) -g -O3 main.c -o ydb

# Pattern rule — compiles each test binary from its own .c file
$(TEST_BINS): %: %.c
	gcc $(CC_FLAGS) -g -static $< -o $@ 

