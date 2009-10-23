#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "SDL.h"
#include "SDL_opengl.h"

int main(int argc, char** argv) 
{
	if(SDL_Init(SDL_INIT_VIDEO) != 0) {
		fprintf(stderr, "Unable to initialize SDL: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}
	atexit(SDL_Quit);
	
	SDL_WM_SetCaption("gcode viewer", "gcview");

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_Surface *screen = SDL_SetVideoMode(640, 480, 0, SDL_OPENGL | SDL_RESIZABLE);

	int running = 1;
	while(running) {
		SDL_Event e;
		while(SDL_PollEvent(&e)) {
			switch(e.type) {
			case SDL_QUIT:
				running = 0;
			}
		}
	}

	exit(EXIT_SUCCESS);
}
