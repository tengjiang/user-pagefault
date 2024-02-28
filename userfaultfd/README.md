# `userfaultfd()`

## Tracing

TODO: Automate this process, generate clear tracing of syscalls incurred and latency.

Use strace to see what system calls are invoked when we call userfaultfd(). `ioctl()` observed. `-T` flag is to print the exec time for each syscalls, `-o` with `-ff`xw prints out output of different threads into seperate files.

```
sudo strace -T -o example_trace.txt -ff ./userfaultfd-demo 3  >> example_out.txt
```

One thread will be the main thread, and the other one created for processing the userfaultfd events. The strace results are stored in `results`, with one combined txt file and two seperate txt files (one for fault handling thread, one for faulting thread.

```
echo "UID          PID    PPID     LWP  C NLWP STIME TTY          TIME CMD"
ps -eLf |grep "userfaultfd"
```

To see the process details:
```
ps -e -o cmd,pid,ppid,tid,tgid | grep "userfaultfd"
```
Alternatively,
```
ps -eLf |grep "userfaultfd"
```

## Flame Graph and Profiling
### Install
```
git clone git@github.com:brendangregg/FlameGraph.git
git clone git@github.com:iovisor/bcc.git
```
Also install perf.

### Latency
```
sudo ./userfaultfd-kernelfault <page_num>

```

### On-CPU
Results in `results/On-CPU_time_FlameGraph.svg`.
```
perf record -g ./userfaultfd-kernelfault <page_num>
perf script |sudo ./FlameGraph/stackcollapse-perf.pl > out.perf-folded
./FlameGraph/flamegraph.pl out.perf-folded > on-CPU.svg

```
### Off-CPU
Results in `results/Off-CPU_time_FlameGraph.svg`. This shell script runs the demo userfaultfd program (which sleeps for a while in the fault handling page (`usleep(100000)`) upon page faults), simulating a super costly user page faulting handling routine. It traces the call stack for 30 seconds, while the program should take more than 1 minute. The problem right now is that tracing the life-time of the faulting thread (and exit the trace with Ctrl-C when the faulting thread exits) produces no results.
```
sudo ./trace_offcputime.sh
```