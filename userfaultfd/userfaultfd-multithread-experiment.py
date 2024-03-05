import subprocess
import time
import csv
import datetime
import math
import matplotlib.pyplot as plt

# Function to run the experiment and return execution times
def run_experiment(num_threads):
    output = subprocess.check_output(['sudo', './userfaultfd-multithread', '1000', str(num_threads)], stderr=subprocess.STDOUT, text=True)
    return output

# Constants
thread_nums = [1, 2, 4, 8, 16, 32, 64, 128, 256]
N = 10 # Number of times to run each command

# Run experiments
results = []
outputs = []
for num_threads in thread_nums:
    avg_time_faulting = 0
    avg_time_raw = 0
    thread_output = []
    for _ in range(N):
        output = run_experiment(num_threads)
        thread_output.append(output)
        time_faulting = float(output.split('\n')[1].split()[-2])
        time_raw = float(output.split('\n')[2].split()[-2])
        avg_time_faulting += time_faulting
        avg_time_raw += time_raw
    avg_time_faulting /= N
    avg_time_raw /= N
    results.append((num_threads, avg_time_faulting, avg_time_raw))
    outputs.append(thread_output)

# Save output to text file
timestamp = datetime.datetime.now().strftime('%Y-%m-%d_%H-%M-%S')
filename_output_txt = f'userfaultfd-multithread-output-{timestamp}.txt'
with open(filename_output_txt, 'w') as f:
    for thread_output in outputs:
        for output in thread_output:
            f.write(output)
            f.write('\n')

# Save results to text file
filename_txt = f'userfaultfd-multithread-results-{timestamp}.txt'
with open(filename_txt, 'w') as f:
    for num_threads, avg_time_faulting, avg_time_raw in results:
        f.write(f"{num_threads} faulting threads: Average time = {avg_time_faulting} seconds\n")
        f.write(f"Raw page fault: Average time = {avg_time_raw} seconds\n\n")

# Save results to CSV file
filename_csv = f'userfaultfd-multithread-results-{timestamp}.csv'
with open(filename_csv, 'w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow(['num_threads', 'avg_time_faulting', 'avg_time_raw'])
    for num_threads, avg_time_faulting, avg_time_raw in results:
        writer.writerow([num_threads, avg_time_faulting, avg_time_raw])

# Plot results
x_values = [math.log2(num_threads) for num_threads, _, _ in results]
y_values_faulting = [avg_time_faulting for _, avg_time_faulting, _ in results]
y_values_raw = [avg_time_raw for _, _, avg_time_raw in results]
fig, ax1 = plt.subplots(figsize=(10, 6))

# Plot the first line with its own y-axis
ax1.plot(x_values, y_values_faulting, label='Average Time with Faulting Threads', color='tab:blue', marker='o')
ax1.set_xlabel('log2(num_threads)')
ax1.set_ylabel('Average Time with Faulting Threads (seconds)', color='tab:blue')
ax1.tick_params(axis='y', labelcolor='tab:blue')
ax1.grid(True)

# Create a second y-axis sharing the same x-axis
ax2 = ax1.twinx()
# Plot the second line with its own y-axis
ax2.plot(x_values, y_values_raw, label='Average Time with Raw Page Fault', color='tab:red', marker='o')
ax2.set_ylabel('Average Time with Raw Page Fault (seconds)', color='tab:red')
ax2.tick_params(axis='y', labelcolor='tab:red')

fig.tight_layout()  # Adjust layout to make room for y-axis labels
plt.title('Average Execution Time vs. Number of Threads')
plt.savefig(f'userfaultfd-multithread-results-{timestamp}.png')
plt.show()