CC = gcc
CFLAGS = -Wall -Wextra -std=c11
SRCDIR = backend
OBJDIR = obj
SRCS = $(SRCDIR)/task-selector.c $(SRCDIR)/builder.c $(SRCDIR)/utils.c
TARGET = $(OBJDIR)/task-wheel

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

clean:
	rm -f $(TARGET)

run: all
	cd $(OBJDIR) && ./task-wheel
