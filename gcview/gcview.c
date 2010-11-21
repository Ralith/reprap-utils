#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <GL/gl.h>
#include <GL/glut.h>

#include "../common/gcode.h"

#define DEFAULT_W 640
#define DEFAULT_H 480

#define FRAME_DELAY 17          /* 1/(17ms) = about 60FPS */

#define MOTION_INCREMENT M_PI

typedef struct {
  float x, y, z;
} point;

GLuint dlist;                   /* Display list pointer */
int gcsource;                   /* FD we're reading gcode from */
fd_set fdset;

GLfloat *camtransform;

struct {
  float latitude;
  float longitude;
  float radius;
} camera;

void storexform(GLfloat *matrix, float latitude, float longitude, float radius) {
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  {
    glLoadIdentity();
    
    glTranslatef(0.0f, 0.0f, -camera.radius);
    glRotatef(-camera.latitude, 1, 0, 0);
    glRotatef(-camera.longitude, 0, 1, 0);

    glGetFloatv(GL_MODELVIEW_MATRIX, matrix);
  }
  glPopMatrix();
}

void updatecam() {
  storexform(camtransform, camera.latitude, camera.longitude, camera.radius);
}

/* Draw the current state of affairs */
void draw(int ignored) {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();
  
  glLoadMatrixf(camtransform);
  
  glCallList(dlist);

  glutSwapBuffers();

  /* TODO: Adjust timing for draw delay */
  glutTimerFunc(FRAME_DELAY, draw, 0);
}

void update(gcblock *head) {
  char relative = 0;
  char extruding = 0;
  point prev = {0.0, 0.0, 0.0}, curr = {0.0, 0.0, 0.0}, offset = {0.0, 0.0, 0.0};
  
  if(glIsList(dlist)) {
    glDeleteLists(dlist, 1);
  }
  dlist = glGenLists(1);
  glNewList(dlist, GL_COMPILE);

  /* Evaluate blocks sequentially */
  gcblock *block;
  for(block = head; block != NULL; block = block->next) {
    /* Evaluate all words in the block */
    size_t i;
    for(i = 0; i < block->wordcnt; ++i) {
      const gcword word = block->words[i];
      switch(word.letter) {
      case 'G':
      case 'g':
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

        case 2:                 /* CW arc */
        case 3:                 /* CC arc */
        case 4:                 /* Dwell */
        case 92:                /* Set offset */
          fprintf(stderr, "UNIMPLEMENTED: G%d\n", (unsigned)word.num);
          break;

        default:
          fprintf(stderr, "WARNING: Skipping unrecognized G code G%d\n", (unsigned)word.num);
          break;
        }
        break;

      case 'M':
      case 'm':
        switch((unsigned)word.num) {
        case 101:
          extruding = 1;
          break;

        case 102:
        case 103:
          extruding = 0;
          break;
        }
        break;

      case 'X':
      case 'x':
        curr.x = (relative ? prev.x : 0) + word.num + offset.x;
        break;

      case 'Y':
      case 'y':
        curr.y = (relative ? prev.y : 0) + word.num + offset.y;
        break;

      case 'Z':
      case 'z':
        curr.z = (relative ? prev.z : 0) + word.num + offset.z;
        break;

        /* Ignored words */
      case 'F':
        break;

      default:
        fprintf(stderr, "WARNING: Skipping unrecognized word %c\n", word.letter);
        break;
      }
    }
    if(curr.x != prev.x || curr.y != prev.y || curr.z != prev.z) {
      glBegin(GL_LINES);
      {
        glVertex3f(prev.x, prev.y, prev.z);
        glVertex3f(curr.x, curr.y, curr.z);
      }
      glEnd();
      prev = curr;
    }
  }

  glEndList();
}

