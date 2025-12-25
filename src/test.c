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

#include "unity.h"
#include "emulator.h"
#include <string.h>

struct emulator emu;

void setUp(void) {
	// Inicializa manualmente ou chama sua função de reset
	// Usaremos uma versão simplificada da lógica de reset_emulator
	memset(&emu, 0, sizeof(struct emulator));
	emu._pc = 0x200; 
	emu.cycles_per_frame = 16;
}

void tearDown(void) {
	// Limpeza após cada teste, se necessário
}

// Auxiliar para carregar um opcode de 2 bytes na memória na posição atual do PC
void load_opcode(uint16_t opcode) {
	emu._memory[emu._pc] = (opcode >> 8) & 0xFF;
	emu._memory[emu._pc + 1] = opcode & 0xFF;
}

// --- Testes ---

void test_opcode_6xkk_sets_register(void) {
	// LD V1, 0x55 (Carrega 0x55 no registro V1)
	load_opcode(0x6155);
	
	int result = emulator_cycle(&emu);
	
	TEST_ASSERT_EQUAL_INT(0, result);
	TEST_ASSERT_EQUAL_UINT8(0x55, emu._v[1]);
	TEST_ASSERT_EQUAL_UINT16(0x202, emu._pc);
}

void test_opcode_1nnn_jumps_to_address(void) {
	// JP 0xABC (Pula para o endereço 0xABC)
	load_opcode(0x1ABC);
	
	emulator_cycle(&emu);
	
	TEST_ASSERT_EQUAL_UINT16(0xABC, emu._pc);
}

void test_opcode_8xy4_adds_with_carry(void) {
	// Configuração: V0 = 0xFE, V1 = 0x03
	emu._v[0] = 0xFE;
	emu._v[1] = 0x03;
	
	// ADD V0, V1 (0x8014)
	load_opcode(0x8014);
	
	emulator_cycle(&emu);
	
	// 0xFE + 0x03 = 0x101. O resultado deve ser 0x01, Carry (VF) deve ser 1
	TEST_ASSERT_EQUAL_UINT8(0x01, emu._v[0]);
	TEST_ASSERT_EQUAL_UINT8(1, emu._v[0xF]); 
}

void test_opcode_8xy4_adds_no_carry(void) {
	emu._v[0] = 0x10;
	emu._v[1] = 0x10;
	load_opcode(0x8014);
	
	emulator_cycle(&emu);
	
	TEST_ASSERT_EQUAL_UINT8(0x20, emu._v[0]);
	TEST_ASSERT_EQUAL_UINT8(0, emu._v[0xF]);
}

void test_opcode_2nnn_and_00EE_call_return(void) {
	// 1. CALL 0x300 (Chama sub-rotina no endereço 0x300)
	load_opcode(0x2300);
	uint16_t next_instr = emu._pc + 2;
	
	emulator_cycle(&emu);
	
	TEST_ASSERT_EQUAL_UINT16(0x300, emu._pc);
	TEST_ASSERT_EQUAL_INT(1, emu._sp);
	TEST_ASSERT_EQUAL_UINT16(next_instr, emu._stack[0]);

	// 2. RET (Retorna da sub-rotina)
	load_opcode(0x00EE);
	emulator_cycle(&emu);

	TEST_ASSERT_EQUAL_UINT16(next_instr, emu._pc);
	TEST_ASSERT_EQUAL_INT(0, emu._sp);
}

void test_opcode_dxyn_draw_sets_collision(void) {
	// Configura um pixel no buffer da tela
	// A largura geralmente é 64, então byte 0, bit 0x80 é a coordenada (0,0)
	emu.screen[0] = 0x80; 
	emu._v[0] = 0; // x
	emu._v[1] = 0; // y
	emu._i = 0x400;
	emu._memory[0x400] = 0x80; // Sprite: 10000000
	
	// DRW V0, V1, 1 (Desenha sprite)
	load_opcode(0xD011);
	
	emulator_cycle(&emu);
	
	// Fazer XOR de 0x80 com 0x80 deve resultar em 0 (colisão detectada)
	TEST_ASSERT_EQUAL_UINT8(0, emu.screen[0]);
	TEST_ASSERT_EQUAL_UINT8(1, emu._v[0xF]);
	TEST_ASSERT_TRUE(emu.draw_flag);
}

// --- Testes de Segurança e Tratamento de Erros ---

