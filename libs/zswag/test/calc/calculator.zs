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

struct String
{
    string value;
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

struct Strings
{
    string values[];
};

struct Doubles
{
    float64 values[];
};

struct Bools
{
    bool values[];
};

/*!

### Calculator Service

Check out these sweet docs.

!*/

service Calculator
{
    Double power(BaseAndExponent);

    Double isum(Integers);

    Double bsum(Bytes);

    Double imul(Integers);

    Double fmul(Doubles);

    Bool bmul(Bools);

    Double identity(Double);

    String concat(Strings);
};
