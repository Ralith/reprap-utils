#include <string.h>
#include <stdlib.h>

#include "gcode.h"

/* Finds the next non-whitespace character. */
int next_dark(char *buffer, size_t len, size_t i) {
  for(; i < len; ++i) {
    switch(buffer[i]) {
    case ' ':
    case '\t':
    case '\r':
    case '\n':
      break;
      
    default:
      return i;
    }
  }
  return len;
}

gcblock *parse_block(char *buffer, unsigned len) {
  size_t i = next_dark(buffer, len, 0);
  if(i == len) {
    return NULL;
  }

  gcblock *block = malloc(sizeof(gcblock));
  block->next = NULL;
  block->line = 0;
  block->optdelete = 0;
  block->words = NULL;
  block->wordcnt = 0;

  /* Check for optional delete */
  if(buffer[i] == '/') {
    block->optdelete = 1;
    i = next_dark(buffer, len, i);
  }

  /* Check for line number */
  if((buffer[i] == 'N' || buffer[i] == 'n') && (i + 1) < len) {
    i = next_dark(buffer, len, len);
    char* endptr;
    block->line = strtol(buffer + i, &endptr, 10);
    i = next_dark(buffer, len, (buffer - endptr));
  }

  /* Parse words */
  unsigned allocsize = 0;
  for(; i < len; i = next_dark(buffer, len, i)) {
    if(block->wordcnt == allocsize) {
      block->words = realloc(block->words, 2*(allocsize ? allocsize : 4));
    }
    
    block->words[block->wordcnt].letter = buffer[i];
    i = next_dark(buffer, len, i);
    if(i == len) {
      free(block->words);
      free(block);
      return NULL;
    }

    /* TODO: Spaces in numbers */
    /* TODO: Gn.m support */
    {
      float fnum;
      int inum;
      char *endptr;
      inum = strtol(buffer + i, &endptr, 10);
      
      if(*endptr == '.') {
        fnum = strtof(buffer + i, &endptr);
        block->words[block->wordcnt].fnum = fnum;
      } else {
        block->words[block->wordcnt].inum = inum;
      }
      
      i = next_dark(buffer, len, (buffer - endptr));
      ++(block->wordcnt);
    }
  }

  return block;
}
