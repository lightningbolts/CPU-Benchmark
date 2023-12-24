#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <curl/curl.h>
#include <jansson.h>

/* Each thread gets a start and end number and returns the number
   Of primes in that range */
struct range
{
    unsigned long start;
    unsigned long end;
    unsigned long count;
};

/* Thread function for counting primes */
void *
prime_check(void *_args)
{
    /* Cast the args to a usable struct type */
    struct range *args = (struct range *)_args;
    unsigned long iter = 2;
    unsigned long value;

    /* Skip over any numbers < 2, which is the smallest prime */
    if (args->start < 2)
        args->start = 2;

    /* Loop from this thread's start to this thread's end */
    args->count = 0;
    for (value = args->start; value <= args->end; value++)
    {
        /* Trivial and intentionally slow algorithm:
           Start with iter = 2; see if iter divides the number evenly.
           If it does, it's not prime.
           Stop when iter exceeds the square root of value */
        bool is_prime = true;
        for (iter = 2; iter * iter <= value && is_prime; iter++)
            if (value % iter == 0)
                is_prime = false;

        if (is_prime)
            args->count++;
    }

    /* All values in the range have been counted, so exit */
    pthread_exit(NULL);
}

int multicore_processing_prime(unsigned long digits, int num_threads)
{
    pthread_t threads[num_threads];
    struct range *args[num_threads];
    int thread;

    /* Count the number of primes smaller than this value: */
    unsigned long number_count = digits;

    /* Specify start and end values, then split based on number of
       threads */
    unsigned long start = 0;
    unsigned long end = start + number_count;
    unsigned long number_per_thread = number_count / num_threads;

    /* Simplification: make range divide evenly among the threads */
    // assert(number_count % num_threads == 0);

    /* Assign a start/end value for each thread, then create it */
    for (thread = 0; thread < num_threads; thread++)
    {
        args[thread] = calloc(sizeof(struct range), 1);
        args[thread]->start = start + (thread * number_per_thread);
        args[thread]->end =
            args[thread]->start + number_per_thread - 1;
        assert(pthread_create(&threads[thread], NULL, prime_check,
                              args[thread]) == 0);
    }

    /* All threads are running. Join all to collect the results. */
    unsigned long total_number = 0;
    for (thread = 0; thread < num_threads; thread++)
    {
        pthread_join(threads[thread], NULL);
        // printf("From %ld to %ld: %ld\n", args[thread]->start,
        //        args[thread]->end, args[thread]->count);
        total_number += args[thread]->count;
        free(args[thread]);
    }

    /* Display the total number of primes in the specified range. */
    // printf("===============================================\n");
    // printf("Total number of primes less than %ld: %ld\n", end,
    //        total_number);

    return 0;
}

double calculate_execution_time(int digits, int num_threads)
{

    struct timeval start, end;
    gettimeofday(&start, NULL);
    multicore_processing_prime(digits, num_threads);
    gettimeofday(&end, NULL);
    double time_taken = end.tv_sec + end.tv_usec / 1e6 -
                        start.tv_sec - start.tv_usec / 1e6; // in seconds
    return time_taken;
}

int calculate_score(int digits, double execution_time)
{
    int multi_core_score = (digits / execution_time) / 666;
    return round(multi_core_score);
}

