CC := gcc
CFLAGS := -Wall -Wextra -std=c11
LDFLAGS := -pthread

TARGET := userfaultfd-demo
SRC := userfaultfd-demo.c
OBJ := $(SRC:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJ)