void test_stack_overflow_protection(void) {
	// Preenche a pilha até o limite
	emu._sp = STACK_SIZE - 1; 
	
	// Tenta outro CALL 0x300 (0x2300)
	emu._memory[emu._pc] = 0x23;
	emu._memory[emu._pc + 1] = 0x00;

	int result = emulator_cycle(&emu);

	// Deve retornar 1 em vez de travar ou sobrescrever memória
	TEST_ASSERT_EQUAL_INT(1, result);
}

void test_stack_underflow_protection(void) {
	emu._sp = 0;

	// Tenta um RET (0x00EE) sem ter feito um CALL
	emu._memory[emu._pc] = 0x00;
	emu._memory[emu._pc + 1] = 0xEE;

	int result = emulator_cycle(&emu);

	TEST_ASSERT_EQUAL_INT(1, result);
}

void test_pc_out_of_bounds_protection(void) {
	// Define o PC no final da memória
	emu._pc = MEMORY_SIZE - 1;

	int result = emulator_cycle(&emu);

	// PC + 1 estaria fora dos limites, deve retornar erro
	TEST_ASSERT_EQUAL_INT(1, result);
}

void test_invalid_opcode_5xyN_format(void) {
	// 5xy0 é válido, mas 5xy1 não é.
	// Tentando 0x5121
	emu._memory[emu._pc] = 0x51;
	emu._memory[emu._pc + 1] = 0x21;

	int result = emulator_cycle(&emu);

	TEST_ASSERT_EQUAL_INT(1, result);
}

void test_fx33_bcd_out_of_bounds(void) {
	// LD B, Vx armazena 3 bytes em I, I+1, I+2
	// Define I no último byte da memória
	emu._i = MEMORY_SIZE - 1;
	emu._v[0] = 123;
	
	// Opcode F033
	emu._memory[emu._pc] = 0xF0;
	emu._memory[emu._pc + 1] = 0x33;

	int result = emulator_cycle(&emu);

	TEST_ASSERT_EQUAL_INT(1, result);
}

void test_fx55_reg_dump_out_of_bounds(void) {
	// LD [I], V5 tentará escrever 6 bytes (V0 até V5)
	// Define I de forma que o 6º byte fique fora dos limites
	emu._i = MEMORY_SIZE - 3;
	
	// Opcode F555
	emu._memory[emu._pc] = 0xF5;
	emu._memory[emu._pc + 1] = 0x55;

	int result = emulator_cycle(&emu);

	TEST_ASSERT_EQUAL_INT(1, result);
}

void test_sprite_draw_out_of_bounds_memory(void) {
	// Define I em um endereço onde ler 'n' bytes excede a memória
	emu._i = MEMORY_SIZE - 2;
	emu._v[0] = 0;
	emu._v[1] = 0;

	// DRW V0, V1, 5 (Tenta ler 5 bytes da memória começando em I)
	emu._memory[emu._pc] = 0xD0;
	emu._memory[emu._pc + 1] = 0x15;

	int result = emulator_cycle(&emu);

	TEST_ASSERT_EQUAL_INT(1, result);
}

// --- 1. Instruções de Salto (Pulo) (3xkk, 4xkk, 5xy0, 9xy0) ---

void test_opcode_3xkk_skips_when_equal(void) {
	emu._v[2] = 0x44;
	load_opcode(0x3244); // SE V2, 0x44 (Pula se V2 == 0x44)
	emulator_cycle(&emu);
	// Deve pular: PC atual (0x200) + tamanho do opcode (2) + pulo (2) = 0x204
	TEST_ASSERT_EQUAL_UINT16(0x204, emu._pc);
}

void test_opcode_3xkk_does_not_skip_when_unequal(void) {
	emu._v[2] = 0x11;
	load_opcode(0x3244); 
	emulator_cycle(&emu);
	TEST_ASSERT_EQUAL_UINT16(0x202, emu._pc);
}

// --- 2. Lógica e Aritmética (8xy1, 8xy2, 8xy3, 8xy6, 8xyE) ---

void test_opcode_8xy1_OR(void) {
	emu._v[1] = 0x0F;
	emu._v[2] = 0xF0;
	load_opcode(0x8121); // OR V1, V2
	emulator_cycle(&emu);
	TEST_ASSERT_EQUAL_UINT8(0xFF, emu._v[1]);
}

