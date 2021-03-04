package calculator;

struct I32
{
    int32 value;
};

struct U64
{
    uint64 value;
};

/*!

### This type has documentation

!*/

struct Double
{
    float64 value;
};

/*!

### Calculator Service

Check out these sweet docs.

!*/

service Calculator
{
    U64 powerOfTwo(I32);

    /** This method has a docstring on top of it. */
    Double squareRoot(Double);
};
