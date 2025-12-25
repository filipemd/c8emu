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

#include "emulator.h"

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <assert.h>
#include <string.h>
#include <time.h>

#define ARRAY_LEN(arr) (sizeof(arr)/sizeof(arr[0]))

#ifdef DEBUG
#define p(...) printf(__VA_ARGS__);
#else
#define p(...)
#endif

#ifndef TEST 
#define show_error_message(...) fprintf(stderr, __VA_ARGS__);
#else
#define show_error_message(...)
#endif

static inline void unknown_opcode(uint16_t opcode) {
	// Pra não dar aviso em testes
	(void)opcode;
	show_error_message("Unknown opcode: 0x%x\n", opcode);
}

static const uint8_t chip8_fontset[] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, //0
    0x20, 0x60, 0x20, 0x20, 0x70, //1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, //2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, //3
    0x90, 0x90, 0xF0, 0x10, 0x10, //4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, //5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, //6
    0xF0, 0x10, 0x20, 0x40, 0x40, //7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, //8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, //9
    0xF0, 0x90, 0xF0, 0x90, 0x90, //A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, //B
    0xF0, 0x80, 0x80, 0x80, 0xF0, //C
    0xE0, 0x90, 0x90, 0x90, 0xE0, //D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, //E
    0xF0, 0x80, 0xF0, 0x80, 0x80  //F
};

static void reset_emulator(struct emulator* emulator) {
	emulator->cycles_per_frame=16;

	// Limpa tudo
	memset(emulator->_memory, 0, sizeof(emulator->_memory));
	memset(emulator->screen, 0, sizeof(emulator->screen));

	emulator->draw_flag=false;
	emulator->keys=0;

	emulator->_pc = MEMORY_START;
	emulator->_i = 0;
	emulator->_sp = 0;

	emulator->_delay_timer=0;
	emulator->_sound_timer=0;

	// Carrega a fonte
	memcpy(emulator->_memory, chip8_fontset, sizeof(chip8_fontset));

	// Gambiarra? Sim. É uma porcaria de gerador de número aleatório? Sim. Alguém liga -- ainda
	// mais considerando um sistema da década de 70? ABSOLUTAMENTE NÃO!
	srand(time(NULL));
}

static void load_rom(struct emulator* emulator, const char* file) {
	FILE* rom = fopen(file, "rb");
	if (rom == NULL) {
		perror("Failed to open ROM");
		exit(EXIT_FAILURE);
	}

	// Pega o tamanho da ROM
	long rom_size;
		if (fseek(rom, 0, SEEK_END) != 0 ||
		(rom_size = ftell(rom)) < 0 ||
		fseek(rom, 0, SEEK_SET) != 0) {
		perror("ROM seek failed");
		fclose(rom);
		exit(EXIT_FAILURE);
	}


	if (rom_size > MEMORY_SIZE-MEMORY_START) {
		show_error_message("ROM file too big.\n");
		exit(EXIT_FAILURE);
	}

	const size_t n = fread(emulator->_memory + MEMORY_START, 1, rom_size, rom);

	if (n != (size_t)rom_size) {
		perror("Error reading ROM");
		exit(EXIT_FAILURE);
	}

	fclose(rom);
}

void emulator_init(struct emulator* emulator, const char* rom) {
	reset_emulator(emulator);
	load_rom(emulator, rom);
}