void test_opcode_8xy2_AND(void) {
	emu._v[1] = 0xCC; // 1100 1100
	emu._v[2] = 0xAA; // 1010 1010
	load_opcode(0x8122); 
	emulator_cycle(&emu);
	/* 0xCC & 0xAA = 0x88 (1000 1000) */
	TEST_ASSERT_EQUAL_UINT8(0x88, emu._v[1]);
}

void test_opcode_8xy6_SHR(void) {
	emu._v[3] = 0x03; // 0000 0011
	load_opcode(0x8306); // SHR V3 (Shift right)
	emulator_cycle(&emu);
	// V3 torna-se 1, VF torna-se 1 (o bit que "caiu")
	TEST_ASSERT_EQUAL_UINT8(0x01, emu._v[3]);
	TEST_ASSERT_EQUAL_UINT8(0x01, emu._v[0xF]);
}

void test_opcode_8xyE_SHL(void) {
	emu._v[3] = 0x81; // 1000 0001
	load_opcode(0x830E); // SHL V3 (Shift left)
	emulator_cycle(&emu);
	// V3 torna-se 0x02, VF torna-se 1 (o bit que saiu pelo topo)
	TEST_ASSERT_EQUAL_UINT8(0x02, emu._v[3]);
	TEST_ASSERT_EQUAL_UINT8(0x01, emu._v[0xF]);
}

// --- 3. Memória e Indexação (Annn, Fx33, Fx55, Fx65) ---

void test_opcode_Annn_sets_I(void) {
	load_opcode(0xA123); // LD I, 0x123 (Define o registro de índice I)
	emulator_cycle(&emu);
	TEST_ASSERT_EQUAL_UINT16(0x123, emu._i);
}

void test_opcode_Fx33_BCD(void) {
	emu._v[0] = 159;
	emu._i = 0x400;
	load_opcode(0xF033); // LD B, V0 (Armazena representação BCD de V0 na memória)
	emulator_cycle(&emu);
	
	TEST_ASSERT_EQUAL_UINT8(1, emu._memory[0x400]);
	TEST_ASSERT_EQUAL_UINT8(5, emu._memory[0x401]);
	TEST_ASSERT_EQUAL_UINT8(9, emu._memory[0x402]);
}

void test_opcode_Fx55_register_dump(void) {
	emu._i = 0x500;
	emu._v[0] = 0xAA;
	emu._v[1] = 0xBB;
	emu._v[2] = 0xCC;
	load_opcode(0xF255); // LD [I], V2 (Despeja V0, V1, V2 na memória)
	
	emulator_cycle(&emu);
	
	TEST_ASSERT_EQUAL_UINT8(0xAA, emu._memory[0x500]);
	TEST_ASSERT_EQUAL_UINT8(0xBB, emu._memory[0x501]);
	TEST_ASSERT_EQUAL_UINT8(0xCC, emu._memory[0x502]);
}

void test_opcode_Fx65_register_load(void) {
	emu._i = 0x600;
	emu._memory[0x600] = 0x11;
	emu._memory[0x601] = 0x22;
	load_opcode(0xF165); // LD V1, [I] (Carrega registros V0 e V1 da memória)
	
	emulator_cycle(&emu);
	
	TEST_ASSERT_EQUAL_UINT8(0x11, emu._v[0]);
	TEST_ASSERT_EQUAL_UINT8(0x22, emu._v[1]);
}

void test_timer_decrement(void) {
	emu._delay_timer = 2;
	emu._sound_timer = 2;

	emulator_tick(&emu); // Primeiro tique (decremento dos timers)
	TEST_ASSERT_EQUAL_UINT8(1, emu._delay_timer);
	TEST_ASSERT_EQUAL_UINT8(1, emu._sound_timer);

	emulator_tick(&emu); // Segundo tique
	TEST_ASSERT_EQUAL_UINT8(0, emu._delay_timer);
	TEST_ASSERT_EQUAL_UINT8(0, emu._sound_timer);

	emulator_tick(&emu); // Deve permanecer em zero, não dar "wrap" para 255
	TEST_ASSERT_EQUAL_UINT8(0, emu._delay_timer);
	TEST_ASSERT_EQUAL_UINT8(0, emu._sound_timer);
}

