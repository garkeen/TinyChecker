CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2

# 源文件
SRCS = term.c core_ops.c context.c runtime.c reducer.c typechecker.c lexer.c surface.c parser.c restricted.c elaborator.c pipeline.c pretty.c cdecl.c
OBJS = $(SRCS:.c=.o)

# 测试文件
TEST_DIR = Test
TESTS = $(TEST_DIR)/test_basic $(TEST_DIR)/test_reducer $(TEST_DIR)/test_typechecker $(TEST_DIR)/test_lexer $(TEST_DIR)/test_parser $(TEST_DIR)/test_pipeline

.PHONY: all clean tests

all: $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

tests: $(TESTS)

$(TEST_DIR)/test_basic: $(OBJS) $(TEST_DIR)/test_basic.o
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_DIR)/test_reducer: $(OBJS) $(TEST_DIR)/test_reducer.o
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_DIR)/test_typechecker: $(OBJS) $(TEST_DIR)/test_typechecker.o
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_DIR)/test_lexer: $(OBJS) $(TEST_DIR)/test_lexer.o
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_DIR)/test_parser: $(OBJS) $(TEST_DIR)/test_parser.o
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_DIR)/test_pipeline: $(OBJS) $(TEST_DIR)/test_pipeline.o
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_DIR)/%.o: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS) -I. -c $< -o $@

clean:
	rm -f *.o $(TEST_DIR)/*.o $(TEST_DIR)/*.exe
