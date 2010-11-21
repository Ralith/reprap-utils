typedef struct point {
  float x, y, z;
} point;

typedef struct gcword {
  char letter;
  union {
    int inum;
    float fnum;
  };
} gcword;

typedef struct gcblock {
  struct gcblock *next;
  char optdelete;
  unsigned line;
  gcword *words;
  unsigned wordslen;
} gcblock;

gcblock *parse_block(char *buffer, unsigned len);
  
