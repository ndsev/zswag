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

enum int32 Enum
{
    TEST_ENUM_0 = 42,
    TEST_ENUM_1 = 1,
    TEST_ENUM_2 = 2
};

struct EnumWrapper
{
    Enum value;
};

/*!

### Calculator Service

Check out these sweet docs.

!*/

service Calculator
{
    Double power(BaseAndExponent);

    Double intSum(Integers);

    Double byteSum(Bytes);

    Double intMul(Integers);

    Double floatMul(Doubles);

    Bool bitMul(Bools);

    Double identity(Double);

    String concat(Strings);

    String name(EnumWrapper);
};
