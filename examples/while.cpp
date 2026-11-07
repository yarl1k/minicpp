int main() {
    int value = 1;
    int counter = 0;
    while (counter < 6) {
        value = value * 2;
        counter = counter + 1;
    }
    bool isLarge = value >= 64;
    if (isLarge && value != 0) {
        return value % 10;
    } else {
        return 0;
    }
}
