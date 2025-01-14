#include "input.h"

#include <stdio.h>

#include "SDL.h"

#include "ppu.h"

controller_t controllers[2];

uint8_t controllerLatch;
int keyNumber;
const uint8_t* keys;

SDL_Event e;

uint8_t pollController(uint8_t port) {
	controller_t* c = &controllers[port];

	if(controllerLatch) { return 0; } // idk what happens when you try to read while latch is set, presumably it reads like just open bus stuff

	if(c->currentBit == 0) { c->currentBit = 0x80; }
	uint8_t ret = (c->buttons & c->currentBit) != 0;
	c->currentBit >>= 1;
	return ret;
}

void initInput(void) {
	keys = SDL_GetKeyboardState(&keyNumber); 
}

uint8_t handleInput(void) {
	SDL_PollEvent(&e);
	if(e.type == SDL_QUIT) {
		return 1;
	}
	const SDL_Keycode inputKeys[] = {
		SDL_SCANCODE_RIGHT,
		SDL_SCANCODE_LEFT,
		SDL_SCANCODE_DOWN,
		SDL_SCANCODE_UP,
		SDL_SCANCODE_RETURN,
		SDL_SCANCODE_RSHIFT,
		SDL_SCANCODE_X,
		SDL_SCANCODE_Z,
	};
	for(uint8_t i = 0; i < 8; ++i) {
		if(keys[inputKeys[i]]) {
			controllers[0].buttons |= 1 << i;
		} else {
			controllers[0].buttons &= ~(1 << i);
		}
	}

	if(keys[SDL_SCANCODE_P]) {
		debugScreenshot();
		exit(1);
	}
	return 0;
}
