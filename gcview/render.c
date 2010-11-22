#include <GL/gl.h>
#include <stdio.h>

#include "render.h"

void render_words(gcblock *head) {
  point curr = {0.0, 0.0, 0.0}, next = {0.0, 0.0, 0.0}, peek = {0.0, 0.0, 0.0};
  char extruding = 0;
  char relative = 0;
  char moved = 0;
  char statechange = 0;        /* Force line drawing */
  
  /* Evaluate blocks sequentially */
  glBegin(GL_LINE_STRIP);
  /*glVertex3f(peek.x, peek.y, peek.z);*/
  glVertex3f(curr.x, curr.y, curr.z);
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
          /* Extruder motor state */
        case 101:               /* On */
          extruding = 1;
          statechange = 1;
          break;
        case 102:               /* Off */
        case 103:               /* Reverse */
          extruding = 0;
          statechange = 1;
          break;

          /* Ignored */
        case 1:                 /* Interactive extruder test */
        case 6:                 /* Wait for warmup */
        case 104:               /* Set temp (slow) (TODO: color?) */
        case 105:               /* Get temp */
        case 106:               /* Fan on (color?) */
        case 107:               /* Fan off (color?) */
        case 108:               /* Set speed */
        case 109:               /* Set temp (slow) (color?) */
        case 113:               /* Extruder PWM */
          break;

        default:
          fprintf(stderr, "WARNING: Line %d: Skipping unrecognized M code M%d\n", block->real_line, (unsigned)word.num);
          break;
        }
        break;

      case 'X':
        peek.x = (relative ? peek.x : 0) + word.num;
        moved = 1;
        break;

      case 'Y':
        peek.y = (relative ? peek.y : 0) + word.num;
        moved = 1;
        break;

      case 'Z':
        peek.z = (relative ? peek.z : 0) + word.num;
        moved = 1;
        break;

        /* Ignored words */
      case 'F':                 /* Feedrate */
      case 'P':                 /* Param to Dwell, others? */
      case 'S':                 /* Speed (TODO: consider coloring) */
      case 'R':                 /* Param to M108 meaning what? */
      case 'T':                 /* Param to M6 (wait for warmup), others? */
      case 'E':                 /* Extrude length */
        break;

      default:
        fprintf(stderr, "WARNING: Line %d: Skipping unrecognized word %c\n", block->real_line, word.letter);
        break;
      }
    }
    /* Contiguous line simplification; interferes with coloring */
    /* const point nextdelta = {next.x - peek.x, next.y - peek.y, next.z - peek.z}; */
    /* const point currdelta = {curr.x - next.x, curr.y - next.y, curr.z - next.z}; */
    /* const float dtheta = angle(currdelta, nextdelta); */
    if(moved) {
      glVertex3f(next.x, next.y, next.z);
      curr = next;
    }
    next = peek;
  }
  glVertex3f(peek.x, peek.y, peek.z);
  glEnd();
}
