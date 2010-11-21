#define GCODE_BLOCKSIZE 256

typedef struct gcword {
  char letter;
  float num;
} gcword;

typedef struct gcblock {
  struct gcblock *next;
  char optdelete;
  unsigned line, real_line, index;
  gcword *words;
  unsigned wordcnt;
} gcblock;

gcblock *parse_block(char *buffer, unsigned len);  





