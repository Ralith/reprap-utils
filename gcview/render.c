#include <GL/gl.h>
#include <stdio.h>

#include "render.h"

void render_words(gcblock *head, point *curr) {
  point prev = *curr, offset = {0.0, 0.0, 0.0};
  char extruding = 0;
  char relative = 0;
  char moved = 0;
  
  /* Evaluate blocks sequentially */
  gcblock *block;
  for(block = head; block != NULL; block = block->next) {
    /* Evaluate all words in the block */
    size_t i;
    for(i = 0; i < block->wordcnt; ++i) {
      const gcword word = block->words[i];
      switch(word.letter) {
      case 'G':
        switch((unsigned)word.num) {
        case 0:                 /* Rapid Positioning */
          if(extruding) {
            glColor3f(1.0, 0.5, 0.0);
          } else {
            glColor3f(0.5, 0.5, 0.5);
          }
          break;
          
        case 1:                 /* Linear Interpolation */
          if(extruding) {
            glColor3f(0.0, 1.0, 0.25);
          } else {
            glColor3f(0.5, 0.5, 0.5);
          }
          break;

        case 90:                /* Absolute positioning */
          relative = 0;
          break;
        case 91:                /* Relative positioning */
          relative = 1;
          break;

        case 2:                 /* CW arc */
        case 3:                 /* CC arc */
          fprintf(stderr, "UNIMPLEMENTED: Line %d: G%d\n", block->real_line, (unsigned)word.num);
          break;

          /* Ignored */
        case 4:                 /* Dwell */
        case 20:                /* Inches (TODO: Scale) */
        case 21:                /* mm */
        case 92:                /* Set offset */
          break;

        default:
          fprintf(stderr, "WARNING: Line %d: Skipping unrecognized G code G%d\n", block->real_line, (unsigned)word.num);
          break;
        }
        break;

      case 'M':
        switch((unsigned)word.num) {
          /* Motor state */
        case 101:               /* On */
          extruding = 1;
          break;
        case 102:               /* Off */
        case 103:               /* Reverse */
          extruding = 0;
          break;

          /* Ignored */
        case 1:                 /* Interactive extruder test */
        case 6:                 /* Wait for warmup */
        case 104:               /* Set temp (TODO: color?) */
        case 108:               /* Set speed */
          break;

        default:
          fprintf(stderr, "WARNING: Line %d: Skipping unrecognized M code M%d\n", block->real_line, (unsigned)word.num);
          break;
        }
        break;

      case 'X':
        curr->x = (relative ? prev.x : 0) + word.num + offset.x;
        moved = 1;
        break;

      case 'Y':
        curr->y = (relative ? prev.y : 0) + word.num + offset.y;
        moved = 1;
        break;

      case 'Z':
        curr->z = (relative ? prev.z : 0) + word.num + offset.z;
        moved = 1;
        break;

        /* Ignored words */
      case 'F':                 /* Feedrate */
      case 'P':                 /* Param to Dwell, others? */
      case 'S':                 /* Speed (TODO: consider coloring) */
      case 'R':                 /* Param to M108 meaning what? */
      case 'T':                 /* Param to M6 (wait for warmup), others? */
        break;

      default:
        fprintf(stderr, "WARNING: Line %d: Skipping unrecognized word %c\n", block->real_line, word.letter);
        break;
      }
    }
    if(moved) {
      glBegin(GL_LINES);
      {
        glVertex3f(prev.x, prev.y, prev.z);
        glVertex3f(curr->x, curr->y, curr->z);
      }
      glEnd();
      prev = *curr;
    }
  }
}
