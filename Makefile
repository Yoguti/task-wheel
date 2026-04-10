CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I.
LDFLAGS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

SRCDIR_BACK = backend
SRCDIR_FRONT = frontend
OBJDIR = obj

# Lista de todos os arquivos .c necessários
SRCS = $(SRCDIR_BACK)/task-selector.c \
       $(SRCDIR_BACK)/builder.c \
       $(SRCDIR_BACK)/utils.c \
       $(SRCDIR_FRONT)/interface.c \
       controler.c

# Converte arquivos .c em .o mapeados na pasta obj
OBJS = $(patsubst %.c, $(OBJDIR)/%.o, $(SRCS))

TARGET = $(OBJDIR)/task-wheel

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR)

run: all
	cd $(OBJDIR) && ./task-wheel