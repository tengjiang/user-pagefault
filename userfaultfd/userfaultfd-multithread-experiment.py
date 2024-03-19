import subprocess
import time
import csv
import datetime
import math
import matplotlib.pyplot as plt
import os

# Function to run the experiment and return execution times
def run_experiment(num_threads):
    output = subprocess.check_output(['sudo', './userfaultfd-multithread', '50', str(num_threads)], stderr=subprocess.STDOUT, text=True)
    return output

# Constants
thread_nums = [1, 2, 4, 8, 16, 32, 64, 128, 192, 256, 320 ,384, 448, 512]
N = 50  # Number of times to run each command

# Create results directory if it doesn't exist
timestamp = datetime.datetime.now().strftime('%Y-%m-%d_%H-%M-%S')
results_dir = 'results/multithread/' + timestamp
os.makedirs(results_dir, exist_ok=True)

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
filename_output_txt = f'{results_dir}/userfaultfd-multithread-output.txt'
with open(filename_output_txt, 'w') as f:
    for thread_output in outputs:
        for output in thread_output:
            f.write(output)
            f.write('\n')

# Save results to text file
filename_txt = f'{results_dir}/userfaultfd-multithread-results.txt'
with open(filename_txt, 'w') as f:
    for num_threads, avg_time_faulting, avg_time_raw in results:
        f.write(f"{num_threads} faulting threads: Average time = {avg_time_faulting} seconds\n")
        f.write(f"Raw page fault: Average time = {avg_time_raw} seconds\n\n")

# Save results to CSV file
filename_csv = f'{results_dir}/userfaultfd-multithread-results.csv'
with open(filename_csv, 'w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow(['num_threads', 'avg_time_faulting', 'avg_time_raw'])
    for num_threads, avg_time_faulting, avg_time_raw in results:
        writer.writerow([num_threads, avg_time_faulting, avg_time_raw])

# Plot results
plt.figure(figsize=(10, 6))
x_values = [num_threads for num_threads, _, _ in results]
y_values_faulting = [avg_time_faulting for _, avg_time_faulting, _ in results]
y_values_raw = [avg_time_raw for _, _, avg_time_raw in results]

# Plot the first line with its own y-axis
plt.plot(x_values, y_values_faulting, label='Average Time with Faulting Threads', color='tab:blue', marker='x')
plt.xlabel('num_threads')
plt.ylabel('Average Time with Faulting Threads (seconds)', color='tab:blue')
plt.tick_params(axis='y', labelcolor='tab:blue')
plt.grid(True)
lines1, labels1 = plt.gca().get_legend_handles_labels()  # Get handles and labels from the first plot


# Create a second y-axis sharing the same x-axis
plt.twinx()
# Plot the second line with its own y-axis
plt.plot(x_values, y_values_raw, label='Average Time with Raw Page Fault', color='tab:red', marker='o')
plt.ylabel('Average Time with Raw Page Fault (seconds)', color='tab:red')
plt.tick_params(axis='y', labelcolor='tab:red')

plt.title('Average Execution Time vs. Number of Threads')
plt.grid(True)

lines2, labels2 = plt.gca().get_legend_handles_labels()  # Get handles and labels from the second plot
plt.legend(lines1 + lines2, labels1 + labels2, loc='upper left')

# Save plot to file
filename_plot_png = f'{results_dir}/userfaultfd-multithread-results-scaled.png'
plt.savefig(filename_plot_png)
plt.show()

# https://stackoverflow.com/questions/5484922/secondary-axis-with-twinx-how-to-add-to-legend

# With a unified y axis
plt.figure(figsize=(10, 6))
plt.plot(x_values, y_values_faulting, label='Average Time with Faulting Threads', marker='o')
plt.plot(x_values, y_values_raw, label='Average Time with Raw Page Fault', marker='o')
plt.xlabel('num_threads')
plt.ylabel('Average Execution Time (seconds)')
plt.title('Average Execution Time vs. Number of Threads')
plt.legend()
plt.grid(True)
plt.savefig(f'{results_dir}/userfaultfd-multithread-results.png')
plt.show()
