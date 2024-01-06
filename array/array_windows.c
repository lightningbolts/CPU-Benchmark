#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <Winsock2.h>
#include <windows.h>
#include <inttypes.h>
#include <string.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "libcrypto.lib")

// #define API_URL "taipan-benchmarks.vercel.app"
// #define API_PATH "/api/cpu-benchmarks"
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

// Structure to pass arguments to the thread function
struct range_array
{
    int64_t *array;
    int64_t start;
    int64_t end;
};

// Function to merge two sorted arrays
void merge(int *array, int start, int mid, int end)
{
    int n1 = mid - start + 1;
    int n2 = end - mid;

    int *left = malloc(n1 * sizeof(int));
    int *right = malloc(n2 * sizeof(int));

    for (int i = 0; i < n1; i++)
        left[i] = array[start + i];
    for (int j = 0; j < n2; j++)
        right[j] = array[mid + 1 + j];

    int i = 0, j = 0, k = start;
    while (i < n1 && j < n2)
    {
        if (left[i] <= right[j])
        {
            array[k] = left[i];
            i++;
        }
        else
        {
            array[k] = right[j];
            j++;
        }
        k++;
    }

    while (i < n1)
    {
        array[k] = left[i];
        i++;
        k++;
    }

    while (j < n2)
    {
        array[k] = right[j];
        j++;
        k++;
    }

    free(left);
    free(right);
}

// Function to perform merge sort on a range of the array
void merge_sort(int *array, int start, int end)
{
    if (start < end)
    {
        int mid = start + (end - start) / 2;

        merge_sort(array, start, mid);
        merge_sort(array, mid + 1, end);

        merge(array, start, mid, end);
    }
}

// Thread function for sorting a portion of the array
void *sort_array_thread(void *_args)
{
    struct range_array *args = (struct range_array *)_args;
    merge_sort(args->array, args->start, args->end);

    pthread_exit(NULL);
}

// Function to sort an array using multiple threads
void multicore_processing_sort(int *array, int array_size, int num_threads)
{
    pthread_t threads[num_threads];
    struct range_array args[num_threads];
    int thread;

    int elements_per_thread = array_size / num_threads;
    int remaining_elements = array_size % num_threads;

    int start = 0;
    for (thread = 0; thread < num_threads; thread++)
    {
        args[thread].array = array;
        args[thread].start = start;
        args[thread].end = start + elements_per_thread - 1;

        if (remaining_elements > 0)
        {
            args[thread].end++;
            remaining_elements--;
        }

        start = args[thread].end + 1;

        assert(pthread_create(&threads[thread], NULL, sort_array_thread, &args[thread]) == 0);
    }

    // Join all threads to wait for sorting completion
    for (thread = 0; thread < num_threads; thread++)
    {
        pthread_join(threads[thread], NULL);
    }

    // Merge the sorted segments
    int merged_size = elements_per_thread * num_threads;
    while (merged_size < array_size)
    {
        for (int i = 0; i < array_size; i += merged_size * 2)
        {
            int start = i;
            int mid = i + merged_size - 1;
            int end = i + merged_size * 2 - 1;

            if (mid >= array_size)
                continue;

            if (end >= array_size)
                end = array_size - 1;

            merge(array, start, mid, end);
        }

        merged_size *= 2;
    }
}

// Calculate execution time of sorting
double calculate_execution_time_sort(int *array, int array_size, int num_threads)
{
    struct timeval start, end;
    gettimeofday(&start, NULL);
    multicore_processing_sort(array, array_size, num_threads);
    gettimeofday(&end, NULL);
    double time_taken = end.tv_sec + end.tv_usec / 1e6 -
                        start.tv_sec - start.tv_usec / 1e6; // in seconds
    return time_taken;
}

