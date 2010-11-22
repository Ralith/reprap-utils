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

#ifdef APPLE
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif
#include <SDL.h>

#include "../common/gcode.h"
#include "render.h"

#define DEFAULT_W 640
#define DEFAULT_H 480

#define FRAME_DELAY 17          /* 1/(17ms) = about 60FPS */

#define MOTION_INCREMENT M_PI

#define VIEWDISTANCE (1000.0f)

#define HELP "Usage: gcview [-s] [file]\n" \
  "\t-s\tShow FPS\n" \
  "\tfile\tFile to read from.  Standard input is used if this is omitted.\n"
  //"When input is read from stdin, SIGHUP will trigger a reset to the initial state.\n"

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
    glRotatef(longitude, 0, 1, 0);
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
  if(glIsList(dlist)) {
    glDeleteLists(dlist, 1);
  }
  dlist = glGenLists(1);
  glNewList(dlist, GL_COMPILE);
  if(head) {
    render_words(head);
  }
  glEndList();
}

int readgcode(struct timeval timeout) {
  static char gcbuf[GCODE_BLOCKSIZE*1024];
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
        needsupdate = 0;
        if(gcsource != STDIN_FILENO) {
          return 0;
        } /* else { */
          /* /\* Reset state *\/ */
          /* sofar = 0; */
          /* blockidx = 1; */
          /* real_line = 0; */
          /* needsupdate = 1; */
          /* head = NULL; */
          /* tail = NULL; */
          /* /\* Free gcode blocks *\/ */
          /* gcblock *block, *next; */
          /* for(block = head; block != NULL; block = next) { */
          /*   next = block->next; */
          /*   free(block->text); */
          /*   free(block->words); */
          /*   free(block); */
          /* } */
        return 1;
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
  char showfps = 0;
  char *file = 0;
  /* Handle args */
  {
    int opt;
    while((opt = getopt(argc, argv, "h?s")) >= 0) {
      switch(opt) {
      case 'h':
      case '?':
        printf("%s", HELP);
        exit(EXIT_SUCCESS);
        break;

      case 's':
        showfps = 1;
        break;

      default:
        break;
      }
    }
    switch(argc - optind) {
    case 1:
      file = argv[optind];
      break;

    default:
      break;
    }
    {
      /* Work out what we're reading from */
      gcsource = STDIN_FILENO;
      if(file) {
        gcsource = open(file, O_RDONLY);
        atexit(cleanup);
        if(gcsource < 0) {
          perror(file);
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
  }

  /* Init SDL */
  if(SDL_Init(SDL_INIT_VIDEO) < 0) {
    fprintf(stderr, "Video initialization failed: %s\n", SDL_GetError());
    exit(EXIT_FAILURE);
  }
  atexit(SDL_Quit);
  SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_INTERVAL, SDL_DEFAULT_REPEAT_INTERVAL);
    
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
    /* TODO: Check for AA support (OpenGL extension test?) */
    /*SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
      SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);*/

    surface = SDL_SetVideoMode(DEFAULT_W, DEFAULT_H, 16, vflags);
    if(!surface) {
      fprintf(stderr, "Failed to set video mode: %s\n", SDL_GetError());
      exit(EXIT_FAILURE);
    }

    char *title;
    if(file) {
      title = calloc(strlen(argv[0]) + strlen(file) + 2, sizeof(char));
      strcpy(title, argv[0]);
      strcat(title, " ");
      strcat(title, file);
    } else {
      title = calloc(strlen(argv[0]) + strlen(" stdin") + 1, sizeof(char));
      strcpy(title, argv[0]);
      strcat(title, " stdin");
    }
    SDL_WM_SetCaption(title, title);
    free(title);
  }

	/* Configure OpenGL */
  glShadeModel(GL_FLAT);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClearDepth(1.0f);
  glEnable(GL_DEPTH_TEST);
  /*glEnable(GL_MULTISAMPLE);*/
  glDepthFunc(GL_LEQUAL);
  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

  /* Prepare for mainloop */
  camtransform = calloc(16, sizeof(GLfloat));

  /* Initialize state */
  update(0);
  camera.latitude = 0;
  camera.longitude = 0;
  camera.radius = 100;
  updatecam();
  resize(DEFAULT_W, DEFAULT_H);

  /* Enter main loop */
  SDL_Event e;
  char done = 0;
  char gcdone = 0;
  char dragging = 0;
  struct timeval t0, t, dt;
  unsigned frames = 0;
  float fps_elapsed = 0;
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

      case SDL_MOUSEMOTION:
        if(dragging) {
          camera.longitude += e.motion.xrel;
          camera.latitude += e.motion.yrel;
          updatecam();
        }
        break;

      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP:
        switch(e.button.button) {
        case SDL_BUTTON_LEFT:
          dragging = e.button.state;
          break;
          
        case SDL_BUTTON_WHEELUP:
          camera.radius -= 10;
          updatecam();
          break;

        case SDL_BUTTON_WHEELDOWN:
          camera.radius += 10;
          updatecam();
          break;

        default:
          break;
        }
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
    ++frames;
    if(showfps) {
      gettimeofday(&t, NULL);
      dt.tv_sec = t.tv_sec - t0.tv_sec;
      dt.tv_usec = t.tv_usec - t0.tv_usec;
      fps_elapsed += (dt.tv_usec / 1000000.0) + dt.tv_sec;
      if(frames >= 30) {
        printf("FPS: %f\n", ((float)frames)/fps_elapsed);
        frames = 0;
        fps_elapsed = 0;
      }
    }
  }

	exit(EXIT_SUCCESS);
}
