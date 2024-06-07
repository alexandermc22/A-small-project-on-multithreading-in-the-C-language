CC = gcc
CFLAGS = -std=gnu99 -Wall -Wextra -Werror -pedantic

SRC = main.c
OBJ = $(SRC:.c=.o)
TARGET = proj2

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c main.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)
