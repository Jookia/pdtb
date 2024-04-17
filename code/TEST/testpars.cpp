#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <assert.h>
#include <string.h>

#pragma pack (push, 1)
struct parserData {
  unsigned short inout;
  unsigned short len_read;
  unsigned short len_write;
  char* read;
  char* write;
};
#pragma pack (pop)
typedef void (*parseFunc)(void);

int callParseFunction(struct parserData *data, parseFunc func);
#pragma aux callParseFunction = \
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

#pragma aux peek "peek"
#pragma aux read "read"
#pragma aux copy "copy"
#pragma aux copyWord "copyWord"
#pragma aux skipSpace "skipSpace"
#pragma aux parseWordIf "parseWordIf"

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

#define PARSE_TEST(name, in_str, body) \
  void name##(void) { \
    char* in = in_str; \
    char out[16] = {0}; \
    struct parserData data_in; \
    data_in.inout = 0; \
    data_in.len_read = strlen(in); \
    data_in.len_write = sizeof(out) / sizeof(*out); \
    data_in.read = in; \
    data_in.write = out; \
    struct parserData data_out = data_in; \
    body \
  }
#define _CHECK_PARSE_FUNC(func, ret_val, file, line) \
  int ret = callParseFunction(&data_in, (parseFunc)func); \
  _ASSERT_INT(ret, ret_val, file, line, {}); \
  _ASSERT_INT(data_in.inout, data_out.inout, file, line, {}); \
  _ASSERT_INT(data_in.len_read, data_out.len_read, file, line, {}); \
  _ASSERT_INT(data_in.len_write, data_out.len_write, file, line, {}); \
  _ASSERT_INT(data_in.read, data_out.read, file, line, {}); \
  _ASSERT_INT(data_in.write, data_out.write, file, line, {});
#define CHECK_PARSE_FUNC(func, ret_val) _CHECK_PARSE_FUNC(func, ret_val, __FILE__ , __LINE__)

// Reads a single character in to inout
PARSE_TEST(testPeek_normal, "Hello",
  data_out.inout = (char)'H';
  CHECK_PARSE_FUNC(peek, 0);
  ASSERT_INT(out[0], 0);
)

// Fails if there's no characters left
PARSE_TEST(testPeek_nospace, "Hello",
  data_in.len_read = 0;
  data_out.len_read = 0;
  CHECK_PARSE_FUNC(peek, 1);
  ASSERT_INT(out[0], 0);
)

// Reads a single character in to inout, decrementing len_read
PARSE_TEST(testRead_normal, "Hello",
  data_out.len_read -= 1;
  data_out.read += 1;
  data_out.inout = (char)'H';
  CHECK_PARSE_FUNC(read, 0);
  ASSERT_INT(out[0], 0);
)

// Error if there's no space to read
PARSE_TEST(testRead_nospace, "Hello",
  data_in.len_read = 0;
  data_out.len_read = 0;
  CHECK_PARSE_FUNC(read, 1);
  ASSERT_INT(out[0], 0);
)

// Copies a single character from read to write
PARSE_TEST(testCopy_normal, "Hello",
  data_out.len_read -= 1;
  data_out.len_write -= 1;
  data_out.read += 1;
  data_out.write += 1;
  data_out.inout = 'H';
  CHECK_PARSE_FUNC(copy, 0);
  ASSERT_INT(out[0], 'H');
)

// Error if there's no space to read
PARSE_TEST(testCopy_nospace_read, "Hello",
  data_in.len_read = 0;
  data_out.len_read = 0;
  CHECK_PARSE_FUNC(copy, 1);
  ASSERT_INT(out[0], 0);
)

// Error if there's no space to write
PARSE_TEST(testCopy_nospace_write, "Hello",
  data_in.len_write = 0;
  data_out.len_write = 0;
  CHECK_PARSE_FUNC(copy, 1);
  ASSERT_INT(out[0], 0);
)

// Copies a word from read to write
PARSE_TEST(testCopyWord_normal, "Hello World",
  const char *word = "Hello";
  int word_len = strlen(word);
  data_out.len_read -= word_len;
  data_out.len_write -= word_len;
  data_out.read += word_len;
  data_out.write += word_len;
  data_out.inout = ' ';
  CHECK_PARSE_FUNC(copyWord, 0);
  ASSERT_STRNCMP(out, word, word_len);
)

// Copies no word if we start with a space
PARSE_TEST(testCopyWord_space, " Hello World",
  const char *word = "Hello";
  data_out.inout = ' ';
  CHECK_PARSE_FUNC(copyWord, 0);
  ASSERT_INT(out[0], 0);
)

// Skips a sequence of spaces
PARSE_TEST(testSkipSpace_normal, "   Hello",
  data_out.len_read -= 3;
  data_out.read += 3;
  data_out.inout = 'H';
  CHECK_PARSE_FUNC(skipSpace, 0);
  ASSERT_INT(out[0], 0);
)

// Skips no spaces
PARSE_TEST(testSkipSpace_nospace, "Hello",
  data_out.inout = 'H';
  CHECK_PARSE_FUNC(skipSpace, 0);
  ASSERT_INT(out[0], 0);
)

