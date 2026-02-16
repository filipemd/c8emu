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

#ifndef EMULATOR_H
#define EMULATOR_H

#include <stdint.h>
#include <stdbool.h>

#define EMULATOR_WIDTH 64
#define EMULATOR_HEIGHT 32

#define MEMORY_START 0x200

#define MEMORY_SIZE 0x1000
#define STACK_SIZE 12

struct emulator {
	uint8_t cycles_per_frame;

	// Cada bit um pixel
	uint8_t screen[EMULATOR_WIDTH/8 * EMULATOR_HEIGHT];

	bool draw_flag;
	bool beep_flag;

	// Cada bit uma tecla
	uint16_t keys;

	uint8_t _memory[MEMORY_SIZE];

	uint16_t _stack[STACK_SIZE];
	uint16_t _sp; // Ponteiro da stack

	uint8_t _v[16]; // Registradores
	uint16_t _i; // Registrador index

	uint16_t _pc;

	uint8_t _delay_timer;
	uint8_t _sound_timer;
};

void emulator_init(struct emulator* emulator, const char* rom);

void emulator_tick(struct emulator* emulator);

int emulator_cycle(struct emulator* emulator);


#endif