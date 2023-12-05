import json
import multiprocessing
import random
import time
import os
import platform
import numpy as np
from numba import cuda

pi_benchmark_data = json.loads(open("pi_benchmark.json", "r").read())

def monte_carlo_pi(points):
    inside_circle = 0
    for _ in range(points):
        x, y = random.random(), random.random()
        distance = x**2 + y**2
        if distance <= 1:
            inside_circle += 1
    return inside_circle

def calculate_pi_with_multiprocessing(digits, processes):
    total_points = digits
    points_per_process = total_points // processes

    with multiprocessing.Pool(processes=processes) as pool:
        results = pool.map(monte_carlo_pi, [points_per_process] * processes)

    total_inside_circle = sum(results)
    pi_estimate = (total_inside_circle / total_points) * 4
    return pi_estimate


@cuda.jit
def monte_carlo_pi_gpu(points, results):
    thread_id = cuda.grid(1)
    inside_circle = 0
    for i in range(thread_id, points, cuda.gridsize(1)):
        x, y = np.random.random(), np.random.random()
        distance = x**2 + y**2
        if distance <= 1:
            inside_circle += 1
    cuda.atomic.add(results, 0, inside_circle)

def calculate_pi_with_gpu(digits):
    total_points = digits
    block_size = 256
    grid_size = (total_points + block_size - 1) // block_size
    results = np.zeros(1, dtype=np.int32)
    monte_carlo_pi_gpu[grid_size, block_size](total_points, results)
    total_inside_circle = results[0]
    pi_estimate = (total_inside_circle / total_points) * 4
    return pi_estimate


def execution_time_for_multiprocessing(digits, processes):
    start_time = time.time()
    calculate_pi_with_multiprocessing(digits, processes)
    end_time = time.time()
    execution_time = end_time - start_time
    return execution_time

def execution_time_for_gpu(digits):
    start_time = time.time()
    calculate_pi_with_gpu(digits)
    end_time = time.time()
    execution_time = end_time - start_time
    return execution_time

def calculate_multi_core_score(digits, execution_time):
    multi_core_score = (digits / execution_time) / 3333
    return round(multi_core_score)

def calculate_gpu_score(digits, execution_time):
    gpu_score = (digits / execution_time) / 3333
    return round(gpu_score)

def display_data_in_txt():
    benchmark_data = json.loads(open("pi_benchmark.json", "r").read())

    with open("pi_benchmark.txt", "w") as file:
        file.write("CPU Model                                          | Execution time (single core) | Execution time (multi core) | Single core score | Multi core score | Speedup | Efficiency | CPU utilization\n")
        file.write("\n")

        for data in benchmark_data:
            cpu_model = data["cpu_model"]
            execution_time_single_core = data["execution_time_single_core"]
            execution_time_multi_core = data["execution_time_multi_core"]
            single_core_score = round(data["single_core_score"])
            multi_core_score = round(data["multi_core_score"])
            speedup = round(data["speedup"], 2)
            efficiency = round(data["efficiency"], 2)
            cpu_utilization = round(data["cpu_utilization"], 2)
            file.write(f"{cpu_model:<51}| {execution_time_single_core:<29}| {execution_time_multi_core:<28}| {single_core_score:<18}| {multi_core_score:<17}| {speedup:<8}| {efficiency:<11}| {cpu_utilization}%\n")
            
def save_data_to_json():
    with open("pi_benchmark.json", "w") as file:
        json.dump(pi_benchmark_data, file, indent=4)

