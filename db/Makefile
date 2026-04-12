CC_FLAGS = -Wall -Wextra -Wpedantic

ydb: tests main.c
	gcc $(CC_FLAGS) -g -O3 main.c -o ydb

tests: test.c
	gcc $(CC_FLAGS) -static test.c -o test