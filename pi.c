#include <stdio.h>
#include <stdlib.h>
#include <time.h>

double monte_carlo_pi(int points) {
    int inside_circle = 0;
    for (int i = 0; i < points; i++) {
        double x = (double)rand();
        double y = (double)rand();
        double distance = x*x + y*y;
        if (distance <= 1) {
            inside_circle++;
        }
    }
    return (double)inside_circle;
}

int main() {
    srand(time(NULL));
    unsigned long long points = 2e50000000000000000000000000000;
    double pi_estimate = monte_carlo_pi(points);
    printf("Pi estimate: %f\n", pi_estimate);
    return 0;
}