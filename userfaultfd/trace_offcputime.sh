./userfaultfd-demo 1000 > /dev/null 2>&1 &

tid=$(pgrep -f userfaultfd | sort -n | tail -n 1) # to get the faulting thread tid
echo $tid

# python3 ./bcc/tools/offcputime.py -df -t $tid > out_$tid.stacks &
python3 ./bcc/tools/offcputime.py -df -t $tid 30 > out_$tid.stacks # Trace for 30 seconds

# BCC_PID=$!
# echo $BCC_PID

# wait $tid
echo "Execution finished"

# kill -2 $BCC_PID

./FlameGraph/flamegraph.pl --color=io --title="Off-CPU Time Flame Graph" --countname=us < out_$tid.stacks > off-CPU_time_$tid.svg