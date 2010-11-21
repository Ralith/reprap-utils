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
#include <GL/glu.h>
#include <SDL.h>

#include "../common/gcode.h"
#include "render.h"

#define DEFAULT_W 640
#define DEFAULT_H 480

#define FRAME_DELAY 17          /* 1/(17ms) = about 60FPS */

#define MOTION_INCREMENT M_PI

#define VIEWDISTANCE (1000.0f)

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
    
    glTranslatef(0.0f, 0.0f, -radius);
    glRotatef(-longitude, 0, 1, 0);
    glRotatef(latitude, 1, 0, 0);
    //glTranslatef(1.0f, 1.0f, 0.0f);

    glGetFloatv(GL_MODELVIEW_MATRIX, matrix);
  }
  glPopMatrix();
}

void updatecam() {
  storexform(camtransform, camera.latitude, camera.longitude, camera.radius);
}

/* Draw the current state of affairs */
void draw() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();
  
  glLoadMatrixf(camtransform);
  
  glCallList(dlist);

  SDL_GL_SwapBuffers();
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

int readgcode(struct timeval timeout) {
  static char gcbuf[GCODE_BLOCKSIZE];
  static gcblock *head = 0, *tail = 0;
  static unsigned blockidx = 1, real_line = 0;
  static unsigned sofar = 0;
  static char needsupdate = 0;

  FD_SET(gcsource, &fdset);

  int result;
  result = select(gcsource + 1, &fdset, NULL, NULL, &timeout);
  if(result < 0) {
    /* Something went wrong */
    perror("select");
    exit(EXIT_FAILURE);
  } else if(result == 0) {
    if(needsupdate) {
      /* Timeout expired; update display list */
      update(head);
      needsupdate = 0;
    }
  } else if(result > 0) {
    /* We have data! */
    if(FD_ISSET(gcsource, &fdset)) {
      ssize_t bytes = read(gcsource, gcbuf + sofar, GCODE_BLOCKSIZE - sofar - 1);
      if(bytes < 0) {
        perror("read");
        exit(EXIT_FAILURE);
      } else if(bytes == 0) {
        /* We got an EOF */
        /* Ensure the display list is up to date before bailing out */
        if(needsupdate) {
          update(head);
        }
        return 0;
      }
      size_t i = sofar;
      size_t block_start = 0;
      const size_t end = sofar+bytes;
      /* Parse any and all blocks */
      for(; i < end; ++i) {
        if(gcbuf[i] == '\n' || gcbuf[i] == '\r') {
          const size_t len = i - block_start;
          if(gcbuf[i] == '\n') {
            real_line++;
          }
          if(len < 1) {
            /* Skip empty lines */
            continue;
          }
          char *text = calloc(len+1, sizeof(char));
          gcbuf[i] = 0;
          strncpy(text, gcbuf + block_start, len);
          gcblock *block = parse_block(gcbuf + block_start, len);
          block_start = i + 1;

          if(!block) {
            fprintf(stderr, "WARNING: Line %d: Skipping malformed block\n", real_line);
            fprintf(stderr, "Block: \"%s\"\n", text);
            free(text);
            continue;
          }

          block->text = text;
          block->real_line = real_line;
          block->index = blockidx++;

          /* Append to block list */
          if(head) {
            tail->next = block;
          } else {
            head = block;
          }
          tail = block;
          needsupdate = 1;
        }
      }
      if(block_start < end) {
        sofar = end - block_start;
        if(block_start > 0) {
          memmove(gcbuf, gcbuf + block_start, sofar);
          gcbuf[sofar] = 0;
        }
      } else {
        sofar = 0;
      }
    }
  }
  return 1;
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
  gluPerspective(45.0f, ratio, 0.1f, VIEWDISTANCE);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

void handlekey(SDL_keysym *key) {
  switch(key->sym) {
  case SDLK_PLUS:
  case SDLK_EQUALS:
    camera.radius -= 10;
    updatecam();
    break;

  case SDLK_MINUS:
    camera.radius += 10;
    updatecam();
    break;

  case SDLK_LEFT:
    camera.longitude -= MOTION_INCREMENT;
    updatecam();
    break;

  case SDLK_RIGHT:
    camera.longitude += MOTION_INCREMENT;
    updatecam();
    break;

  case SDLK_DOWN:
    camera.latitude -= MOTION_INCREMENT;
    updatecam();
    break;

  case SDLK_UP:
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

  /* Init SDL */
  if(SDL_Init(SDL_INIT_VIDEO) < 0) {
    fprintf(stderr, "Video initialization failed: %s\n", SDL_GetError());
    exit(EXIT_FAILURE);
  }
  atexit(SDL_Quit);
  SDL_EnableKeyRepeat(1, SDL_DEFAULT_REPEAT_INTERVAL);
    
  /* Create window */
  int vflags;
  SDL_Surface *surface;
  {
    const SDL_VideoInfo *vinfo;
    vinfo = SDL_GetVideoInfo();
    if(!vinfo) {
      fprintf(stderr, "Video query failed: %s\n", SDL_GetError());
      exit(EXIT_FAILURE);
    }

    vflags = SDL_OPENGL
      | SDL_GL_DOUBLEBUFFER
      | SDL_HWPALETTE
      | SDL_RESIZABLE
      | (vinfo->hw_available ? SDL_HWSURFACE : SDL_SWSURFACE)
      | (vinfo->blit_hw ? SDL_HWACCEL : 0);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    surface = SDL_SetVideoMode(DEFAULT_W, DEFAULT_H, 16, vflags);
    if(!surface) {
      fprintf(stderr, "Failed to set video mode: %s\n", SDL_GetError());
      exit(EXIT_FAILURE);
    }

    /* Build title string */
    size_t titlelen = strlen(argv[0]);
    int i;
    for(i = 1; i < argc; ++i) {
      /* Include room for a space */
      titlelen += 1 + strlen(argv[i]);
    }
    char *title = malloc(titlelen);
    *title = '\0';
    strcat(title, "gcview");
    for(i = 1; i < argc; ++i) {
      strcat(title, " ");
      strcat(title, argv[i]);
    }
    SDL_WM_SetCaption(title, title);
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
  camtransform = calloc(16, sizeof(GLfloat));

  /* Initialize state */
  update(0);
  camera.latitude = 0;
  camera.longitude = 0;
  camera.radius = 100;
  updatecam();

  /* Enter main loop */
  SDL_Event e;
  char done = 0;
  char gcdone = 0;
  struct timeval t0, t, dt;
  while(!done) {
    dt.tv_sec = 0;
    dt.tv_usec = 0;
    gettimeofday(&t0, NULL);
    while(SDL_PollEvent(&e)) {
      switch(e.type) {
      case SDL_VIDEORESIZE:
        surface = SDL_SetVideoMode(e.resize.w, e.resize.h, 16, vflags);
        if(!surface) {
          fprintf(stderr, "Failed to set video mode: %s\n", SDL_GetError());
          exit(EXIT_FAILURE);
        }
        resize(e.resize.w, e.resize.h);
        break;

      case SDL_KEYDOWN:
        handlekey(&e.key.keysym);
        break;

      case SDL_QUIT:
        done = 1;
        break;
      }
    }
    while(!gcdone && dt.tv_sec == 0 && dt.tv_usec <= (1000 * FRAME_DELAY)) {
      struct timeval timeout = {0, (1000 * FRAME_DELAY) - dt.tv_usec};
      gcdone = !readgcode(timeout);
      gettimeofday(&t, NULL);
      dt.tv_sec = t.tv_sec - t0.tv_sec;
      dt.tv_usec = t.tv_usec - t0.tv_usec;
    }
    draw();
  }

	exit(EXIT_SUCCESS);
}