int emulator_cycle(struct emulator* emulator) {
	// Assert possívelmente útil
	assert(emulator->_sp < STACK_SIZE);

	if (emulator->_pc >= MEMORY_SIZE-1) {
		show_error_message("Error: program counter (PC) exceeds the memory amount.\n");
		return 1;
	}

	// Opcode
	const uint16_t opcode = emulator->_memory[emulator->_pc] << 8 | emulator->_memory[emulator->_pc + 1];

	const uint8_t x   = (opcode >> 8) & 0x000F; // Os 4 bits menores do byte alto
	const uint8_t y   = (opcode >> 4) & 0x000F; // Os 4 bits maiores do byte baixo
	const uint8_t n   = opcode & 0x000F; // Os 4 bits menores
	const uint8_t kk  = opcode & 0x00FF; // Os 8 bits menores
	const uint16_t nnn = opcode & 0x0FFF; // Os 12 bits menores

	// Asserts que só vão servir se eu for otário e tiver lascado as linhas acima
	assert(x < 16);
	assert(y < 16);
	assert(n < 16);
	assert(nnn <= 0xFFF);

/*#ifdef DEBUG 
	printf("PC: 0x%04X Op: 0x%04x\n", PC, opcode);
#endif*/

	switch (opcode & 0xF000) {
	case 0x0000:
		switch (kk) {
		// 00E0 => CLS. 
		// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#00E0
		case 0x00E0:
			p("CLS\n");

			memset(emulator->screen, 0, sizeof(emulator->screen));
			emulator->draw_flag=true;
			emulator->_pc+=2;
			break;
		// 00EE => RET
		// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#00EE
		case 0x00EE:
			p("RET\n");

			// Out-of-bounds
			if (emulator->_sp <= 0) {
				show_error_message("Error: stack pointer smaller than zero.\n");
				return 1;
			}
			emulator->_pc=emulator->_stack[--emulator->_sp];
			break;
		// Instrução ignorada
		default:
			break;
		}
		break;
	// 1nnn => JP addr
	// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#1nnn
	case 0x1000:
		p("JP 0x%03X\n", nnn);

		emulator->_pc=nnn;
		break;
	// 2nnn => CALL addr
	// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#2nnn
	case 0x2000:
		p("CALL 0x%03X\n", nnn);

		// Stack overflow
		if ((long unsigned)emulator->_sp+1 >= ARRAY_LEN(emulator->_stack)) {
			show_error_message("Error: stack overflow (SP=%d).\n", emulator->_sp);
			return 1;
		}

		emulator->_stack[emulator->_sp] = emulator->_pc+2;
		emulator->_sp++;
		emulator->_pc=nnn;

		break;
	// 3xkk => SE Vx, byte
	// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#3xkk
	case 0x3000:
		p("SE V%X, $%02X\n", x, kk);

		if (emulator->_v[x] == kk) {
			emulator->_pc+=2;
		}

		// Tem que pular a instrução pra próxima.
		emulator->_pc+=2;
		break;
	// 4xkk => SNE Vx, byte
	// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#4xkk
	case 0x4000:
		p("SNE V%X, $%02X\n", x, kk);

		if (emulator->_v[x] != kk) {
			emulator->_pc+=2;
		}

		// Tem que pular a instrução pra próxima.
		emulator->_pc+=2;
		break;
	// 5xy0 => SE Vx, Vy
	// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#5xy0
	case 0x5000:
		// Checa se o opcode está correto e o último valor é zero.
		if (n != 0) {
			unknown_opcode(opcode);
			return 1;
		}
		p("SNE V%X, V%X\n", x, y);
		if (emulator->_v[x] == emulator->_v[y]) {
			emulator->_pc+=2;
		}

		// Tem que pular a instrução pra próxima.
		emulator->_pc+=2;
		break;
	// 6xkk => LD Vx, byte
	// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#6xkk
	case 0x6000:
		p("LD V%X, %02X\n", x, kk);

		emulator->_v[x]=kk;
		emulator->_pc+=2;
		break;
	// 7xkk => ADD Vx, byte
	// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#7xkk
	case 0x7000:
		p("ADD V%X, %02X\n", x, kk);

		emulator->_v[x]+=kk;
		emulator->_pc+=2;
		break;
	case 0x8000:
		switch (n) { // Verifica o último bit
		// 8xy0 => LD Vx, Vy
		// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#8xy0
		case 0x0:
			p("LD  V%X, V%X\n", x, y);
			emulator->_v[x]=emulator->_v[y];
			break;
		// 8xy1 => OR Vx, Vy
		// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#8xy1
		case 0x1:
			p("OR  V%X, V%X\n", x, y);
			emulator->_v[x]|=emulator->_v[y];
			break;
		// 8xy2 => AND Vx, Vy
		// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#8xy2
		case 0x2:
			p("AND  V%X, V%X\n", x, y);
			emulator->_v[x]&=emulator->_v[y];
			break;
		// 8xy3 => XOR Vx, Vy
		// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#8xy3
		case 0x3:
			p("XOR  V%X, V%X\n", x, y);
			emulator->_v[x]^=emulator->_v[y];
			break;
		// 8xy4 => ADD Vx, Vy
		// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#8xy4
		case 0x4:
			p("ADD  V%X, V%X\n", x, y);
			const uint8_t sum = emulator->_v[x]+emulator->_v[y];

			// Deu carry
			if (sum < emulator->_v[x]) {
				emulator->_v[0xF]=1;
			} else {
				emulator->_v[0xF]=0;
			}

			emulator->_v[x]+=emulator->_v[y];
			break;
		// 8xy5 => SUB Vx, Vy
		// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#8xy5
		case 0x5:
			p("SUB  V%X, V%X\n", x, y);

			// Deu carry
			if (emulator->_v[x] > emulator->_v[y]) {
				emulator->_v[0xF]=1;
			} else {
				emulator->_v[0xF]=0;
			}

			emulator->_v[x]-=emulator->_v[y];
			break;
		// 8xy6 => SHR Vx {, Vy}
		// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#8xy6
		case 0x6:
			p("SHR V%X\n", x);

			emulator->_v[0xF]=emulator->_v[x]&0x1;
			emulator->_v[x]>>=1;
			break;
		// 8xy7 => SUBN Vx, Vy
		// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#8xy7
		case 0x7:
			p("SUBN  V%X, V%X\n", x, y);

			if (emulator->_v[y] > emulator->_v[x]) {
				emulator->_v[0xF]=1;
			} else {
				emulator->_v[0xF]=0;
			}

			emulator->_v[x]=emulator->_v[y]-emulator->_v[x];
			break;
		// 8xyE => SHL Vx {, Vy}
		// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#8xyE
		case 0xE:
			p("SHL V%X\n", x);

		
			emulator->_v[0xF]=(emulator->_v[x] >> 7) & 0x01;
			emulator->_v[x]<<=1;
			break;
		default:
			unknown_opcode(opcode);
			return 1;
		}

		emulator->_pc+=2;
		break;
	// 9xy0 => SNE Vx, Vy
	// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#9xy0
	case 0x9000:
		p("SNE  V%X, V%X\n", x, y);

		// Checa se o opcode está correto e o último valor é zero.
		if (n != 0) {
			unknown_opcode(opcode);
			return 1;
		}

		if (emulator->_v[x] != emulator->_v[y]) {
			emulator->_pc+=2;
		}

		// Tem que pular a instrução pra próxima.
		emulator->_pc+=2;
		break;
	// Annn => LD I, addr
	// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#Annn
	case 0xA000:
		p("LD  I, %03X\n", nnn);

		emulator->_i=nnn;

		emulator->_pc+=2;
		break;
	// Bnnn => JP V0, addr
	// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#Bnnn
	case 0xB000:
		p("JP V0, %03X", nnn);

		emulator->_pc = nnn+emulator->_v[0];
		break;
	// Cxkk - RND Vx, byte
	// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#Cxkk
	case 0xC000:
		p("RND V%X, %02X", x, kk);

		emulator->_v[x]=(rand()%256) & kk;

		emulator->_pc+=2;
		break;
	// Dxyn => DRW Vx, Vy, nibble
	// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#Cxyn
	case 0xD000:
		p("DRW V%X, V%X, %X\n", x, y, n);

		// Parte chata
		const uint8_t x0 = emulator->_v[x]%EMULATOR_WIDTH;
		const uint8_t y0 = emulator->_v[y]%EMULATOR_HEIGHT;
		const uint8_t height = n;

		// Não colidiu
		emulator->_v[0xF]=0;

		for (uint8_t row=0; row<height; row++) {
			if (emulator->_i + row >= MEMORY_SIZE) {
				show_error_message("Error: sprite read out of bounds.\n");
				return 1;
			}
			const uint8_t sprite = emulator->_memory[emulator->_i+row];
			const uint8_t y = (y0+row) % EMULATOR_HEIGHT;

			for (uint8_t col=0; col<8; col++) {
				if ((sprite & (0x80 >> col)) == 0) {
            				continue;
				}
				
				const uint8_t x = (x0 + col) % EMULATOR_WIDTH;

				const uint8_t byte_index = y * (EMULATOR_WIDTH/8) + (x/8);
				const uint8_t bit_mask = 0x80 >> (x % 8);

				// Detecta colisão
				if (emulator->screen[byte_index] & bit_mask) {
					emulator->_v[0xF] = 1;
				}

				// XOR no pixel
				emulator->screen[byte_index] ^= bit_mask;
			}
		}

		emulator->draw_flag=true;

		emulator->_pc+=2;
		break;
	case 0xE000:
		switch (kk) {
		// Ex9E => SKP Vx
		// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#Ex9E
		case 0x9E:
			p("SKP V%X\n", x);

			// Não válido
			if (emulator->_v[x] >= 16) {
				show_error_message("Invalid key code: 0x%02X. Must be smaller than 0xF (16).\n", emulator->_v[x]);
				return 1;
			}

			if (emulator->keys & (1 << emulator->_v[x])) {
				emulator->_pc+=2;
			}

			// Tem que pular a instrução pra próxima.
			emulator->_pc+=2;
			break;
		// ExA1 => SKNP Vx
		// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#ExA1
		case 0xA1:
			p("SKNP V%X\n", x);

			// Não válido
			if (emulator->_v[x] >= 16) {
				show_error_message("Invalid key code: 0x%02X. Must be smaller than 0xF (16).\n", emulator->_v[x]);
				return 1;
			}

			if (!(emulator->keys & (1 << emulator->_v[x]))) {
				emulator->_pc+=2;
			}

			// Tem que pular a instrução pra próxima.
			emulator->_pc+=2;
			break;
		default:
			unknown_opcode(opcode);
		}
		break;
	case 0xF000:
		switch (kk) {
		// Fx07 - LD Vx, DT
		// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#Fx07
		case 0x07:
			p("LD V%X, DT\n", x);

			emulator->_v[x] = emulator->_delay_timer;

			emulator->_pc+=2;
			break;
		// Fx0A - LD Vx, K
		// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#Fx0A
		case 0x0A:
			p("LD V%X, K\n", x);

			bool pressed=false;
			for (uint8_t k=0; k<16; k++) {
				if (emulator->keys & (1 << k)) {
					emulator->_v[x]=k;
					pressed=true;
					break;
				}
			}

			if (pressed) {
				emulator->_pc+=2;
			}
			break;
		// Fx15 => LD DT, Vx
		// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#Fx15
		case 0x15:
			p("LD DT, V%X\n", x);

			emulator->_delay_timer = emulator->_v[x];
			emulator->_pc+=2;
			break;
		// Fx18 => LD DT, Vx
		// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#Fx18
		case 0x18:
			p("LD ST, V%X\n", x);

			emulator->_sound_timer = emulator->_v[x];
			emulator->_pc+=2;
			break;
		// Fx1E => ADD I, Vx
		// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#Fx1E
		case 0x1E:
			p("ADD I, V%X\n", x);

			// Alguns emuladores colocam a flag em VF. Esse não é um deles.
			emulator->_i+=emulator->_v[x];
			emulator->_pc+=2;
			break;
		// Fx29 => LD F, Vx
		// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#Fx29
		// Coloca a fonte da letra em Vx em I
		case 0x29:
			p("LD F, V%X\n", x);

			if (emulator->_v[x] >= 16) {
				show_error_message("Error: invalid font character: 0x%02x. Must be smaller than 0xF (16).\n", emulator->_v[x]);
				return 1;
			}

			// Cada fonte tem 5 bytes e estão localizadas no início da memória.
			emulator->_i = emulator->_v[x]*5;
			emulator->_pc+=2;
			break;
		// Fx33 => LD B, Vx
		// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#Fx33
		// Armazena o valor de Vx em I como decimal codificado em binário
		case 0x33:
			p("LD B, V%X\n", x);

			if (emulator->_i+2 >= MEMORY_SIZE) {
				show_error_message("Error: instruction LD B, Vx with I=%04X exceeds the memory size.\n", emulator->_i);
				return 1;
			}

			emulator->_memory[emulator->_i]=emulator->_v[x]/100; // Centena
			emulator->_memory[emulator->_i+1]=(emulator->_v[x]/10) % 10; // Dezena
			emulator->_memory[emulator->_i+2]=emulator->_v[x] % 10; // Unidade

			emulator->_pc+=2;
			break;
		// Fx55 => LD [I], Vx
		// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#Fx55
		case 0x55:
			p("LD [I], V%X\n", x);

			if (emulator->_i + x >= MEMORY_SIZE) {
				show_error_message("Error: instruction LD [I], Vx with I=%04X and V%X exceeds the memory size.\n", emulator->_i, x);
				return 1;
			}

			for (uint8_t i=0; i<=x; i++) {
				emulator->_memory[emulator->_i+i]=emulator->_v[i];
			}

			//emulator->_i+=x+1;
			emulator->_pc+=2;
			break;
		// Fx65 => LD Vx, [I]
		// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#Fx65
		case 0x65:
			p("LD V%X, [I]\n", x);

			if (emulator->_i + x >= MEMORY_SIZE) {
				show_error_message("Error: instruction LD Vx, [I] with I=%04X and V%X exceeds the memory size.\n", emulator->_i, x);
				return 1;
			}

			for (uint8_t i=0; i<=x; i++) {
				emulator->_v[i]=emulator->_memory[emulator->_i+i];
			}

			// emulator->_i+=x+1;
			emulator->_pc+=2;
			break;
		default:
			unknown_opcode(opcode);
			return 1;
		}
		break;
	default:
		unknown_opcode(opcode);
		return 1;
	}

	return 0;
}

void emulator_tick(struct emulator* emulator) {
	emulator->draw_flag=false;

	for (size_t i=0; i<emulator->cycles_per_frame; i++) {
		if (emulator_cycle(emulator) != 0) {
		// Em testes, a função deve ser avançada não importa qual seja
#ifndef TEST
			exit(EXIT_FAILURE);
#else
			emulator->_pc+=2;
#endif
		}
	}
	if (emulator->_delay_timer > 0) {
		emulator->_delay_timer--;
	}
	if (emulator->_sound_timer > 0) {
		emulator->_sound_timer--;
	}
}