import time
def calculate_pi(n):
    pi = 0
    for k in range(n):
        pi += (1 / 16 ** k) * (
            (4 / (8 * k + 1)) -
            (2 / (8 * k + 4)) -
            (1 / (8 * k + 5)) -
            (1 / (8 * k + 6))
        )
    return pi

def calculate_pi_execution_time(n):
    start_time = time.time()
    calculate_pi(n)
    end_time = time.time()
    execution_time = end_time - start_time
    return execution_time

def calculate_single_core_score(n, execution_time):
    single_core_score = (n / execution_time) / 2
    return single_core_score

def main():
    n = int(input("Enter the number of iterations: "))
    execution_time = calculate_pi_execution_time(n)
    print(f"Execution time for n = {n} is {execution_time}")
    print(f"Single core score for n = {n} is {calculate_single_core_score(n, execution_time)}")
    
if __name__ == "__main__":
    main()