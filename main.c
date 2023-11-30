
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

double calculatePi(int n) {
    double pi = 0.0;
    #pragma omp parallel for reduction(+:pi)
    for (int k = 0; k <= n; k++) {
        double a = 1.0 / pow(16, k);
        double b = 4.0 / (8 * k + 1);
        double c = 2.0 / (8 * k + 4);
        double d = 1.0 / (8 * k + 5);
        double e = 1.0 / (8 * k + 6);
        pi += a * (b - c - d - e);
    }
    return pi;
}

int main() {
    int n;
    printf("Enter the number of digits to calculate pi: ");
    scanf("%d", &n);
    double pi = calculatePi(n);
    printf("Pi to %d digits: %.15f\n", n, pi);
    return 0;
}

