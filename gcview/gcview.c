#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#include <stdio.h>

#include <GL/gl.h>
#include <GL/glut.h>

#include "../common/gcode.h"

#define DEFAULT_W 640
#define DEFAULT_H 480

#define FRAME_DELAY 17          /* 1/(17ms) = about 60FPS */

#define MOTION_INCREMENT (M_PI/5)

#define GCODE_BUFSIZE 512       /* Standard states 256 chars max */

GLuint dlist;                   /* Display list pointer */
FILE *gcsource;

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
  draw();
    
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

  /* Work out what we're reading from */
  gcsource = stdin;             /* Default to stdin */
  if(argc > 1) {
    gcsource = fopen(argv[1], "r");
    if(!gcsource) {
      perror(argv[1]);
      return 1;
    }
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

  /* Allocate display list */
  dlist = glGenLists(1);
  update();

  /* Prepare for mainloop */
  glutReshapeFunc(resize);
  glutSpecialFunc(special_key);

  /* Enter main loop */
  idle(0);
  glutMainLoop();

	exit(EXIT_SUCCESS);
}
