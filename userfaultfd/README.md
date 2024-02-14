# `userfaultfd()`

## Tracing

TODO: Automate this process, generate clear tracing of syscalls incurred and latency.

Use strace to see what system calls are invoked when we call userfaultfd(). `ioctl()` observed.

```strace -p <pid>```

To find the pid, use. One thread will be the main thread, and the other one created for processing the userfaultfd events.

```ps -A -o pid,cmd|grep "userfaultfd"```
