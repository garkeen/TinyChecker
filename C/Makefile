CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2

# 检测操作系统
ifeq ($(OS),Windows_NT)
    EXT = .exe
    RM = del /Q
    MKDIR = mkdir
    SEP = \\
else
    EXT =
    RM = rm -f
    MKDIR = mkdir -p
    SEP = /
endif

# 源文件
SRCS = term.c core_ops.c context.c runtime.c reducer.c typechecker.c lexer.c surface.c parser.c restricted.c elaborator.c pipeline.c pretty.c cdecl.c
OBJS = $(SRCS:.c=.o)

# 测试文件
TEST_DIR = Test
TESTS = $(TEST_DIR)/test_basic$(EXT) $(TEST_DIR)/test_reducer$(EXT) $(TEST_DIR)/test_typechecker$(EXT) $(TEST_DIR)/test_lexer$(EXT) $(TEST_DIR)/test_parser$(EXT) $(TEST_DIR)/test_pipeline$(EXT)

.PHONY: all clean tests

all: $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

tests: $(TESTS)

$(TEST_DIR)/test_basic$(EXT): $(OBJS) $(TEST_DIR)/test_basic.o
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_DIR)/test_reducer$(EXT): $(OBJS) $(TEST_DIR)/test_reducer.o
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_DIR)/test_typechecker$(EXT): $(OBJS) $(TEST_DIR)/test_typechecker.o
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_DIR)/test_lexer$(EXT): $(OBJS) $(TEST_DIR)/test_lexer.o
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_DIR)/test_parser$(EXT): $(OBJS) $(TEST_DIR)/test_parser.o
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_DIR)/test_pipeline$(EXT): $(OBJS) $(TEST_DIR)/test_pipeline.o
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_DIR)/%.o: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS) -I. -c $< -o $@

clean:
ifeq ($(OS),Windows_NT)
	-$(RM) *.o $(TEST_DIR)\*.o $(TEST_DIR)\*.exe
else
	$(RM) *.o $(TEST_DIR)/*.o $(TEST_DIR)/*$(EXT)
endif
