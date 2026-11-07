int calculate(int x)
{
    return x * 2;
}

void printResult(int x)
{
    int result = calculate(x);
    return;
}

int main()
{
    printResult(10);
    return calculate(5);
}