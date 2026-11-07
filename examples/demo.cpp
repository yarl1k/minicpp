// Recursive function + for loop + double + bool + logical operators.
int factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

int main() {
    int sum = 0;
    for (int i = 1; i <= 5; i++) {
        sum = sum + i;
    }

    double ratio = 10.0 / 4.0;
    bool ready = ratio > 2.0 && sum == 15;

    if (ready) {
        return factorial(5);
    } else {
        return 0;
    }
}