void readgcode(int ignored) {
  static char gcbuf[GCODE_BLOCKSIZE];
  static unsigned sofar = 0;    /* Number of bytes we've already read */
  static gcblock *head = 0, *tail = 0;
  static struct timeval timeout = {0, 0};
  static char done = 0;

  FD_SET(gcsource, &fdset);

  int result;
  result = select(gcsource + 1, &fdset, NULL, NULL, &timeout);
  if(result < 0) {
    /* Something went wrong */
    perror("select");
    exit(EXIT_FAILURE);
  } else if(result > 0) {
    /* We have data! */
    if(FD_ISSET(gcsource, &fdset)) {
      ssize_t bytes = read(gcsource, gcbuf + sofar, GCODE_BLOCKSIZE - sofar);
      if(bytes < 0) {
        perror("read");
        exit(EXIT_FAILURE);
      } else if(bytes == 0) {
        /* We got an EOF. */
        done = 1;
      } else {
        size_t i;
        for(i = 0; i < (size_t)bytes; ++i) {
          if(gcbuf[sofar + i] == '\r' || gcbuf[sofar + i] == '\n') {
            /* Parse new block */
            gcblock *block = parse_block(gcbuf, sofar + i + 1);

            /* Shuffle data around for a clean start for the next */
            unsigned skip = (gcbuf[sofar + i + 1] == '\n') ? 2 : 1;
            memmove(gcbuf, gcbuf + sofar + i + skip, bytes - i);

            if(block) {
              /* Append block to block list */
              if(head) {
                tail->next = block;
                tail = block;
              } else {
                head = block;
                tail = block;
              }
            } else {
              fprintf(stderr, "WARNING: Got malformed block, ignoring.\n");
            }

            /* Rebuild display list */
            update(head);

            /* Reset loop */
            bytes -= i;
            i = 0;
          }
        }
      }
    }
  }
  
  /* TODO: Less delay if the above took a nontrivial amount of time */
  if(!done) {
    glutTimerFunc(FRAME_DELAY, readgcode, 0);
  }
}

void resize(int width, int height) {
  GLfloat ratio;
  if(height == 0) {
    return;
  }
  ratio = (GLfloat)width / (GLfloat)height;
  glViewport(0, 0, (GLsizei)width, (GLsizei)height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(45.0f, ratio, 0.1f, 100.0f);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

void key(unsigned char key, int x, int y) {
  switch(key) {
  case '+':
  case '=':
    camera.radius += 1;
    updatecam();
    break;

  case '-':
    camera.radius -= 1;
    updatecam();
    break;
  }
}

void special_key(int key, int x, int y) {
  switch(key) {
  case GLUT_KEY_LEFT:
    camera.longitude -= MOTION_INCREMENT;
    updatecam();
    break;

  case GLUT_KEY_RIGHT:
    camera.longitude += MOTION_INCREMENT;
    updatecam();
    break;

  case GLUT_KEY_DOWN:
    camera.latitude -= MOTION_INCREMENT;
    updatecam();
    break;

  case GLUT_KEY_UP:
    camera.latitude += MOTION_INCREMENT;
    updatecam();
    break;

  default:
    break;
  }   
}

void cleanup(void) {
  if(gcsource != STDIN_FILENO) {
    close(gcsource);
  }
}

int main(int argc, char** argv) {
  glutInit(&argc, argv);

  {
    /* Work out what we're reading from */
    gcsource = STDIN_FILENO;
    atexit(cleanup);
    if(argc > 1) {
      gcsource = open(argv[1], O_RDONLY);
      if(gcsource < 0) {
        perror(argv[1]);
        exit(EXIT_FAILURE);
      }

      int flags;
      if (-1 == (flags = fcntl(gcsource, F_GETFL, 0))) {
        flags = 0;
      }
      if(-1 == fcntl(gcsource, F_SETFL, flags | O_NONBLOCK)) {
        fprintf(stderr, "Failed to enter non-blocking mode: ");
        perror("fcntl");
        exit(EXIT_FAILURE);
      }
    }
    /* Init select data */
    FD_ZERO(&fdset);
  }
    
  /* Create window */
  {
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH | GLUT_MULTISAMPLE);
    glutInitWindowSize(DEFAULT_W, DEFAULT_H);

    /* Build title string */
    size_t titlelen = strlen(argv[0]);
    int i;
    for(i = 1; i < argc; ++i) {
      /* Include room for a space */
      titlelen += 1 + strlen(argv[i]);
    }
    char *title = malloc(titlelen);
    *title = '\0';
    strcat(title, argv[0]);
    for(i = 1; i < argc; ++i) {
      strcat(title, " ");
      strcat(title, argv[i]);
    }
        
    glutCreateWindow(title);
    free(title);
  }

	/* Configure OpenGL */
  glShadeModel( GL_SMOOTH );
  glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
  glClearDepth( 1.0f );
  glEnable( GL_DEPTH_TEST );
  glDepthFunc( GL_LEQUAL );
  glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );

  /* Prepare for mainloop */
  glutReshapeFunc(resize);
  glutKeyboardFunc(key);
  glutSpecialFunc(special_key);
  camtransform = calloc(16, sizeof(GLfloat));

  /* Initialize state */
  update(0);
  camera.latitude = 0;
  camera.longitude = 0;
  camera.radius = 6;
  updatecam();

  /* Enter main loop */
  readgcode(0);
  draw(0);
  glutMainLoop();

	exit(EXIT_SUCCESS);
}
