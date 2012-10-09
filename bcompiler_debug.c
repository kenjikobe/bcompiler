/*
   +----------------------------------------------------------------------+
   | PHP bcompiler                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2010 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Alan Knowles <alan@akbkhome.com>                            |
   |          val khokhlov <val@php.net>                                  |
   +----------------------------------------------------------------------+
 */

/* $Id: bcompiler_debug.c 305695 2010-11-23 20:01:22Z val $ */

#include "php_bcompiler.h"

#ifdef BCOMPILER_DEBUG_ON

static const char* opcodes[] = {
	"NOP", /*  0 */
	"ADD", /*  1 */
	"SUB", /*  2 */
	"MUL", /*  3 */
	"DIV", /*  4 */
	"MOD", /*  5 */
	"SL", /*  6 */
	"SR", /*  7 */
	"CONCAT", /*  8 */
	"BW_OR", /*  9 */
	"BW_AND", /*  10 */
	"BW_XOR", /*  11 */
	"BW_NOT", /*  12 */
	"BOOL_NOT", /*  13 */
	"BOOL_XOR", /*  14 */
	"IS_IDENTICAL", /*  15 */
	"IS_NOT_IDENTICAL", /*  16 */
	"IS_EQUAL", /*  17 */
	"IS_NOT_EQUAL", /*  18 */
	"IS_SMALLER", /*  19 */
	"IS_SMALLER_OR_EQUAL", /*  20 */
	"CAST", /*  21 */
	"QM_ASSIGN", /*  22 */
	"ASSIGN_ADD", /*  23 */
	"ASSIGN_SUB", /*  24 */
	"ASSIGN_MUL", /*  25 */
	"ASSIGN_DIV", /*  26 */
	"ASSIGN_MOD", /*  27 */
	"ASSIGN_SL", /*  28 */
	"ASSIGN_SR", /*  29 */
	"ASSIGN_CONCAT", /*  30 */
	"ASSIGN_BW_OR", /*  31 */
	"ASSIGN_BW_AND", /*  32 */
	"ASSIGN_BW_XOR", /*  33 */
	"PRE_INC", /*  34 */
	"PRE_DEC", /*  35 */
	"POST_INC", /*  36 */
	"POST_DEC", /*  37 */
	"ASSIGN", /*  38 */
	"ASSIGN_REF", /*  39 */
	"ECHO", /*  40 */
	"PRINT", /*  41 */
	"JMP", /*  42 */
	"JMPZ", /*  43 */
	"JMPNZ", /*  44 */
	"JMPZNZ", /*  45 */
	"JMPZ_EX", /*  46 */
	"JMPNZ_EX", /*  47 */
	"CASE", /*  48 */
	"SWITCH_FREE", /*  49 */
	"BRK", /*  50 */
	"CONT", /*  51 */
	"BOOL", /*  52 */
	"INIT_STRING", /*  53 */
	"ADD_CHAR", /*  54 */
	"ADD_STRING", /*  55 */
	"ADD_VAR", /*  56 */
	"BEGIN_SILENCE", /*  57 */
	"END_SILENCE", /*  58 */
	"INIT_FCALL_BY_NAME", /*  59 */
	"DO_FCALL", /*  60 */
	"DO_FCALL_BY_NAME", /*  61 */
	"RETURN", /*  62 */
	"RECV", /*  63 */
	"RECV_INIT", /*  64 */
	"SEND_VAL", /*  65 */
	"SEND_VAR", /*  66 */
	"SEND_REF", /*  67 */
	"NEW", /*  68 */
	"JMP_NO_CTOR", /*  69 */
	"FREE", /*  70 */
	"INIT_ARRAY", /*  71 */
	"ADD_ARRAY_ELEMENT", /*  72 */
	"INCLUDE_OR_EVAL", /*  73 */
	"UNSET_VAR", /*  74 */
	"UNSET_DIM_OBJ", /*  75 */
	"ISSET_ISEMPTY", /*  76 */
	"FE_RESET", /*  77 */
	"FE_FETCH", /*  78 */
	"EXIT", /*  79 */
	"FETCH_R", /*  80 */
	"FETCH_DIM_R", /*  81 */
	"FETCH_OBJ_R", /*  82 */
	"FETCH_W", /*  83 */
	"FETCH_DIM_W", /*  84 */
	"FETCH_OBJ_W", /*  85 */
	"FETCH_RW", /*  86 */
	"FETCH_DIM_RW", /*  87 */
	"FETCH_OBJ_RW", /*  88 */
	"FETCH_IS", /*  89 */
	"FETCH_DIM_IS", /*  90 */
	"FETCH_OBJ_IS", /*  91 */
	"FETCH_FUNC_ARG", /*  92 */
	"FETCH_DIM_FUNC_ARG", /*  93 */
	"FETCH_OBJ_FUNC_ARG", /*  94 */
	"FETCH_UNSET", /*  95 */
	"FETCH_DIM_UNSET", /*  96 */
	"FETCH_OBJ_UNSET", /*  97 */
	"FETCH_DIM_TMP_VAR", /*  98 */
	"FETCH_CONSTANT", /*  99 */
#ifndef ZEND_ENGINE_2
	"DECLARE_FUNCTION_OR_CLASS", /*  100 */
#else
    "ZEND_GOTO", /*                            100 */
#endif
	"EXT_STMT", /*  101 */
	"EXT_FCALL_BEGIN", /*  102 */
	"EXT_FCALL_END", /*  103 */
	"EXT_NOP", /*  104 */
	"TICKS", /*  105 */
	"SEND_VAR_NO_REF", /*  106 */
	  
	"ZEND_CATCH", /*                        107 */
	"ZEND_THROW", /*                        108 */
	"ZEND_FETCH_CLASS", /*                  109 */
	"ZEND_CLONE", /*                        110 */
	"ZEND_INIT_CTOR_CALL", /*               111 */
	"ZEND_INIT_METHOD_CALL", /*             112 */
	"ZEND_INIT_STATIC_METHOD_CALL", /*      113 */
	"ZEND_ISSET_ISEMPTY_VAR", /*            114 */
	"ZEND_ISSET_ISEMPTY_DIM_OBJ", /*        115 */
	"ZEND_IMPORT_FUNCTION", /*              116 */
	"ZEND_IMPORT_CLASS", /*                 117 */
	"ZEND_IMPORT_CONST", /*                 118 */
	
	"**UNKNOWN**", /*                 119 */
	"**UNKNOWN**", /*                 120 */
	
	"ZEND_ASSIGN_ADD_OBJ", /*               121 */
	"ZEND_ASSIGN_SUB_OBJ", /*               122 */
	"ZEND_ASSIGN_MUL_OBJ", /*               123 */
	"ZEND_ASSIGN_DIV_OBJ", /*               124 */
	"ZEND_ASSIGN_MOD_OBJ", /*               125 */
	"ZEND_ASSIGN_SL_OBJ", /*                126 */
	"ZEND_ASSIGN_SR_OBJ", /*                127 */
	"ZEND_ASSIGN_CONCAT_OBJ", /*            128 */
	"ZEND_ASSIGN_BW_OR_OBJ", /*             129 */
	"END_ASSIGN_BW_AND_OBJ", /*             130 */
	"END_ASSIGN_BW_XOR_OBJ", /*             131 */
	"ZEND_PRE_INC_OBJ", /*                  132 */
	"ZEND_PRE_DEC_OBJ", /*                  133 */
	"ZEND_POST_INC_OBJ", /*                 134 */
	"ZEND_POST_DEC_OBJ", /*                 135 */
	"ZEND_ASSIGN_OBJ", /*                   136 */
#ifndef ZEND_ENGINE_2
	"ZEND_OP_DATA",	/*			137 */
	"BEGIN_HASH_DEF", /*			138 */
	"OPCODE_ARRAY"  /*			139 */
#else
	"ZEND_MAKE_VAR", /*                     137 */
	"ZEND_INSTANCEOF", /*			138 */
	"ZEND_DECLARE_CLASS", /*		139 */
	"ZEND_DECLARE_INHERITED_CLASS", /*	140 */
	"ZEND_DECLARE_FUNCTION", /*		141 */
	"ZEND_RAISE_ABSTRACT_ERROR", /*		142 */
	"ZEND_DECLARE_CONST", /*                 143 */
	"ZEND_ADD_INTERFACE", /*		144 */
	"ZEND_DECLARE_INHERITED_CLASS_DELAYED", /*                 145 */
	"ZEND_VERIFY_ABSTRACT_CLASS", /*	146 */
	"ZEND_ASSIGN_DIM", /*			147 */
	"ZEND_ISSET_ISEMPTY_PROP_OBJ", /*	148 */
	"ZEND_HANDLE_EXCEPTION", /*		149 */
    "ZEND_USER_OPCODE", /*                    150 */
    "ZEND_JMP_SET", /*                        152 */
    "ZEND_DECLARE_LAMBDA_FUNCTION" /*         153 */
#endif   
};