int main(int argc, char **argv)
{
    unsigned long digits = 50000000L;
    srand(time(NULL));
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
    // Get CPU model
    FILE *cpuinfo = fopen("/proc/cpuinfo", "r");

    if (cpuinfo == NULL)
    {
        perror("Error opening /proc/cpuinfo");
        exit(EXIT_FAILURE);
    }

    char line[256];
    int found = 0;

    while (fgets(line, sizeof(line), cpuinfo) != NULL)
    {
        if (strstr(line, "model name"))
        {
            // Remove "model name:" from the line
            char *model_info = strchr(line, ':');
            if (model_info != NULL)
            {
                model_info++; // Move past ':'
                while (*model_info == ' ' || *model_info == '\t')
                {
                    model_info++; // Skip leading spaces or tabs
                }
                // printf("CPU Model: %s", model_info);
                found = 1;
                break;
            }
        }
    }

    if (!found)
    {
        printf("CPU Model information not found in /proc/cpuinfo.\n");
    }
    char *model_info = strchr(line, ':');
    // Remove /n from the line
    model_info[strcspn(model_info, "\n")] = 0;
    strcpy(cpu_model, model_info);
    // Remove leading spaces or tabs
    while (*model_info == ' ' || *model_info == '\t')
    {
        model_info++; // Skip leading spaces or tabs
    }
    // Declare cpu_display_model variable
    char *cpu_display_model;
    // Assign value to cpu_display_model
    cpu_display_model = model_info + 2;

    fclose(cpuinfo);
    // Get OS info
    FILE *os_info_file = popen("lsb_release -d", "r");
    fgets(os_info, sizeof(os_info), os_info_file);
    pclose(os_info_file);
    os_info[strcspn(os_info, "\n")] = 0;
    // Remove "Description:" from the line
    char *os_display = strchr(os_info, ':');
    os_display++;
    // Remove leading spaces or tabs
    while (*os_display == ' ' || *os_display == '\t')
    {
        os_display++; // Skip leading spaces or tabs
    }
    // printf("OS Info: %s\n", os_display);

#elif __APPLE__
#include <sys/sysctl.h>

    // Query for CPU model
    size_t len = sizeof(cpu_model);
    sysctlbyname("machdep.cpu.brand_string", &cpu_model, &len, NULL, 0);
    // Query for OS info
    size_t len2 = sizeof(os_info);
    sysctlbyname("kern.osproductversion", &os_info, &len2, NULL, 0);
    // Declare model_info variable
    char *model_info;
    // Assign value to model_info
    model_info = cpu_model;
    // Declare cpu_display_model variable
    char *cpu_display_model;
    // Assign value to cpu_display_model
    cpu_display_model = cpu_model;
    // Declare os_display variable
    char *os_display;
    // Assign value to os_display
    os_display = os_info;
    // Add "macOS" to the beginning of os_display
    char *os_display_prefix = "macOS ";
    char *os_display_temp = malloc(strlen(os_display_prefix) + strlen(os_display) + 1);
    strcpy(os_display_temp, os_display_prefix);
    strcat(os_display_temp, os_display);
    os_display = os_display_temp;

#endif

    double execution_time_single_core = calculate_execution_time(digits, 1);
    double execution_time_multi_core = calculate_execution_time(digits, processes);
    printf("CPU Model%s", model_info);
    printf("\n");
    printf(os_display);
    printf("\n");
    printf("Execution time for %d digits with single core is %f\n", digits, execution_time_single_core);
    printf("Execution time for %d digits with %d cores is %f\n", digits, processes, execution_time_multi_core);
    printf("Single core score for %d digits is %d\n", digits, calculate_score(digits, execution_time_single_core));
    printf("Multi core score for %d digits is %d\n", digits, calculate_score(digits, execution_time_multi_core));
    printf("Speedup for %d digits is %f\n", digits, execution_time_single_core / execution_time_multi_core);
    printf("Efficiency for %d digits is %f\n", digits, (execution_time_single_core / execution_time_multi_core) / processes);
    printf("CPU utilization for %d digits is %f%%\n", digits, 100 - (execution_time_multi_core / execution_time_single_core) * 100);

    time_t t = time(NULL);
    struct tm tm = *gmtime(&t);
    char time_string[64];
    strftime(time_string, sizeof(time_string), "%c", &tm);

    char hostname[256];
    gethostname(hostname, sizeof(hostname));

    struct prime_benchmark
    {
        /* data */
        char *cpu_model;
        char *os_info;
        unsigned long digits;
        int single_core_score;
        int multi_core_score;
        double speedup;
        double efficiency;
        double cpu_utilization;
        char *time;
        char *hostname;
    };

    struct prime_benchmark prime_benchmark = {
        cpu_display_model,
        os_display,
        digits,
        calculate_score(digits, execution_time_single_core),
        calculate_score(digits, execution_time_multi_core),
        execution_time_single_core / execution_time_multi_core,
        (execution_time_single_core / execution_time_multi_core) / processes,
        100 - (execution_time_multi_core / execution_time_single_core) * 100,
        time_string,
        hostname};

    // Function to convert struct to JSON object
    json_t *structToJson(struct prime_benchmark * prime_benchmark)
    {
        json_t *prime_benchmark_json = json_object();
        json_object_set_new(prime_benchmark_json, "cpu_model", json_string(prime_benchmark->cpu_model));
        json_object_set_new(prime_benchmark_json, "os_info", json_string(prime_benchmark->os_info));
        json_object_set_new(prime_benchmark_json, "digits", json_integer(prime_benchmark->digits));
        json_object_set_new(prime_benchmark_json, "single_core_score", json_integer(prime_benchmark->single_core_score));
        json_object_set_new(prime_benchmark_json, "multi_core_score", json_integer(prime_benchmark->multi_core_score));
        json_object_set_new(prime_benchmark_json, "speedup", json_real(prime_benchmark->speedup));
        json_object_set_new(prime_benchmark_json, "efficiency", json_real(prime_benchmark->efficiency));
        json_object_set_new(prime_benchmark_json, "cpu_utilization", json_real(prime_benchmark->cpu_utilization));
        json_object_set_new(prime_benchmark_json, "time", json_string(prime_benchmark->time));
        json_object_set_new(prime_benchmark_json, "hostname", json_string(prime_benchmark->hostname));
        return prime_benchmark_json;
    }

    size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
    {
        // Simply print the response to the console
        printf("%.*s", (int)(size * nmemb), (char *)contents);
        printf("\n");
        return size * nmemb;
    }

    // Connect to API endpoint: https://taipan-benchmarks.vercel.app/api/cpu-benchmarks
    char *host = "https://taipan-benchmarks.vercel.app/api/cpu-benchmarks";
    // Initialize libcurl
    CURL *curl;
    CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);

    if (res != CURLE_OK)
    {
        fprintf(stderr, "curl_global_init() failed: %s\n", curl_easy_strerror(res));
        return 1;
    }

    // Create a curl handle
    curl = curl_easy_init();

    if (!curl)
    {
        fprintf(stderr, "curl_easy_init() failed\n");
        curl_global_cleanup();
        return 1;
    }

    // Set the target URL
    const char *url = "https://taipan-benchmarks.vercel.app/api/cpu-benchmarks";
    curl_easy_setopt(curl, CURLOPT_URL, url);

    // Set the HTTP method to POST
    curl_easy_setopt(curl, CURLOPT_POST, 1L);

    // Set the POST data (replace with your JSON object)

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_dumps(structToJson(&prime_benchmark), 0));

    printf("Sending request to %s\n", url);

    // Set the Content-Type header
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Set the callback function to handle the server response
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

    // Perform the HTTP request
    res = curl_easy_perform(curl);

    printf("Request sent successfully!\n");

    // Check for errors
    if (res != CURLE_OK)
    {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    }

    // Clean up
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 0;
}