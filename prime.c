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
    strcpy(cpu_model, model_info);

    fclose(cpuinfo);
    // Get OS info
    FILE *os_info_file = popen("lsb_release -d", "r");
    fgets(os_info, sizeof(os_info), os_info_file);
    pclose(os_info_file);
    os_info[strcspn(os_info, "\n")] = 0;
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
    printf(os_info);
    printf("\n");
    printf("Execution time for %d digits with single core is %f\n", digits, execution_time_single_core);
    printf("Execution time for %d digits with %d cores is %f\n", digits, processes, execution_time_multi_core);
    printf("Single core score for %d digits is %d\n", digits, calculate_score(digits, execution_time_single_core));
    printf("Multi core score for %d digits is %d\n", digits, calculate_score(digits, execution_time_multi_core));
    printf("Speedup for %d digits is %f\n", digits, execution_time_single_core / execution_time_multi_core);
    printf("Efficiency for %d digits is %f\n", digits, (execution_time_single_core / execution_time_multi_core) / processes);
    printf("CPU utilization for %d digits is %f%%\n", digits, 100 - (execution_time_multi_core / execution_time_single_core) * 100);

    bson_error_t error = {0};
    mongoc_server_api_t *api = NULL;
    mongoc_database_t *database = NULL;
    bson_t *command = NULL;
    bson_t reply = BSON_INITIALIZER;
    int rc = 0;
    bool ok = true;
    mongoc_client_t *client = NULL; // Declare the "client" variable

    // Initialize the MongoDB C Driver.
    mongoc_init();
    char *MONGODB_URI = getenv("MONGODB_URI");
    printf("MONGODB_URI: %s\n", MONGODB_URI);
    client = mongoc_client_new(getenv("MONGODB_URI"));
    if (!client)
    {
        fprintf(stderr, "Failed to create a MongoDB client.\n");
        rc = 1;
        goto cleanup;
    }
    // Set the version of the Stable API on the client.
    api = mongoc_server_api_new(MONGOC_SERVER_API_V1);
    if (!api)
    {
        fprintf(stderr, "Failed to create a MongoDB server API.\n");
        rc = 1;
        goto cleanup;
    }
    ok = mongoc_client_set_server_api(client, api, &error);
    if (!ok)
    {
        bson_error_t error = {0};
        mongoc_server_api_t *api = NULL;
        mongoc_database_t *database = NULL;
        bson_t *command = NULL;
        bson_t reply = BSON_INITIALIZER;
        int rc = 0;
        bool ok = true;
        // Initialize the MongoDB C Driver.
        mongoc_init();
        client = mongoc_client_new("mongodb+srv://timberlake2025:<password>@taipan-cluster-1.iieczyn.mongodb.net/?retryWrites=true&w=majority");
        if (!client)
        {
            fprintf(stderr, "Failed to create a MongoDB client.\n");
            rc = 1;
            goto cleanup;
        }
        // Set the version of the Stable API on the client.
        api = mongoc_server_api_new(MONGOC_SERVER_API_V1);
        if (!api)
        {
            fprintf(stderr, "Failed to create a MongoDB server API.\n");
            rc = 1;
            goto cleanup;
        }
        ok = mongoc_client_set_server_api(client, api, &error);
        if (!ok)
        {
            fprintf(stderr, "error: %s\n", error.message);
            rc = 1;
            goto cleanup;
        }

        // Get a handle on the "admin" database.
        database = mongoc_client_get_database(client, "admin");
        if (!database)
        {
            fprintf(stderr, "Failed to get a MongoDB database handle.\n");
            rc = 1;
            goto cleanup;
        }
        // Ping the database.
        command = BCON_NEW("ping", BCON_INT32(1));
        ok = mongoc_database_command_simple(
            database, command, NULL, &reply, &error);
        if (!ok)
        {
            fprintf(stderr, "error: %s", error.message);
            rc = 1;
            goto cleanup;
        }
        bson_destroy(&reply);
        printf("Pinged your deployment. You successfully connected to MongoDB!\n");
        // Perform cleanup.
    cleanup:
        bson_destroy(command);
        mongoc_database_destroy(database);
        mongoc_server_api_destroy(api);
        mongoc_client_destroy(client);
        mongoc_cleanup();
        return rc;
    }
}