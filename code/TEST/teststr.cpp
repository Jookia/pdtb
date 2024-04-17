#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <assert.h>
#include <string.h>

#pragma pack (push, 1)
struct builderData {
  unsigned short inout; /* ax */
  unsigned short len_read; /* bx */
  unsigned short len_write; /* cx */
  char* read; /* si */
  char* write; /* di */
};
#pragma pack (pop)
typedef void (*builderFunc)(void);

int callBuilderFunction(struct builderData *data, builderFunc func);
#pragma aux callBuilderFunction = \
  "push bp" \
  "mov bp, bx" \
  "mov ax, [bp+0]" \
  "mov bx, [bp+2]" \
  "mov cx, [bp+4]" \
  "mov si, [bp+6]" \
  "mov di, [bp+8]" \
  "call dx" \
  "mov [bp+0], ax" \
  "mov [bp+2], bx" \
  "mov [bp+4], cx" \
  "mov [bp+6], si" \
  "mov [bp+8], di" \
  "mov bx, bp" \
  "pop bp" \
  "pushf" \
  "pop ax" \
  "and ax, 1" \
  modify [bx cx si di bp] \
  parm [bx] [dx] \
  value [ax]

#define _ASSERT_INT(val, expected, file, line, EXTRA) \
  if(val != expected) { \
    printf("Assertion failed: " #val " != %i, got: %i\n" \
           "Error at %s:%i\n", expected, val, file, line); \
    EXTRA \
    exit(1); \
  }
#define ASSERT_INT(val, expected) _ASSERT_INT(val, expected, __FILE__ , __LINE__, {})
#define DATA_ASSERT_INT(data, val, expected) _ASSERT_INT(val, expected, __FILE__ , __LINE__, { \
  printData(&data); \
})
#define _ASSERT_STRNCMP(a, b, len, file, line, EXTRA) \
  if(strncmp(a, b, len) != 0) { \
    printf("String comparison failed: '%s' != '%s'\n" \
           "Error at %s:%i\n", a, b, file, line); \
    EXTRA \
    exit(1); \
  }
#define ASSERT_STRNCMP(a, b, len) _ASSERT_STRNCMP(a, b, len, __FILE__ , __LINE__, {})
#define DATA_ASSERT_STRNCMP(data, a, b, len) _ASSERT_STRNCMP(a, b, len, __FILE__ , __LINE__, { \
  printData(&data); \
})

#define STRING_TEST(name, in_str, body) \
  void name##(void) { \
    char* in = in_str; \
    char out[16] = {0}; \
    struct builderData data_in; \
    data_in.inout = 0; \
    data_in.len_read = strlen(in); \
    data_in.len_write = sizeof(out) / sizeof(*out); \
    data_in.read = in; \
    data_in.write = out; \
    struct builderData data_out = data_in; \
    body \
  }
#define _CHECK_STRING_FUNC(func, ret_val, file, line) \
  int ret = callBuilderFunction(&data_in, (builderFunc)func); \
  _ASSERT_INT(ret, ret_val, file, line, {}); \
  _ASSERT_INT(data_in.inout, data_out.inout, file, line, {}); \
  _ASSERT_INT(data_in.len_read, data_out.len_read, file, line, {}); \
  _ASSERT_INT(data_in.len_write, data_out.len_write, file, line, {}); \
  _ASSERT_INT(data_in.read, data_out.read, file, line, {}); \
  _ASSERT_INT(data_in.write, data_out.write, file, line, {});
#define CHECK_STRING_FUNC(func, ret_val) _CHECK_STRING_FUNC(func, ret_val, __FILE__ , __LINE__)

// Used for packing the AH and AL register
#define AH_AL(a, b) (a << 8) + b

#pragma aux writeChar "writeChar"
#pragma aux writeStr "writeStr"
#pragma aux writeNum "writeNum"
#pragma aux writeNumSigned "writeNumSigned"

// Writes a single character to a string
STRING_TEST(testWriteChar_normal, "",
  data_in.inout = data_out.inout = AH_AL(0, 'H');
  data_out.write += 1;
  data_out.len_write -= 1;
  CHECK_STRING_FUNC(writeChar, 0);
  ASSERT_INT(out[0], 'H');
)

// Writes a single character to a full string
STRING_TEST(testWriteChar_full, "",
  data_in.inout = data_out.inout = AH_AL(0, 'H');
  data_in.len_write = data_out.len_write = 0;
  CHECK_STRING_FUNC(writeChar, 0);
  ASSERT_INT(out[0], 0);
)

// Writes a string to a string
STRING_TEST(testWriteStr_normal, "Hello",
  data_out.len_read = 0;
  data_out.len_write -= data_in.len_read;
  data_out.write += data_in.len_read;
  data_out.read += data_in.len_read;
  CHECK_STRING_FUNC(writeStr, 0);
  ASSERT_STRNCMP(data_in.write, data_in.read, data_in.len_read);
)

