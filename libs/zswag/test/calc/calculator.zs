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

struct BaseAndExponent
{
    I32 base;
    I32 exponent;
    int32 unused1;
    string unused2;
    float32 unused3;
    uint8 unused4[];
};

/*!

### Calculator Service

Check out these sweet docs.

!*/

service Calculator
{
    Double power(BaseAndExponent);
};
