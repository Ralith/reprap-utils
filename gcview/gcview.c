#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#include <GL/gl.h>
#include <GL/glut.h>

#define DEFAULT_W 640
#define DEFAULT_H 480

#define FRAME_DELAY 17          /* 1/(17ms) = about 60FPS */

#define MOTION_INCREMENT (M_PI/5)

char rerender;                  /* Do we currently need to rerender? */

struct {
    float latitude;
    float longitude;
    float radius;
} camera;

/* Render the current state of affairs */
void render(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glColor3f(1.0, 1.0, 1.0);   /* set current color to white */
    glBegin(GL_POLYGON);        /* draw filled triangle */
    glVertex2i(200,125);        /* specify each vertex of triangle */
    glVertex2i(100,375);
    glVertex2i(300,375);
    glEnd();                    /* OpenGL draws the filled triangle */
    glutSwapBuffers();

    rerender = 0;               /* Image is now up to date */
}

void idle(int ignored) {
    if(rerender) {
        render();
    }
    
    glutTimerFunc(FRAME_DELAY, idle, 0);
}

void resize(int width, int height) {
    rerender = 1;
    glViewport(0, 0, width, height);
}

void special_key(int key, int x, int y) {
    rerender = 1;
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
        rerender = 0;
    }   
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);

    /* TODO: Validate arguments */
    
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
    }

	/* Configure OpenGL */
	glClearColor(0, 0, 0, 0);

    /* Prepare for mainloop */
    glutReshapeFunc(resize);
    glutSpecialFunc(special_key);
    rerender = 0;

    /* Enter main loop */
    idle(0);
    glutMainLoop();

	exit(EXIT_SUCCESS);
}
