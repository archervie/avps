CC = gcc
CFLAGS = -Wall -Wextra -Werror -O2 -pthread
TARGET = avps

SRCS = main.c scan.c
OBJS = $(SRCS:.c=.o)
HEADERS = scan.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(TARGET)

.PHONY: all clean
