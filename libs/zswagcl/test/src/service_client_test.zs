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

// ============================================================================
// New types for array and complex type testing
// ============================================================================

// Enum for testing
enum uint8 Status {
  ACTIVE = 0,
  INACTIVE = 1,
  PENDING = 2
};

// Bitmask for testing
bitmask uint16 Permissions {
  PERM_READ = 0,
  PERM_WRITE = 1,
  PERM_EXECUTE = 2,
  PERM_DELETE = 3
};

// Struct for complex type testing
struct Address {
  string street;
  uint32 zipCode;
};

// Choice type for testing
choice ColorChoice(bool useRgb) on useRgb {
  case true:
    uint32 rgb;
  case false:
    string name;
};

// Union for testing
union SimpleUnion {
  int32 intValue;
  string stringValue;
  float32 floatValue;
};

// Request with all primitive array types
struct ArrayTestRequest {
  // Boolean array
  uint32 boolArrayLen;
  bool boolArray[boolArrayLen];

  // Signed integer arrays
  uint32 int8ArrayLen;
  int8 int8Array[int8ArrayLen];

  uint32 int16ArrayLen;
  int16 int16Array[int16ArrayLen];

  uint32 int32ArrayLen;
  int32 int32Array[int32ArrayLen];

  uint32 int64ArrayLen;
  int64 int64Array[int64ArrayLen];

  // Unsigned integer arrays
  uint32 uint8ArrayLen;
  uint8 uint8Array[uint8ArrayLen];

  uint32 uint16ArrayLen;
  uint16 uint16Array[uint16ArrayLen];

  uint32 uint32ArrayLen;
  uint32 uint32Array[uint32ArrayLen];

  uint32 uint64ArrayLen;
  uint64 uint64Array[uint64ArrayLen];

  // Floating point arrays
  uint32 floatArrayLen;
  float32 floatArray[floatArrayLen];

  uint32 doubleArrayLen;
  float64 doubleArray[doubleArrayLen];

  // String array (already exists in Request but adding here for completeness)
  uint32 stringArrayLen;
  string stringArray[stringArrayLen];
};

// Request with complex type arrays
struct ComplexArrayTestRequest {
  // Bytes array
  uint32 bytesArrayLen;
  bytes bytesArray[bytesArrayLen];

  // Struct array
  uint32 structArrayLen;
  Address structArray[structArrayLen];

  // Enum array
  uint32 enumArrayLen;
  Status enumArray[enumArrayLen];

  // Bitmask array
  uint32 bitmaskArrayLen;
  Permissions bitmaskArray[bitmaskArrayLen];
};

// Request with single complex values
struct ComplexValueTestRequest {
  bytes singleBytes;
  Address singleStruct;
  Status singleEnum;
  Permissions singleBitmask;
};

// Extern type for BIT_BUFFER testing
struct ExternTestRequest {
  extern singleExtern;
  uint32 externArrayLen;
  extern externArray[externArrayLen];
};
