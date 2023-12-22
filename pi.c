#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>

#define NUM_POINTS 10000000

struct ThreadData
{
    int thread_id;
    int points_in_circle;
};

double monte_carlo_pi(int points)
{
    int inside_circle = 0;
    for (int i = 0; i < points; i++)
    {
        double x = (double)rand() / RAND_MAX;
        double y = (double)rand() / RAND_MAX;
        double distance = x * x + y * y;
        if (distance <= 1)
        {
            inside_circle++;
        }
    }
    return inside_circle;
}

#include <sys/mman.h> // for mmap and munmap
#include <sys/wait.h> // for wait
#include <unistd.h>   // for fork

double calculate_pi_with_multiprocessing(int digits, int processes)
{
    int total_points = digits;
    int points_per_process = total_points / processes;

// Create shared memory
#include <sys/mman.h> // Add this line to include the necessary header file

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

    int *results = mmap(NULL, processes * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    for (int i = 0; i < processes; i++)
    {
        if (fork() == 0)
        { // Child process
            results[i] = monte_carlo_pi(points_per_process);
            exit(0); // End child process
        }
    }

    // Wait for all child processes to finish
    for (int i = 0; i < processes; i++)
    {
        wait(NULL);
    }

    int total_inside_circle = 0;
    for (int i = 0; i < processes; i++)
    {
        total_inside_circle += results[i];
    }

    // Free shared memory
    munmap(results, processes * sizeof(int));

    double pi_estimate = (double)(total_inside_circle * 4) / total_points;
    return pi_estimate;
}

double execution_time_for_multiprocessing(int digits, int processes)
{
    clock_t start_time = clock();
    calculate_pi_with_multiprocessing(digits, processes);
    clock_t end_time = clock();
    double execution_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("Execution time for %d digits with %d cores is %f\n", digits, processes, execution_time);
    return execution_time;
}

int calculate_multi_core_score(int digits, double execution_time)
{
    int multi_core_score = (digits / execution_time) / 3333;
    return round(multi_core_score);
}

int calculate_gpu_score(int digits, double execution_time)
{
    int gpu_score = (digits / execution_time) / 3333;
    return round(gpu_score);
}

void display_data_in_txt()
{
    FILE *file = fopen("pi_benchmark.txt", "w");
    if (file == NULL)
    {
        printf("Error opening file.\n");
        return;
    }
    fprintf(file, "CPU Model                                          | Execution time (single core) | Execution time (multi core) | Single core score | Multi core score | Speedup | Efficiency | CPU utilization\n");
    fprintf(file, "\n");
    fclose(file);
}

int main()
{
    srand(time(NULL));
    int digits = NUM_POINTS;
#include <unistd.h>
    int processes = sysconf(_SC_NPROCESSORS_ONLN);
    char cpu_model[256];
    char os_info[256];
#ifdef _WIN32
    FILE *cpu_info = popen("wmic cpu get name", "r");
    fgets(cpu_model, sizeof(cpu_model), cpu_info);
    pclose(cpu_info);
    cpu_model[strcspn(cpu_model, "\n")] = 0;
    FILE *os_info_file = popen("systeminfo | findstr /C:OS", "r");
    fgets(os_info, sizeof(os_info), os_info_file);
    pclose(os_info_file);
    os_info[strcspn(os_info, "\n")] = 0;
    os_info[strcspn(os_info, "\r")] = 0;
    os_info = strtok(os_info, "Version:");
    os_info = strtok(os_info, "                  ");
#elif __linux__
    FILE *cpu_info = popen("cat /proc/cpuinfo | grep 'model name' | uniq", "r");
    fgets(cpu_model, sizeof(cpu_model), cpu_info);
    pclose(cpu_info);
    cpu_model[strcspn(cpu_model, "\n")] = 0;
    // Truncate the string to remove the "model name: " part
    char *token = strtok(cpu_model, "model name: ");
    strcpy(cpu_model, token);
    FILE *os_info_file = popen("lsb_release -d", "r");
    fgets(os_info, sizeof(os_info), os_info_file);
    pclose(os_info_file);
    os_info[strcspn(os_info, "\n")] = 0;
#elif __APPLE__
    FILE *cpu_info = popen("sysctl -n machdep.cpu.brand_string", "r");
    fgets(cpu_model, sizeof(cpu_model), cpu_info);
    pclose(cpu_info);
    cpu_model[strcspn(cpu_model, "\n")] = 0;
    FILE *os_info_file = popen("sw_vers -productName", "r");
    fgets(os_info, sizeof(os_info), os_info_file);
    pclose(os_info_file);
    os_info[strcspn(os_info, "\n")] = 0;
#endif

    double execution_time_single_core = execution_time_for_multiprocessing(digits, 1);
    double execution_time_multi_core = execution_time_for_multiprocessing(digits, processes);
    printf("CPU model: %s\n", cpu_model);
    printf("Execution time for %d digits with single core is %f\n", digits, execution_time_single_core);
    printf("Execution time for %d digits with %d cores is %f\n", digits, processes, execution_time_multi_core);
    printf("Single core score for %d digits is %d\n", digits, calculate_multi_core_score(digits, execution_time_single_core));
    printf("Multi core score for %d digits is %d\n", digits, calculate_multi_core_score(digits, execution_time_multi_core));
    printf("Speedup for %d digits is %f\n", digits, execution_time_single_core / execution_time_multi_core);
    printf("Efficiency for %d digits is %f\n", digits, (execution_time_single_core / execution_time_multi_core) / processes);
    printf("CPU utilization for %d digits is %f%%\n", digits, 100 - (execution_time_multi_core / execution_time_single_core) * 100);
    return 0;
}
