// Names that are special in Python must remain safe after translation.
int _cxx_div(int class, int None)
{
    return class + None;
}

int __minicpp_runtime_cxx_div(int value)
{
    return value + 10;
}

double readPrecision()
{
    return 123.456789;
}

int main()
{
    int print = _cxx_div(3, 4);
    int helper = __minicpp_runtime_cxx_div(1);
    double value = readPrecision();
    bool precise = value > 123.456780;

    if (precise && print == 7 && helper == 11)
    {
        return 42;
    }

    return 0;
}