// Used for packing the AH and AL register
#define AH_AL(a, b) (a << 8) + b

// Optionally skips a word
PARSE_TEST(testParseWordIf_normal, "@Hello   world",
  const char *word = "Hello";
  int word_len = strlen(word);
  data_in.inout = AH_AL('@', 0);
  data_out.len_read -= 1 + word_len + 3;
  data_out.len_write -= word_len;
  data_out.read += 1 + word_len + 3;
  data_out.write += word_len;
  data_out.inout = AH_AL('@', 'w');
  CHECK_PARSE_FUNC(parseWordIf, 0);
  ASSERT_STRNCMP(out, word, word_len);
)

// Skips nothing
PARSE_TEST(testParseWordIf_noprefix, "Hello  world",
  data_in.inout = AH_AL('@', 0);
  data_out.inout = AH_AL('@', 'H');
  CHECK_PARSE_FUNC(parseWordIf, 0);
)

#pragma aux doParse "doParse"
int callDoParse(char *in, int len);
#pragma aux callDoParse = \
  "call doParse" \
  "pushf" \
  "pop cx" \
  "and cx, 1" \
  parm [si] [cx] \
  value [cx]

#pragma aux tBuffer "tagsBuffer"
#pragma aux tBufferLen "tagsBufferLen"
extern char *tagsBuffer = (char*)tBuffer;
extern int *tagsBufferLen = (int*)tBufferLen;

#pragma aux pBuffer "prefixBuffer"
#pragma aux pBufferLen "prefixBufferLen"
extern char *prefixBuffer = (char*)pBuffer;
extern int *prefixBufferLen = (int*)pBufferLen;

#pragma aux cBuffer "cmdBuffer"
#pragma aux cBufferLen "cmdBufferLen"
extern char *cmdBuffer = (char*)cBuffer;
extern int *cmdBufferLen = (int*)cBufferLen;

// Stock test data

struct testData {
  char *tags;
  char *prefix;
  char *cmd;
  char *msg;
};

struct testData normals[] = {
  {NULL,   NULL,     "COMAND", "COMAND \r"},
  {"TAGS", NULL,     "COMAND", "@TAGS  COMAND \r"},
  {NULL,   "PREFIX", "COMAND", ":PREFIX  COMAND \r"},
  {"TAGS", "PREFIX", "COMAND", "@TAGS :PREFIX  COMAND \r"},
};

void printData(struct testData *data) {
  printf("TEST REPORT:\ntags: %s\nprefix: %s\ncmd: %s\nmsg: %s\n",
    data->tags, data->prefix, data->cmd, data->msg);
}

void testParse_normal(struct testData data) {
  char *in = data.msg;
  int len = strlen(in);
  int ret = callDoParse(in, len);
  DATA_ASSERT_INT(data, ret, 0);
  if(data.tags) {
    int len = strlen(data.tags);
    DATA_ASSERT_INT(data, *tagsBufferLen, len);
    DATA_ASSERT_STRNCMP(data, tagsBuffer, data.tags, len);
  } else {
    DATA_ASSERT_INT(data, *tagsBufferLen, 0);
  }
  if(data.prefix) {
    int len = strlen(data.prefix);
    DATA_ASSERT_INT(data, *prefixBufferLen, len);
    DATA_ASSERT_STRNCMP(data, prefixBuffer, data.prefix, len);
  } else {
    DATA_ASSERT_INT(data, *prefixBufferLen, 0);
  }
  // command
  {
    int len = strlen(data.cmd);
    DATA_ASSERT_INT(data, *cmdBufferLen, len);
    DATA_ASSERT_STRNCMP(data, cmdBuffer, data.cmd, len);
  }
}

void testParse_normals(void) {
  int count = (sizeof(normals) / sizeof(normals[0]));
  for(int i = 0; i < count; ++i) {
    testParse_normal(normals[i]);
  }
}

// Tests we get an error if the tags buffer overflows
void testParse_tagsOverflow(void) {
  int size = 520;
  char *msg = (char*)malloc(size);
  if(msg == NULL) {
    printf("Couldn't alloc %i\n", msg);
    exit(1);
  }
  memset(msg, 'A', size);
  msg[0] = '@';
  msg[514] = ' ';
  int ret = callDoParse(msg, size);
  ASSERT_INT(ret, 1);
  free(msg);
}

// Tests we get an error if we omit an '\r'
void testParse_noR(void) {
  int size = 520;
  char *msg = "COMMAND \r";
  int ret = callDoParse(msg, strlen(msg));
  ASSERT_INT(ret, 0);
  ret = callDoParse(msg, strlen(msg) - 1);
  ASSERT_INT(ret, 1);
}

int main(void) {
  testPeek_normal();
  testPeek_nospace();
  testRead_normal();
  testRead_nospace();
  testCopy_normal();
  testCopy_nospace_read();
  testCopy_nospace_write();
  testCopyWord_normal();
  testCopyWord_space();
  testSkipSpace_normal();
  testSkipSpace_nospace();
  testParseWordIf_normal();
  testParseWordIf_noprefix();
  testParse_normals();
  testParse_tagsOverflow();
  testParse_noR();
  return 0;
}