// Calculate score of sorting
int calculate_score_sort(int *array, int array_size, double execution_time)
{
    int multi_core_score = (array_size / execution_time) / (666 * 4.75 * 1.25);
    return round(multi_core_score);
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

double calculate_execution_time_prime(int digits, int num_threads)
{

    struct timeval start, end;
    gettimeofday(&start, NULL);
    multicore_processing_prime(digits, num_threads);
    gettimeofday(&end, NULL);
    double time_taken = end.tv_sec + end.tv_usec / 1e6 -
                        start.tv_sec - start.tv_usec / 1e6; // in seconds
    return time_taken;
}

int calculate_score_prime(int digits, double execution_time)
{
    int multi_core_score = (digits / execution_time) / 666;
    return round(multi_core_score);
}

double calculate_e_part(int64_t start, int64_t end)
{
    double result = 0.0;
    double term = 1.0;

    for (int64_t i = start; i <= end; ++i)
    {
        term *= 1.0 / i;
        result += term;
    }

    return result;
}

// Thread function for calculating digits of e
void *calculate_e_thread(void *_args)
{
    struct range *args = (struct range *)_args;
    args->count = calculate_e_part(args->start, args->end);

    pthread_exit(NULL);
}

// Function to calculate digits of e using multiple threads
double multicore_processing_e(int64_t iterations, int num_threads)
{
    pthread_t threads[num_threads];
    struct range *args[num_threads];
    int thread;

    // Specify start and end values, then split based on the number of threads
    int64_t start = 1;
    int64_t end = iterations;
    int64_t iterations_per_thread = iterations / num_threads;

    // Assign start and end values for each thread, then create it
    for (thread = 0; thread < num_threads; thread++)
    {
        args[thread] = calloc(sizeof(struct range), 1);
        args[thread]->start = start + (thread * iterations_per_thread);
        args[thread]->end = args[thread]->start + iterations_per_thread - 1;
        assert(pthread_create(&threads[thread], NULL, calculate_e_thread, args[thread]) == 0);
    }

    // All threads are running. Join all to collect the results.
    double total_result = 1.0; // Initial value for e
    for (thread = 0; thread < num_threads; thread++)
    {
        pthread_join(threads[thread], NULL);
        total_result += args[thread]->count;
        free(args[thread]);
    }

    return total_result;
}

double calculate_execution_time_e(int64_t digits, int num_threads)
{

    struct timeval start, end;
    gettimeofday(&start, NULL);
    multicore_processing_e(digits, num_threads);
    gettimeofday(&end, NULL);
    double time_taken = end.tv_sec + end.tv_usec / 1e6 -
                        start.tv_sec - start.tv_usec / 1e6; // in seconds
    return time_taken;
}

int calculate_score_e(int64_t digits, double execution_time)
{
    int multi_core_score = (digits / execution_time) / (666 * 377 * 1.95);
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
    int64_t digits_e = 40000000000;
    int64_t digits_prime = 50000000L;
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

#define ARRAY_SIZE 500000000
    int *array = malloc(ARRAY_SIZE * sizeof(int));
    for (int i = 0; i < ARRAY_SIZE; i++)
    {
        array[i] = rand() % 1000;
    }

    // double execution_time_single_core_e = calculate_execution_time_e(digits_e, 1);
    // double execution_time_multi_core_e = calculate_execution_time_e(digits_e, processes);
    // double execution_time_single_core_prime = calculate_execution_time_prime(digits_prime, 1);
    // double execution_time_multi_core_prime = calculate_execution_time_prime(digits_prime, processes);
    // int64_t score_single_core_e = calculate_score_e(digits_e, execution_time_single_core_e);
    // int64_t score_multi_core_e = calculate_score_e(digits_e, execution_time_multi_core_e);
    // int64_t score_single_core_prime = calculate_score_prime(digits_prime, execution_time_single_core_prime);
    // int64_t score_multi_core_prime = calculate_score_prime(digits_prime, execution_time_multi_core_prime);
    // double execution_time_single_core = (execution_time_single_core_e + execution_time_single_core_prime) / 2;
    // double execution_time_multi_core = (execution_time_multi_core_e + execution_time_multi_core_prime) / 2;
    // int64_t score_single_core = (score_single_core_e + score_single_core_prime) / 2;
    // int64_t score_multi_core = (score_multi_core_e + score_multi_core_prime) / 2;

    printf("Starting benchmark...\n");
    printf("Starting single core...\n");
    double execution_time_single_core = calculate_execution_time_sort(array, ARRAY_SIZE, 1);
    printf("Ending single core...\n");
    printf("Starting multi core...\n");
    double execution_time_multi_core = calculate_execution_time_sort(array, ARRAY_SIZE, processes);
    printf("Ending multi core...\n");
    printf("Benchmark finished.\n");
    int64_t score_single_core = calculate_score_sort(array, ARRAY_SIZE, execution_time_single_core);
    int64_t score_multi_core = calculate_score_sort(array, ARRAY_SIZE, execution_time_multi_core);

    printf("CPU Model%s", model_info);
    printf("\n");
    printf(os_display);
    printf("\n");
    printf("Number of cores: %d\n", processes);
    printf("Single core score: %ld\n", score_single_core);
    printf("Multi core score: %ld\n", score_multi_core);
    printf("Single core execution time: %lf\n", execution_time_single_core);
    printf("Multi core execution time: %lf\n", execution_time_multi_core);
    printf("Speedup: %lf\n", execution_time_single_core / execution_time_multi_core);
    printf("Efficiency: %lf\n", (execution_time_single_core / execution_time_multi_core) / processes);
    printf("CPU utilization: %lf\n", 100 - (execution_time_multi_core / execution_time_single_core) * 100);

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
        digits_prime,
        score_single_core,
        score_multi_core,
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
    const char* API_URL = "taipan-benchmarks.vercel.app";
    const char* API_PATH = "/api/cpu-benchmarks";
    const char *JSON_PAYLOAD = jsonString;
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        report_and_exit("WSAStartup failed");
    }

    // Create a TCP socket
    SOCKET sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == INVALID_SOCKET)
    {
        report_and_exit("Socket creation failed");
    }

    // Resolve the server's address
    struct hostent *server = gethostbyname(API_URL);
    if (server == NULL)
    {
        report_and_exit("Error resolving hostname");
    }

    // Set up the server address structure
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(443); // HTTPS port
    serverAddr.sin_addr.s_addr = *((unsigned long *)server->h_addr);

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        report_and_exit("Connection failed");
    }

    // Initialize OpenSSL
    SSL_library_init();
    SSL_CTX *ctx = SSL_CTX_new(SSLv23_client_method());
    if (!ctx)
    {
        report_and_exit("SSL_CTX_new error");
    }

    // Create an SSL connection
    SSL *ssl = SSL_new(ctx);
    if (!ssl)
    {
        report_and_exit("SSL_new error");
    }

    // Set up the SSL connection
    SSL_set_fd(ssl, sockfd);
    if (SSL_connect(ssl) != 1)
    {
        report_and_exit("SSL_connect error");
    }

    // Create the HTTP POST request
    char request[4096];
    sprintf(request, "POST %s HTTP/1.1\r\n"
                     "Host: %s\r\n"
                     "Content-Type: application/json\r\n"
                     "Content-Length: %ld\r\n"
                     "\r\n"
                     "%s",
            API_PATH, API_URL, strlen(JSON_PAYLOAD), JSON_PAYLOAD);

    // Send the HTTP request
    if (SSL_write(ssl, request, strlen(request)) <= 0)
    {
        report_and_exit("SSL_write error");
    }

    // Receive and print the response
    char buffer[4096];
    int bytes_read;
    while (1)
    {
        bytes_read = SSL_read(ssl, buffer, sizeof(buffer));
        if (bytes_read <= 10)
        {
            break;
        }
        printf("%.*s", bytes_read, buffer);
    }
    // Get object id from response
    char *object_id = strstr(buffer, "_id");
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
    // Clean up
    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    closesocket(sockfd);
    WSACleanup();

    return EXIT_SUCCESS;
}

void report_and_exit(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(EXIT_FAILURE);
}