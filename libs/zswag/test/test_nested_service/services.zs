package test_nested_service.services;

struct I32 {
    int32 x;
};

service MyService {
    I32 myMethod(I32);
};
