CC := gcc
CFLAGS := -Wall -Wextra -std=c11 -fno-omit-frame-pointer
LDFLAGS := -pthread

TARGETS := userfaultfd-demo userfaultfd-kernelfault userfaultfd-multithread
SRCS := userfaultfd-demo.c userfaultfd-kernelfault.c userfaultfd-multithread.cma

.PHONY: all clean

all: $(TARGETS)

userfaultfd-demo: userfaultfd-demo.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

userfaultfd-kernelfault: userfaultfd-kernelfault.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

userfaultfd-multithread: userfaultfd-multithread.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)
