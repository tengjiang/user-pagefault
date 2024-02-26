# `userfaultfd()`

## Tracing

TODO: Automate this process, generate clear tracing of syscalls incurred and latency.

Use strace to see what system calls are invoked when we call userfaultfd(). `ioctl()` observed. `-T` flag is to print the exec time for each syscalls, `-o` with `-ff` prints out output of different threads into seperate files.

```sudo strace -T -o example_trace.txt -ff ./userfaultfd-demo 3  >> example_out.txt```

One thread will be the main thread, and the other one created for processing the userfaultfd events.

```ps -A -o pid,cmd|grep "userfaultfd"```
