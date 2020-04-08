package calculator;

struct I32
{
    int32 value;
};

struct U64
{
    uint64 value;
};

struct Double
{
    float64 value;
};

service Calculator
{
    U64 powerOfTwo(I32);
    Double squareRoot(Double);
};
