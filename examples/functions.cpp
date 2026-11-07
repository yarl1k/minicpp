double average(int a, int b) {
    return (a + b) / 2.0;
}

int main() {
    double value = average(7, 12);
    bool passed = value >= 9.0;
    if (passed) {
        return 1;
    }
    return 0;
}