static const int NUM_OPCODES = sizeof(opcodes) / sizeof(opcodes[0]);

inline const char *bcompiler_opcode_name(int op) {
  return op < NUM_OPCODES ? opcodes[op] : "(unknown)";
}
/*#define getOpcodeName(op) \
	(op < NUM_OPCODES ? opcodes[op] : "(unknown)")*/

/* Debugging I/O */

static FILE *DF = NULL;

void bcompiler_vdebug(const char *s, va_list va) {
	TSRMLS_FETCH();
	if (!DF) {
		if (BCOMPILERG(debug_file) && *(BCOMPILERG(debug_file)))
			DF = fopen(BCOMPILERG(debug_file), "w");
			else DF = stderr;
	}
	if (DF) {
		fputs("[DEBUG] ", DF);
		vfprintf(DF, s, va);
	}
}

void bcompiler_debug(const char *s, ...) {
	va_list va;

	TSRMLS_FETCH();
	if (BCOMPILERG(debug_lvl) >= 1 && s) {
		va_start(va, s);
		bcompiler_vdebug(s, va);
		va_end(va);
	}
	if (DF) { if (!s) { fclose(DF); DF = NULL; } else fflush(DF); }
}

void bcompiler_debugfull(const char *s, ...) {
	va_list va;

	TSRMLS_FETCH();
	if (BCOMPILERG(debug_lvl) >= 2 && s) {
		va_start(va, s);
		bcompiler_vdebug(s, va);
		va_end(va);
	}
	if (DF) { if (!s) { fclose(DF); DF = NULL; } else fflush(DF); }
}

void bcompiler_dump(const char *s, size_t n) {
	size_t i = 0, k;
	char t[17], x[49];

	TSRMLS_FETCH();
	if (!DF || BCOMPILERG(debug_lvl) < 2) return;
	while (i < n) {
		fputs("[DEBUG] DUMP: ", DF);
		memset(x, ' ', 48);
		k = 0;
		while (k < 16 && i < n) {
			t[k] = (s[i] != 0 && s[i] != '\n' ? s[i] : '.');
			sprintf(x + 3*k, "%02X", s[i]);
			x[3*k + 2] = ' ';
			i++; k++;
		}
		t[k] = x[48] = '\0';
		fputs(x, DF);
		fputs(t, DF);
		fputs("\n", DF);
	}
}

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
