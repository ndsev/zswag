package service_client_test;

struct Flat {
  string role;
  string firstName;
};

struct Request {
  /* Primitive */
  string str;

  /* Array */
  int32 strLen;
  string strArray[strLen];

  /* Object */
  Flat flat;
};
