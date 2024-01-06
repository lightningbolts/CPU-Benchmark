#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <assert.h>

#define ARRAY_SIZE 500000000
// Structure to pass arguments to the thread function
struct range
{
    int *array;
    int start;
    int end;
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
    struct range *args = (struct range *)_args;
    merge_sort(args->array, args->start, args->end);

    pthread_exit(NULL);
}

// Function to sort an array using multiple threads
void multicore_processing_sort(int *array, int array_size, int num_threads)
{
    pthread_t threads[num_threads];
    struct range args[num_threads];
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
    int multi_core_score = (array_size / execution_time) / (666 * 4.75);
    return round(multi_core_score);
}

// Function to print an array
void print_array(int *array, int size)
{
    for (int i = 0; i < size; i++)
    {
        printf("%d ", array[i]);
    }
    printf("\n");
}

int main()
{
    // Create an array to be sorted
    int *array = malloc(ARRAY_SIZE * sizeof(int));
    for (int i = 0; i < ARRAY_SIZE; i++)
    {
        array[i] = rand() % 1000;
    }

    // Print the original array
    printf("Original array:\n");
    // print_array(array, ARRAY_SIZE);

    int num_threads = 24; // Number of threads to use

    // Calculate execution time of sorting
    double execution_time = calculate_execution_time_sort(array, ARRAY_SIZE, num_threads);
    printf("Execution time: %f seconds\n", execution_time);
    int64_t score = calculate_score_sort(array, ARRAY_SIZE, execution_time);
    printf("Score: %ld\n", score);

    free(array);

    return 0;
}
