#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <mongoc/mongoc.h>

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
    unsigned long digits = 5000000L;
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
    size_t len;
    char *model = NULL;

    // Query for CPU model
    if (sysctlbyname("machdep.cpu.brand_string", NULL, &len, NULL, 0) == 0)
    {
        model = (char *)malloc(len);
        if (model != NULL)
        {
            if (sysctlbyname("machdep.cpu.brand_string", model, &len, NULL, 0) == 0)
            {
                printf("CPU Model: %s\n", model);
            }
            else
            {
                perror("Error getting CPU model");
            }
            free(model);
        }
        else
        {
            perror("Memory allocation error");
        }
    }
    else
    {
        perror("Error getting CPU model length");
    }

    // Query for OS info
    FILE *os_info_file = popen("sw_vers -productName", "r");
    fgets(os_info, sizeof(os_info), os_info_file);
    pclose(os_info_file);
    os_info[strcspn(os_info, "\n")] = 0;
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

    mongoc_client_t *client;
    mongoc_collection_t *collection;
    bson_error_t error;
    bson_oid_t oid;
    bson_t *doc;

    mongoc_init();

    client = mongoc_client_new("mongodb+srv://timberlake2025:IamMusical222@taipan-cluster-1.iieczyn.mongodb.net/?retryWrites=true&w=majority");
    collection = mongoc_client_get_collection(client, "taipan_benchmarks", "cpu_benchmarks");

    doc = bson_new();
    bson_oid_init(&oid, NULL);
    BSON_APPEND_OID(doc, "_id", &oid);
    BSON_APPEND_UTF8(doc, "cpu_model", cpu_display_model);
    BSON_APPEND_UTF8(doc, "os_info", os_display);
    BSON_APPEND_INT32(doc, "digits", digits);
    BSON_APPEND_INT32(doc, "single_core_score", calculate_score(digits, execution_time_single_core));
    BSON_APPEND_INT32(doc, "multi_core_score", calculate_score(digits, execution_time_multi_core));
    BSON_APPEND_DOUBLE(doc, "speedup", execution_time_single_core / execution_time_multi_core);
    BSON_APPEND_DOUBLE(doc, "efficiency", (execution_time_single_core / execution_time_multi_core) / processes);
    BSON_APPEND_DOUBLE(doc, "cpu_utilization", 100 - (execution_time_multi_core / execution_time_single_core) * 100);
    // Append current time to document in UTC
    time_t t = time(NULL);
    struct tm tm = *gmtime(&t);
    char time_string[64];
    strftime(time_string, sizeof(time_string), "%c", &tm);
    BSON_APPEND_UTF8(doc, "time", time_string);
    // Append hostname to document
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    BSON_APPEND_UTF8(doc, "hostname", hostname);

    if (!mongoc_collection_insert_one(
            collection, doc, NULL, NULL, &error))
    {
        fprintf(stderr, "%s\n", error.message);
    }

    bson_destroy(doc);
    mongoc_collection_destroy(collection);
    mongoc_client_destroy(client);
    mongoc_cleanup();

    return 0;
}