if __name__ == "__main__":
    digits = 200000000
    processes = os.cpu_count()
    if platform.system() == "Windows":
        cpu_model = os.popen("wmic cpu get name").read()
        cpu_model = cpu_model[cpu_model.find("\n") + 1:]
        cpu_model = cpu_model.replace("\n", "")
    elif platform.system() == "Linux":
        cpu_model = os.popen("cat /proc/cpuinfo | grep 'model name' | uniq").read()
        cpu_model = cpu_model[cpu_model.find(":") + 2:]
        cpu_model = cpu_model.replace("\n", "")
    elif platform.system() == "Darwin":
        cpu_model = os.popen("sysctl -n machdep.cpu.brand_string").read()
        cpu_model = cpu_model.replace("\n", "")
    
    execution_time_single_core = execution_time_for_multiprocessing(digits, 1)
    execution_time_multi_core = execution_time_for_multiprocessing(digits, processes)
    print(f"CPU model: {cpu_model}")
    print(f"Execution time for {digits} digits with single core is {execution_time_single_core}")
    print(f"Execution time for {digits} digits with {processes} cores is {execution_time_multi_core}")
    print(f"Single core score for {digits} digits is {calculate_multi_core_score(digits, execution_time_single_core)}")
    print(f"Multi core score for {digits} digits is {calculate_multi_core_score(digits, execution_time_multi_core)}")
    print(f"Speedup for {digits} digits is {round(execution_time_single_core / execution_time_multi_core, 2)}")
    print(f"Efficiency for {digits} digits is {round((execution_time_single_core / execution_time_multi_core) / processes, 2)}")
    print(f"CPU utilization for {digits} digits is {round(((execution_time_single_core / execution_time_multi_core) / processes) * 100, 2)}%")
    # Add results to pi_benchmark.txt
    # Format them like this:
    # CPU Model                                | Execution time (single core) | Execution time (multi core) | Single core score | Multi core score | Speedup | Efficiency | CPU utilization
    # Intel(R) Core(TM) i5-8250U CPU @ 1.60GHz | 0.2s                         | 0.1s                        | 10000000          | 10000000         | 2       | 1          | 50%
    # Format the results so that they are aligned in columns
    # If the cpu is the same, update the results by taking the average of the old and new results
    # If the cpu is different, add the results to the file
    # If the file does not exist, create it
    # Add results to pi_benchmark.json
    # Format them like this:
    # [
    #     {
    #         "cpu_model": "Intel(R) Core(TM) i5-8250U CPU @ 1.60GHz",
    #         "execution_time_single_core": 0.2,
    #         "execution_time_multi_core": 0.1,
    #         "single_core_score": 10000000,
    #         "multi_core_score": 10000000,
    #         "speedup": 2,
    #         "efficiency": 1,
    #         "cpu_utilization": 50
    #         "number_of_cpus_tested": 1
    #     }
    # ]
    for elem in pi_benchmark_data:
        if cpu_model == elem["cpu_model"]:
            elem["execution_time_single_core"] = (elem["number_of_cpus_tested"] * elem["execution_time_single_core"] + execution_time_single_core) / (elem["number_of_cpus_tested"] + 1)
            elem["execution_time_multi_core"] = (elem["number_of_cpus_tested"] * elem["execution_time_multi_core"] + execution_time_multi_core) / (elem["number_of_cpus_tested"] + 1)
            elem["single_core_score"] = (elem["number_of_cpus_tested"] * elem["single_core_score"] + calculate_multi_core_score(digits, execution_time_single_core)) / (elem["number_of_cpus_tested"] + 1)
            elem["multi_core_score"] = (elem["number_of_cpus_tested"] * elem["multi_core_score"] + calculate_multi_core_score(digits, execution_time_multi_core)) / (elem["number_of_cpus_tested"] + 1)
            elem["speedup"] = (elem["number_of_cpus_tested"] * elem["speedup"] + round(execution_time_single_core / execution_time_multi_core, 2)) / (elem["number_of_cpus_tested"] + 1)
            elem["efficiency"] = (elem["number_of_cpus_tested"] * elem["efficiency"] + round((execution_time_single_core / execution_time_multi_core) / processes, 2)) / (elem["number_of_cpus_tested"] + 1)
            elem["cpu_utilization"] = (elem["number_of_cpus_tested"] * elem["cpu_utilization"] + round(((execution_time_single_core / execution_time_multi_core) / processes) * 100, 2)) / (elem["number_of_cpus_tested"] + 1)
            elem["number_of_cpus_tested"] += 1
            break
    if cpu_model not in [elem["cpu_model"] for elem in pi_benchmark_data]:
        pi_benchmark_data.append({
            "cpu_model": cpu_model,
            "execution_time_single_core": execution_time_single_core,
            "execution_time_multi_core": execution_time_multi_core,
            "single_core_score": calculate_multi_core_score(digits, execution_time_single_core),
            "multi_core_score": calculate_multi_core_score(digits, execution_time_multi_core),
            "speedup": round(execution_time_single_core / execution_time_multi_core, 2),
            "efficiency": round((execution_time_single_core / execution_time_multi_core) / processes, 2),
            "cpu_utilization": round(((execution_time_single_core / execution_time_multi_core) / processes) * 100, 2),
            "number_of_cpus_tested": 1
        })
    save_data_to_json()
    display_data_in_txt()
    