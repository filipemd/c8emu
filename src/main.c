/*
	Copyright (C) 2025 filipemd

	This file is part of C8EMU.

	C8EMU is free software: you can redistribute it and/or modify it under the terms of the 
	GNU General Public License as published by the Free Software Foundation, either version 3 
	of the License, or (at your option) any later version.

	C8EMU is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without 
	even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along with C8EMU. If not,
	see <https://www.gnu.org/licenses/>. 
*/

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "emulator.h"
#include "beep.h"

#define SCALE 10

static const char* version="0.2.0";

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

struct emulator emulator;

static inline void show_usage(const char* argv0) {
	printf("%s <rom_file> [ticks_per_frame]\n", argv0);
}

static inline void show_version(const char *argv0) {
	printf("%s version %s\n", argv0, version);
}

static void init_emulator(int argc, char* argv[]) {
	if (argc < 2 || strcmp(argv[1], "--help")==0 || strcmp(argv[1], "-h")==0) {
		show_usage(argv[0]);
		exit(EXIT_SUCCESS);
	}

	if (strcmp(argv[1], "--version")==0 || strcmp(argv[1], "--version")==0) {
		show_version(argv[0]);
		exit(EXIT_SUCCESS);
	}

	emulator_init(&emulator, argv[1]);
	beep_init();
	if (argc >= 3) {
		const int cycles_per_frame_arg = atoi(argv[2]);
		if (cycles_per_frame_arg <= 0 || cycles_per_frame_arg > 255) {
			fprintf(stderr, "Error: cycles per frame amount must be between 1-255.\n");
			exit(EXIT_FAILURE);
		}
		emulator.cycles_per_frame = cycles_per_frame_arg;
	}
}

static uint8_t map_scancode_to_key(SDL_Scancode scancode) {
	switch (scancode) {
		case SDL_SCANCODE_1: return 0x1; case SDL_SCANCODE_2: return 0x2; case SDL_SCANCODE_3: return 0x3; case SDL_SCANCODE_4: return 0xC;
		case SDL_SCANCODE_Q: return 0x4; case SDL_SCANCODE_W: return 0x5; case SDL_SCANCODE_E: return 0x6; case SDL_SCANCODE_R: return 0xD;
		case SDL_SCANCODE_A: return 0x7; case SDL_SCANCODE_S: return 0x8; case SDL_SCANCODE_D: return 0x9; case SDL_SCANCODE_F: return 0xE;
		case SDL_SCANCODE_Z: return 0xA; case SDL_SCANCODE_X: return 0x0; case SDL_SCANCODE_C: return 0xB; case SDL_SCANCODE_V: return 0xF;
		default: return 16;
	}
}

static void render_emulator(void) {
	// 1. Definir cor preta e limpar o fundo
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);

	// 2. Definir cor branca para os pixels do emulador
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

	for (uint8_t y=0; y<EMULATOR_HEIGHT; y++) {
	for (uint8_t x=0; x<EMULATOR_WIDTH; x++) {
		const size_t i = y*EMULATOR_WIDTH + x;
		const size_t byte = i/8;
		const uint8_t bit = 7 - (i % 8);

		if (emulator.screen[byte] & 1<<bit) {
			const SDL_FRect rect = {x*SCALE, y*SCALE, SCALE, SCALE};
			SDL_RenderFillRect(renderer, &rect);
		}
	}
	}
}

static void update_emulator(void) {
	const bool* keys = SDL_GetKeyboardState(NULL);
	
	emulator.keys=0;
	for (SDL_Scancode key=0; key<SDL_SCANCODE_COUNT; key++) {
		if (!keys[key]) {
			continue;
		}
		const uint16_t emulator_key = map_scancode_to_key(key);

		if (emulator_key < 16) {
			emulator.keys|=(1 << emulator_key);
		}
	}

	emulator_tick(&emulator);
	if (emulator.draw_flag) {
		render_emulator();
		SDL_RenderPresent(renderer);
	}
	if (emulator.beep_flag) {
		beep_play();
	}

	// 60 FPS
	SDL_Delay(16);
}

static void quit_emulator(void) {
	beep_quit();
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
	(void)appstate;
	SDL_SetAppMetadata("CH8EMU", "0.1.0", "com.filipemd.ch8emu");

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
		SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	if (!SDL_CreateWindowAndRenderer("examples/CATEGORY/NAME", EMULATOR_WIDTH*SCALE, EMULATOR_HEIGHT*SCALE, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
		SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	SDL_SetRenderLogicalPresentation(renderer, SCALE*EMULATOR_WIDTH, SCALE*EMULATOR_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);

	init_emulator(argc, argv);

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
	(void)appstate;
	if (event->type == SDL_EVENT_QUIT) {
		return SDL_APP_SUCCESS;
	}

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
	(void)appstate;

	update_emulator();

	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
	(void)appstate;
	(void)result;

	quit_emulator();
}