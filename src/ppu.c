#include "ppu.h"

#include <SDL2/SDL.h>

ppu_t ppu;

uint8_t ppuRAM[0x4000];

SDL_Window* w;
SDL_Surface* windowSurface;
SDL_Surface* tile;
SDL_Surface* nameTable;
SDL_Surface* frameBuffer;

#define NAMETABLE_WIDTH 32*8*2
#define NAMETABLE_HEIGHT 32*8*2

uint8_t initRenderer(void) {
	if(SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("could not init SDL\n");
		return 1;
	}

	w = SDL_CreateWindow("nesEmu", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
	windowSurface = SDL_GetWindowSurface(w);
	tile = SDL_CreateRGBSurface(0, 8, 16, 32, 0xFF000000, 0xFF0000, 0xFF00, 0xFF);
	frameBuffer = SDL_CreateRGBSurface(0, FB_WIDTH, FB_HEIGHT, 32, 0xFF000000, 0xFF0000, 0xFF00, 0xFF);
	nameTable = SDL_CreateRGBSurface(0, NAMETABLE_WIDTH, NAMETABLE_HEIGHT, 32, 0xFF000000, 0xFF0000, 0xFF00, 0xFF);
	return 0;
}

void uninitRenderer(void) {
	SDL_FreeSurface(tile);
	SDL_FreeSurface(frameBuffer);
	SDL_FreeSurface(nameTable);
	SDL_DestroyWindowSurface(w);
	SDL_DestroyWindow(w);

	SDL_Quit();
}

void debugScreenshot(void) {
	SDL_SaveBMP(nameTable, "nametable.bmp");
	SDL_SaveBMP(frameBuffer, "framebuffer.bmp");
}

#define MIRROR_HORIZONTAL 0x40
#define MIRROR_VERTICAL 0x80

void drawTile(SDL_Surface* dst, uint8_t* bitplaneStart, uint16_t x, uint16_t y, uint8_t attribs) {
	SDL_Rect targetRect = {
		.x = x,
		.y = y,
		.w = 8,
		.h = 8,
	};
	SDL_Rect srcRect = {
		.x = 0,
		.y = 0,
		.w = 8,
		.h = 8,
	};
	uint32_t* target = (uint32_t*)tile->pixels + (attribs & MIRROR_VERTICAL ? 64 : 0);
	uint8_t* bitplane1 = bitplaneStart;
	uint8_t* bitplane2 = bitplane1 + 8;
	for(uint8_t j = 0; j < 8; ++j) {
		/*if(attribs & MIRROR_HORIZONTAL) {
			for(uint8_t k = 0; k < 8; ++k) {
				uint8_t combined = ((*bitplane1 >> k) & 1) | ((*bitplane2 >> k & 1) << 1);
				*target = 0xFFFFFF00 | (0xFF * (combined != 0)); // just doing this until I set up pallete stuff
				++target;
			}
		} else {
			for(uint8_t k = 7; k < 8; --k) {
				uint8_t combined = ((*bitplane1 >> k) & 1) | (((*bitplane2 >> k) & 1) << 1);
				*target = 0xFFFFFF00 | (0xFF * (combined != 0)); // just doing this until I set up pallete stuff
				++target;
			}
		}*/
		uint8_t k = (attribs & MIRROR_HORIZONTAL ? 0 : 7);
		while(k < 8) {
			uint8_t combined = ((*bitplane1 >> k) & 1) | (((*bitplane2 >> k) & 1) << 1);
			*target = 0xFFFFFF00 | (0xFF * (combined != 0));
			++target;
			k += (attribs & MIRROR_HORIZONTAL ? 1 : -1);
		}
		if(attribs & MIRROR_VERTICAL) {
			target -= 16;
		}
		//target += tile->pitch;
		++bitplane1;
		++bitplane2;
	}
	SDL_BlitSurface(tile, &srcRect, dst, &targetRect);
}

void drawNametable(uint8_t* bank, uint8_t* table, uint16_t x, uint16_t y) {
	for(uint16_t i = 0; i < 1024; ++i) {
		uint8_t tileID = *(table+i);

		uint8_t* bitplaneStart = bank + tileID*8*2;
		uint16_t xPos = ((i%32)*8 + x) % 480;
		uint16_t yPos = (i/32)*8 + y;
		drawTile(nameTable, bitplaneStart, xPos, yPos, 0);
	}
}

void render(void) {
	SDL_FillRect(windowSurface, &(SDL_Rect){0,0,SCREEN_WIDTH,SCREEN_HEIGHT}, 0xFF000000);
	SDL_FillRect(frameBuffer, &(SDL_Rect){0,0,FB_WIDTH,FB_HEIGHT}, 0xFF000000);
	SDL_FillRect(nameTable, &(SDL_Rect){0,0,NAMETABLE_WIDTH,NAMETABLE_HEIGHT}, 0xFF000000);
	// draw nametable
	// only dealing with the horizontal mirroring for now
	uint8_t* bank = &ppuRAM[(ppu.control & 0x10 ? 0x1000 : 0x0000)];
	drawNametable(bank, &ppuRAM[0x2000], 0, 0);
	drawNametable(bank, &ppuRAM[0x2800], 0, 240);
	drawNametable(bank, &ppuRAM[0x2000], 256, 0);
	drawNametable(bank, &ppuRAM[0x2800], 256, 240);
	SDL_Rect srcRect = {
		.x = ppu.scrollX,
		.y = ppu.scrollY,
		.w = 256,
		.h = 240,
	};
	SDL_Rect dstRect = {
		.x = 0,
		.y = 0,
		.w = FB_WIDTH,
		.h = FB_HEIGHT,
	};
	SDL_BlitSurface(nameTable, &srcRect, frameBuffer, &dstRect);
	// draw sprites in OAM
	for(uint8_t i = 0; i < 64; ++i) {
		#ifdef DEBUG
			SDL_Rect asdf = {
				.x = ppu.oam[i*4 + 3],
				.y = ppu.oam[i*4 + 0],
				.w = 8,
				.h = 8,
			};
			SDL_FillRect(frameBuffer, &asdf, 0xFFFFFF55);
		#endif

		uint8_t tileID = ppu.oam[i*4 + 1];
		uint8_t* bank;
		if(ppu.control & 0x20) {
			bank = &ppuRAM[(tileID & 1 ? 0x1000 : 0x0000)];
		} else {
			bank = &ppuRAM[(ppu.control & 0x08 ? 0x1000 : 0x0000)];
		}

		uint8_t* bitplaneStart = bank + tileID*8*2;
		drawTile(frameBuffer, bitplaneStart, ppu.oam[i*4 + 3], ppu.oam[i*4 + 0], ppu.oam[i*4 + 2]);
		if(ppu.control & 0x20) {
			bitplaneStart += 16;
			drawTile(frameBuffer, bitplaneStart, ppu.oam[i*4 + 3], ppu.oam[i*4 + 0]+8, ppu.oam[i*4 + 2]);
		}
	}
	SDL_BlitScaled(frameBuffer, &(SDL_Rect){0,0,FB_WIDTH,FB_HEIGHT}, windowSurface, &(SDL_Rect){0,0,SCREEN_WIDTH,SCREEN_HEIGHT});
	SDL_UpdateWindowSurface(w);
}
