package calculator;

/*!

### This type has documentation

!*/

struct I32
{
    int32 value;
};

struct Double
{
    float64 value;
};

struct Bool
{
    bool value;
};

struct BaseAndExponent
{
    I32 base;
    I32 exponent;
    int32 unused1;
    string unused2;
    float32 unused3;
    bool unused5[];
};

struct Integers
{
    int32 values[];
};

struct Bytes
{
    uint8 values[];
};

/*!

### Calculator Service

Check out these sweet docs.

!*/

service Calculator
{
    Double power(BaseAndExponent);

    Double isum(Integers);

    Double imul(Integers);

    Double bsum(Bytes);

    Double identity(Double);
};
