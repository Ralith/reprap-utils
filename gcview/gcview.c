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
#include "render.h"

#define DEFAULT_W 640
#define DEFAULT_H 480

#define FRAME_DELAY 17          /* 1/(17ms) = about 60FPS */

#define MOTION_INCREMENT M_PI

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
  point root = {0.0, 0.0, 0.0};
  
  if(glIsList(dlist)) {
    glDeleteLists(dlist, 1);
  }
  dlist = glGenLists(1);
  glNewList(dlist, GL_COMPILE);
  if(head) {
    render_words(head, &root);
  }
  glEndList();
}

void readgcode() {
  static char gcbuf[GCODE_BLOCKSIZE];
  static gcblock *head = 0, *tail = 0;
  static struct timeval timeout = {0, (FRAME_DELAY*1000)};
  static unsigned blockidx = 1, real_line = 0;
  static unsigned sofar = 0;

  FD_SET(gcsource, &fdset);

  int result;
  result = select(gcsource + 1, &fdset, NULL, NULL, &timeout);
  printf("Select returned %d\n", result);
  if(result < 0) {
    /* Something went wrong */
    perror("select");
    exit(EXIT_FAILURE);
  } else if(result == 0) {
    /* Timeout expired; update display list */
    update(head);
  } else if(result > 0) {
    /* We have data! */
    if(FD_ISSET(gcsource, &fdset)) {
      ssize_t bytes = read(gcsource, gcbuf + sofar, GCODE_BLOCKSIZE - sofar - 1);
      if(bytes < 0) {
        perror("read");
        exit(EXIT_FAILURE);
      } else if(bytes == 0) {
        /* We got an EOF, no need to run any more */
        glutIdleFunc(NULL);
        /* Ensure the display list is up to date before bailing out */
        update(head);
        return;
      }
      size_t i = sofar;
      size_t block_start = 0;
      const size_t end = sofar+bytes;
      /* Parse any and all blocks */
      for(; i < end; ++i) {
        if(gcbuf[i] == '\n' || gcbuf[i] == '\r') {
          /* gcbuf[i] = '\0'; */
          const size_t len = i - block_start;
          if(gcbuf[i] == '\n') {
            real_line++;
          }
          if(len < 1) {
            /* Skip empty lines */
            continue;
          }
          gcblock *block = parse_block(gcbuf + block_start, len);
          /* printf("Found line: \"%s\"\n", gcbuf + block_start); */
          block_start = i + 1;

          if(!block) {
            fprintf(stderr, "WARNING: Line %d: Skipping malformed block\n", real_line);
            continue;
          }

          block->real_line = real_line;
          block->index = blockidx++;

          /* Append to block list */
          if(head) {
            tail->next = block;
          } else {
            head = block;
          }
          tail = block;
        }
      }
      if(block_start < end) {
        sofar = end - block_start;
        if(block_start > 0) {
          memmove(gcbuf, gcbuf + block_start, sofar);
        }
      }
    }
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
  glutIdleFunc(readgcode);
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
  draw(0);
  glutMainLoop();

	exit(EXIT_SUCCESS);
}
