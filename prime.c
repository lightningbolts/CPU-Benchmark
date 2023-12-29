#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <inttypes.h>
#include <string.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#define MAX_BUFFER_SIZE 1024

/* Each thread gets a start and end number and returns the number
   Of primes in that range */
struct range
{
    int64_t start;
    int64_t end;
    int64_t count;
};

struct prime_benchmark
{
    /* data */
    char *cpu_model;
    char *os_info;
    int64_t digits;
    int64_t single_core_score;
    int64_t multi_core_score;
    double speedup;
    double efficiency;
    double cpu_utilization;
    char *time;
    char *hostname;
    char *key;
    int64_t processes;
};

void error(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

// Function to convert struct to JSON string
void primeBenchmarkToJson(struct prime_benchmark benchmark, char *jsonString)
{
    sprintf(jsonString, "{"
                        "\"cpu_model\":\"%s\","
                        "\"os_info\":\"%s\","
                        "\"digits\":%" PRId64 ","
                        "\"single_core_score\":%d,"
                        "\"multi_core_score\":%d,"
                        "\"speedup\":%lf,"
                        "\"efficiency\":%lf,"
                        "\"cpu_utilization\":%lf,"
                        "\"time\":\"%s\","
                        "\"hostname\":\"%s\","
                        "\"key\":\"%s\","
                        "\"processes\":%d"
                        "}",
            benchmark.cpu_model, benchmark.os_info, benchmark.digits,
            benchmark.single_core_score, benchmark.multi_core_score,
            benchmark.speedup, benchmark.efficiency, benchmark.cpu_utilization,
            benchmark.time, benchmark.hostname, benchmark.key, benchmark.processes);
}

/* Thread function for counting primes */
void *
prime_check(void *_args)
{
    /* Cast the args to a usable struct type */
    struct range *args = (struct range *)_args;
    int64_t iter = 2;
    int64_t value;

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

int multicore_processing_prime(int64_t digits, int num_threads)
{
    pthread_t threads[num_threads];
    struct range *args[num_threads];
    int thread;

    /* Count the number of primes smaller than this value: */
    int64_t number_count = digits;

    /* Specify start and end values, then split based on number of
       threads */
    int64_t start = 0;
    int64_t end = start + number_count;
    int64_t number_per_thread = number_count / num_threads;

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
    int64_t total_number = 0;
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

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    // Simply print the response to the console
    printf("%.*s", (int)(size * nmemb), (char *)contents);
    printf("\n");
    return size * nmemb;
}

int main(int argc, char **argv)
{
    int64_t digits = 50000000L;
    srand(time(NULL));
    int processes;
    char cpu_model[256];
    char os_info[256];
#ifdef _WIN32
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    processes = sysInfo.dwNumberOfProcessors;
    // Get CPU model
    char cpu_model_temp[256];
    DWORD bufSize = sizeof(cpu_model_temp);
    DWORD dwMHz = bufSize;
    HKEY hKey;
    LONG lError = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                               "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                               0,
                               KEY_READ,
                               &hKey);
    if (lError != ERROR_SUCCESS)
    {
        printf("Error opening key");
    }
    // Query CPU model
    RegQueryValueEx(hKey, "ProcessorNameString", NULL, NULL, (LPBYTE)cpu_model_temp, &bufSize);
    strcpy(cpu_model, cpu_model_temp);

    // Remove leading spaces or tabs
    //    while (*cpu_model == ' ' || *cpu_model == '\t')
    //    {
    //        cpu_model++; // Skip leading spaces or tabs
    //    }
    // Declare model_info variable
    char *model_info;
    // Assign value to model_info
    model_info = cpu_model;
    // Declare cpu_display_model variable
    char *cpu_display_model;
    // Assign value to cpu_display_model
    cpu_display_model = cpu_model;

    // Get OS info
    char os_info_temp[256];
    DWORD bufSize2 = sizeof(os_info_temp);
    HKEY hKey2;
    LONG lError2 = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                                0,
                                KEY_READ,
                                &hKey2);
    if (lError2 != ERROR_SUCCESS)
    {
        printf("Error opening key");
    }
    // Query OS info
    RegQueryValueEx(hKey2, "ProductName", NULL, NULL, (LPBYTE)os_info_temp, &bufSize2);
    strcpy(os_info, os_info_temp);
    // Remove leading spaces or tabs
    //    while (*os_info == ' ' || *os_info == '\t')
    //    {
    //        os_info++; // Skip leading spaces or tabs
    //    }
    // Declare os_display variable
    char *os_display;
    // Assign value to os_display
    os_display = os_info;
#elif __linux__
    processes = sysconf(_SC_NPROCESSORS_ONLN);
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
    processes = sysconf(_SC_NPROCESSORS_ONLN);
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

    // Generate 32 digit hex key
    char key[33];
    for (int i = 0; i < 32; i++)
    {
        key[i] = "0123456789ABCDEF"[rand() % 16];
    }
    key[32] = '\0';
    time_t t = time(NULL);
    struct tm tm = *gmtime(&t);
    char time_string[64];
    strftime(time_string, sizeof(time_string), "%c", &tm);

    char hostname[256];
#ifdef __linux__
    gethostname(hostname, sizeof(hostname));
#elif __APPLE__
    gethostname(hostname, sizeof(hostname));
#elif _WIN32
    DWORD size = sizeof(hostname);
    GetComputerNameA(hostname, &size);
#endif

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
        hostname,
        key,
        processes};

    char jsonString[1024];
    primeBenchmarkToJson(prime_benchmark, jsonString);

    // Server information
    const char *host = "taipan-benchmarks.vercel.app";
    const char *path = "/api/cpu-benchmarks";
    const int port = 443;

    // Initialize OpenSSL
    SSL_library_init();
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx)
    {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    // Create a socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        error("Error opening socket");
    }

    // Resolve the host IP address
    struct hostent *server = gethostbyname(host);
    if (server == NULL)
    {
        fprintf(stderr, "Error: Could not resolve host %s\n", host);
        exit(EXIT_FAILURE);
    }

    // Set up the server address structure
    struct sockaddr_in server_addr;
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&server_addr.sin_addr.s_addr, server->h_length);
    server_addr.sin_port = htons(port);

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        error("Error connecting to server");
    }

    // Create an SSL connection
    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sockfd);
    if (SSL_connect(ssl) != 1)
    {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    // JSON object to send
    const char *jsonObject = jsonString;
    printf("Sending JSON object:\n%s\n", jsonObject);

    // Build the HTTP request
    char request[1024];
    snprintf(request, sizeof(request),
             "POST %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %zu\r\n"
             "\r\n"
             "%s",
             path, host, strlen(jsonObject), jsonObject);

    // Send the HTTP request
    SSL_write(ssl, request, strlen(request));

    // Read and print the response
    char response[4096];
    int bytesRead;
    while (1)
    {
        bytesRead = SSL_read(ssl, response, sizeof(response));
        if (bytesRead <= 10)
        {
            break;
        }
        printf("%d", bytesRead);
        printf("%.*s", bytesRead, response);
    }

    // Get object id from response
    char *object_id = strstr(response, "_id");
    // Display two urls: one for viewing the benchmark and one for claiming the benchmark
    // View benchmark url link: https://taipan-benchmarks.vercel.app/cpu-benchmarks/<object_id>
    // Claim benchmark url link: https://taipan-benchmarks.vercel.app/cpu-benchmarks/<object_id>?key=<key>
    char view_benchmark_url[256];
    // char claim_benchmark_url[256];
    snprintf(view_benchmark_url, sizeof(view_benchmark_url),
             "https://taipan-benchmarks.vercel.app/cpu-benchmarks/%.*s",
             24, object_id + 6);
    // snprintf(claim_benchmark_url, sizeof(claim_benchmark_url),
    //          "https://taipan-benchmarks.vercel.app/cpu-benchmarks/%.*s?key=%s",
    //          24, object_id + 6, key);
    printf("View benchmark url: %s\n", view_benchmark_url);
    // printf("Claim benchmark url: %s\n", claim_benchmark_url);

    // Close the SSL connection and free resources
    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);

    // Close the socket
    close(sockfd);
    return 0;
}