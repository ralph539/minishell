# Compiler to use
CC = gcc

# Compilation flags
CFLAGS = -Wall -pedantic -O3

# Linker flags
LDFLAGS =

# Executables to build
EXE = minishell_fr minishell_eng

# Test binary
TEST_FILE = test_readcmd

# Object files (excluding test file)
OBJECTS = $(patsubst %c,%o,$(filter-out test_readcmd.c, $(wildcard *.c)))

all: $(EXE)

minishell_fr: minishell_fr.c readcmd.c readcmd.h
	$(CC) $(CFLAGS) -o minishell_fr minishell_fr.c readcmd.c

minishell_eng: minishell_eng.c readcmd.c readcmd.h
	$(CC) $(CFLAGS) -o minishell_eng minishell_eng.c readcmd.c

test: $(TEST_FILE)

$(TEST_FILE): test_readcmd.o readcmd.o
	$(CC) $(CFLAGS) -o $@ $^

clean:
	\rm -f *.o *~
	\rm -f $(EXE)
	\rm -f $(TEST_FILE)

archive: clean
	(cd .. ; tar cvf minishell-`whoami`.tar minishell)

help:
	@echo "Makefile for minishell."
	@echo "Targets:"
	@echo " all             Build all minishell versions"
	@echo " test            Build the test executable"
	@echo " archive         Archive the minishell directory"
	@echo " clean           Remove all build artifacts"

