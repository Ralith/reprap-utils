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

#define MOTION_INCREMENT (M_PI/5)

GLuint dlist;                   /* Display list pointer */
int gcsource;                   /* FD we're reading gcode from */
fd_set fdset;

struct {
  float latitude;
  float longitude;
  float radius;
} camera;


/* Draw the current state of affairs */
void draw(void) {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glCallList(dlist);

  glutSwapBuffers();
}

void idle(int ignored) {
  static char gcbuf[GCODE_BLOCKSIZE];
  static unsigned sofar = 0;    /* Number of bytes we've already read */
  static gcblock *head = 0, *tail = 0;
  static struct timeval timeout = {0, 0};

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
      }
      size_t i;
      for(i = 0; i < bytes; ++i) {
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
            } else {
              head = block;
              tail = block;
            }
          } else {
            fprintf(stderr, "WARNING: Got malformed block, ignoring.\n");
          }

          /* Rebuild display list */
          update();

          /* Leave loop */
          break;
        }
      }
    }
  }

  draw();
  /* TODO: Less delay if the above took a nontrivial amount of time */
  glutTimerFunc(FRAME_DELAY, idle, 0);
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

void special_key(int key, int x, int y) {
  switch(key) {
  case GLUT_KEY_LEFT:
    camera.longitude -= MOTION_INCREMENT;
    break;

  case GLUT_KEY_RIGHT:
    camera.longitude += MOTION_INCREMENT;
    break;

  case GLUT_KEY_DOWN:
    camera.latitude -= MOTION_INCREMENT;
    break;

  case GLUT_KEY_UP:
    camera.latitude += MOTION_INCREMENT;
    break;

  default:
    break;
  }   
}

void update() {
  if(glIsList(dlist)) {
    glDeleteLists(dlist, 1);
  }
  dlist = glGenLists(1);
  
  glNewList(dlist, GL_COMPILE);
  /* Move Left 1.5 Units And Into The Screen 6.0 */
  glLoadIdentity();
  glTranslatef(-1.5f, 0.0f, -6.0f);

  glBegin(GL_TRIANGLES);            /* Drawing Using Triangles */
  glVertex3f( 0.0f,  1.0f, 0.0f); /* Top */
  glVertex3f(-1.0f, -1.0f, 0.0f); /* Bottom Left */
  glVertex3f( 1.0f, -1.0f, 0.0f); /* Bottom Right */
  glEnd();                           /* Finished Drawing The Triangle */

  /* Move Right 3 Units */
  glTranslatef(3.0f, 0.0f, 0.0f);

  glBegin(GL_QUADS);                /* Draw A Quad */
  glVertex3f(-1.0f,  1.0f, 0.0f); /* Top Left */
  glVertex3f( 1.0f,  1.0f, 0.0f); /* Top Right */
  glVertex3f( 1.0f, -1.0f, 0.0f); /* Bottom Right */
  glVertex3f(-1.0f, -1.0f, 0.0f); /* Bottom Left */
  glEnd();                           /* Done Drawing The Quad */
  glEndList();
}

int main(int argc, char** argv) {
  glutInit(&argc, argv);

  {
    /* Work out what we're reading from */
    gcsource = STDIN_FILENO;
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
  glutSpecialFunc(special_key);

  /* Enter main loop */
  update();
  idle(0);
  glutMainLoop();

	exit(EXIT_SUCCESS);
}