void test_opcode_Fx07_reads_delay_timer(void) {
	emu._delay_timer = 0x42;
	load_opcode(0xF107); // LD V1, DT (Lê o valor do delay timer para V1)
	emulator_cycle(&emu);
	TEST_ASSERT_EQUAL_UINT8(0x42, emu._v[1]);
}

void test_opcode_Fx0A_halts_until_keypress(void) {
	emu.keys = 0; // Nenhuma tecla pressionada
	load_opcode(0xF10A); // LD V1, K (Aguarda pressionamento de tecla)
	
	uint16_t initial_pc = emu._pc;
	emulator_cycle(&emu);
	
	// O PC NÃO deve ter avançado porque nenhuma tecla foi pressionada
	TEST_ASSERT_EQUAL_UINT16(initial_pc, emu._pc);

	emu.keys = (1 << 5); // Simula pressão da tecla 5
	emulator_cycle(&emu);
	
	// Agora o PC deve ter avançado, e V1 deve conter o valor 5
	TEST_ASSERT_EQUAL_UINT16(initial_pc + 2, emu._pc);
	TEST_ASSERT_EQUAL_UINT8(5, emu._v[1]);
}

void test_opcode_Fx29_font_character_pointer(void) {
	// Testa o caractere '0' (deve estar no índice 0 da memória de fontes)
	emu._v[0] = 0x0;
	load_opcode(0xF029); 
	emulator_cycle(&emu);
	TEST_ASSERT_EQUAL_UINT16(0, emu._i);

	// Testa o caractere 'A' (10º caractere). 10 * 5 bytes por sprite = 50 (0x32)
	emu._pc = 0x200; // Reseta o PC para o próximo teste
	emu._v[0] = 0xA;
	load_opcode(0xF029);
	emulator_cycle(&emu);
	TEST_ASSERT_EQUAL_UINT16(50, emu._i);
}

void test_chained_skips(void) {
	// Configuração: V0=1, V1=1. 
	// Programa:
	// 0x200: 3001 (SE V0, 1) -> Isso deve causar um pulo
	// 0x202: 3101 (SE V1, 1) -> Esta é a instrução que será pulada
	// 0x204: 62FF (LD V2, 0xFF) -> É aqui que devemos terminar
	
	emu._v[0] = 1;
	emu._v[1] = 1;
	load_opcode(0x3001); 
	
	emulator_cycle(&emu);
	
	// O PC deve estar em 0x204, tendo saltado sobre a instrução em 0x202
	TEST_ASSERT_EQUAL_UINT16(0x204, emu._pc);
}

int main(void) {
	UNITY_BEGIN();
	RUN_TEST(test_opcode_6xkk_sets_register);
	RUN_TEST(test_opcode_1nnn_jumps_to_address);
	RUN_TEST(test_opcode_8xy4_adds_with_carry);
	RUN_TEST(test_opcode_8xy4_adds_no_carry);
	RUN_TEST(test_opcode_2nnn_and_00EE_call_return);
	RUN_TEST(test_opcode_dxyn_draw_sets_collision);
	RUN_TEST(test_stack_overflow_protection);
	RUN_TEST(test_stack_underflow_protection);
	RUN_TEST(test_pc_out_of_bounds_protection);
	RUN_TEST(test_invalid_opcode_5xyN_format);
	RUN_TEST(test_fx33_bcd_out_of_bounds);
	RUN_TEST(test_sprite_draw_out_of_bounds_memory);
	RUN_TEST(test_opcode_3xkk_skips_when_equal);
	RUN_TEST(test_opcode_3xkk_does_not_skip_when_unequal);
	RUN_TEST(test_opcode_8xy1_OR);
	RUN_TEST(test_opcode_8xy2_AND);
	RUN_TEST(test_opcode_8xy6_SHR);
	RUN_TEST(test_opcode_8xyE_SHL);
	RUN_TEST(test_opcode_Annn_sets_I);
	RUN_TEST(test_opcode_Fx33_BCD);
	RUN_TEST(test_opcode_Fx55_register_dump);
	RUN_TEST(test_opcode_Fx65_register_load);
	RUN_TEST(test_timer_decrement);
	RUN_TEST(test_opcode_Fx07_reads_delay_timer);
	RUN_TEST(test_opcode_Fx0A_halts_until_keypress);
	RUN_TEST(test_opcode_Fx29_font_character_pointer);
	RUN_TEST(test_chained_skips);

	return UNITY_END();
}