// Writes an empty string to a string
STRING_TEST(testWriteStr_emptyInput, "",
  CHECK_STRING_FUNC(writeStr, 0);
  ASSERT_INT(out[0], 0);
)

// Writes a string to an empty string
STRING_TEST(testWriteStr_emptyOutput, "Hello",
  data_in.len_write = data_out.len_write = 0;
  CHECK_STRING_FUNC(writeStr, 0);
  ASSERT_INT(out[0], 0);
)

// Writes a long string to a small string
STRING_TEST(testWriteStr_tooLong, "Hello world this is a very long string",
  data_out.read += data_in.len_write;
  data_out.write += data_in.len_write;
  data_out.len_read = 0;
  data_out.len_write = 0;
  CHECK_STRING_FUNC(writeStr, 0);
  ASSERT_STRNCMP(data_in.write, data_in.read, data_in.len_write);
)

// Check that we can write a zero correctly
STRING_TEST(testWriteNum_0, "",
  data_in.inout = 0;
  data_in.read = data_out.read = 0;
  data_in.len_read = 0;
  data_out.write += 1;
  data_out.len_read = 0;
  data_out.len_write -= 1;
  CHECK_STRING_FUNC(writeNum, 0);
  ASSERT_INT(out[0], '0');
)

// Check that we can write a 1 correctly
STRING_TEST(testWriteNum_1, "",
  data_in.inout = 1;
  data_in.read = data_out.read = 0;
  data_in.len_read = 0;
  data_out.write += 1;
  data_out.len_read = 0;
  data_out.len_write -= 1;
  CHECK_STRING_FUNC(writeNum, 0);
  ASSERT_INT(out[0], '1');
)

// Check that we can write a multi-digit number
STRING_TEST(testWriteNum_12345, "",
  data_in.inout = 12345;
  data_in.len_read = 0;
  data_in.read = data_out.read = 0;
  data_out.write += 5;
  data_out.len_read = 0;
  data_out.len_write -= 5;
  CHECK_STRING_FUNC(writeNum, 0);
  ASSERT_STRNCMP(out, "12345", 5);
)

// Check that we can pad a number
STRING_TEST(testWriteNum_00345, "",
  data_in.inout = 345;
  data_in.len_read = 5;
  data_in.read = data_out.read = 0;
  data_out.write += 5;
  data_out.len_read = 0;
  data_out.len_write -= 5;
  CHECK_STRING_FUNC(writeNum, 0);
  ASSERT_STRNCMP(out, "00345", 5);
)

// Check that we can pad a number partially
STRING_TEST(testWriteNum_0345, "",
  data_in.inout = 345;
  data_in.len_read = 4;
  data_in.read = data_out.read = 0;
  data_out.write += 4;
  data_out.len_read = 0;
  data_out.len_write -= 4;
  CHECK_STRING_FUNC(writeNum, 0);
  ASSERT_STRNCMP(out, "0345", 5);
)

// Check that we don't unnecessarily pad a number
STRING_TEST(testWriteNum_345, "",
  data_in.inout = 345;
  data_in.len_read = 2;
  data_in.read = data_out.read = 0;
  data_out.write += 3;
  data_out.len_read = 0;
  data_out.len_write -= 3;
  CHECK_STRING_FUNC(writeNum, 0);
  ASSERT_STRNCMP(out, "345", 5);
)

// Check that we can write a positive number
STRING_TEST(testWriteNumSigned_012, "",
  data_in.inout = 12;
  data_in.len_read = 3;
  data_in.read = data_out.read = 0;
  data_out.write += 3;
  data_out.len_read = 0;
  data_out.len_write -= 3;
  CHECK_STRING_FUNC(writeNumSigned, 0);
  ASSERT_STRNCMP(out, "012", 5);
)

// Check that we can write a negative number
STRING_TEST(testWriteNumSigned_n012, "",
  data_in.inout = -12;
  data_in.len_read = 3;
  data_in.read = data_out.read = 0;
  data_out.write += 4;
  data_out.len_read = 0;
  data_out.len_write -= 4;
  CHECK_STRING_FUNC(writeNumSigned, 0);
  ASSERT_STRNCMP(out, "-012", 5);
)

int main(void) {
  testWriteChar_normal();
  testWriteChar_full();
  testWriteStr_normal();
  testWriteStr_emptyInput();
  testWriteStr_tooLong();
  testWriteNum_0();
  testWriteNum_1();
  testWriteNum_12345();
  testWriteNum_00345();
  testWriteNum_0345();
  testWriteNum_345();
  testWriteNumSigned_012();
  testWriteNumSigned_n012();
  return 0;
}
