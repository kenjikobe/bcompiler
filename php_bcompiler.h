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

/* $Id: php_bcompiler.h 311909 2011-06-08 01:33:35Z alan_k $ */

#ifndef PHP_BCOMPILER_H
#define PHP_BCOMPILER_H

#define PHP_BCOMPILER_VERSION "1.0.2"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/file.h"
#include "zend.h"
#include "zend_extensions.h"
#include "zend_variables.h"	//  for zval_dtor() 
#ifdef ZTS
#include "TSRM.h"
#endif
#ifdef ZEND_ENGINE_2
#include "zend_API.h"
#endif

extern zend_module_entry bcompiler_module_entry;
#define phpext_bcompiler_ptr &bcompiler_module_entry

#ifdef PHP_WIN32
#define PHP_BCOMPILER_API __declspec(dllexport)
#else
#define PHP_BCOMPILER_API
#endif

/* Zend Engine versions */

#if (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION >= 1) || PHP_MAJOR_VERSION >= 6
#define ZEND_ENGINE_2_1
#include "zend_vm.h"
#endif

#if (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION >= 2) || PHP_MAJOR_VERSION >= 6
#define ZEND_ENGINE_2_2
#endif 

#if (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION >= 3) || PHP_MAJOR_VERSION >= 6
#define ZEND_ENGINE_2_3
#endif 

#if PHP_VERSION_ID >= 50399 || PHP_MAJOR_VERSION >= 6
#define ZEND_ENGINE_2_4
#endif 

/* 
  	Declare any global variables you may need between the BEGIN
	and END macros here:     
*/
ZEND_BEGIN_MODULE_GLOBALS(bcompiler)
	int enabled;                /* if true, bcompiler is enabled (defaults to true) */
	int debug_lvl;              /* debug level (0 = off, 1 = basic, 2 = full */
	char* debug_file;           /* debug file (NULL or "" for stderr) */
    php_stream *stream;         /* output stream  */
    char* buffer;		        /* our read buffer */
    char _buf[8];               /* buffer for fixed-size data i/o */
    unsigned int buffer_size;   /* our read buffer's current size */
    zval* callback;             /* callback for parser */
    zval* callback_value;       /* callback for grouped calls */
    char* callback_key;         /* callback key for associated vals*/
    int current_version;        /* current bytecode version */
	int is_unicode;             /* reading a unicode-enabled bytecodes */
    int current_write;          /* write bytecode version */
    int current_include;        /* read file after bcompiler_write_file() */
    int parsing_error;          /* i/o error while parsing, skip file */
    const size_t *bcompiler_stdsize;  /* array of standard data sizes */
	zend_class_entry *cur_zc;   /* current class entry */
	char* current_filename;     /* name of the bytecode file being loaded */
	zval* filename_handler;      /* filename handler function zval */
#if PHP_MAJOR_VERSION >= 6
	zval* filename_handler_name; /* filename handler function name */
#else
	char* filename_handler_name; /* filename handler function name */
#endif
	int detect_filedir;         /* try to detect __FILE__ and __DIR__ (default: true) */
	zend_op_array *zoa;         /* current zend_op_array */
ZEND_END_MODULE_GLOBALS(bcompiler)

/* we use globals */
ZEND_EXTERN_MODULE_GLOBALS(bcompiler)

#ifdef ZTS
#define BCOMPILERG(v) TSRMG(bcompiler_globals_id, zend_bcompiler_globals *, v)
#else
#define BCOMPILERG(v) (bcompiler_globals.v)
#endif

/* PHP6-specific macros */

#if PHP_MAJOR_VERSION >= 6
# define ZS2SR(zstr) ((zstr).s)
# define ZS2S(zstr) ((char*)((zstr).s))
# define ZS2SP(zstr) ((char**)&((zstr).s))
# define ZLEN(len)  (UG(unicode) ? UBYTES(len) : (len))
#else
/* R= raw, S=(char*) P= pointer.. */
# define ZS2SR(zstr) (zstr)
# define ZS2S(zstr) ((char*)zstr)
# define ZS2SP(zstr) ((char**)&zstr)
# define ZLEN(len)  (len)
#endif

/* Define the header & version */

#define BCOMPILER_VERSION PHP_BCOMPILER_VERSION
#define BCOMPILER_STREAMS "s"
#define BCOMPILER_UNICODE "u"
#ifndef PHP_HAVE_STREAMS
# error You lose - you must have stream support in your PHP
#endif
#define BCOMPILER_MAGIC_START "bcompiler v"
#define BCOMPILER_MAGIC_HEADER BCOMPILER_MAGIC_START  BCOMPILER_VERSION  BCOMPILER_STREAMS

/* Bytecode version parameters */

#if PHP_MAJOR_VERSION >= 6
#define BCOMPILER_CUR_VER  29 /* current bytecode version */
#define BCOMPILER_CAN_READ 10, 15, 19, 23, 29 /* can read bytecodes */
#elif defined(ZEND_ENGINE_2_4)
#define BCOMPILER_CUR_VER  28 /* current bytecode version */
#define BCOMPILER_CAN_READ 24, 28 /* can read bytecodes */
#elif defined(ZEND_ENGINE_2_3)
#define BCOMPILER_CUR_VER  27 /* current bytecode version */
#define BCOMPILER_CAN_READ 20, 22, 27 /* can read bytecodes */
#elif defined(ZEND_ENGINE_2_1)
#define BCOMPILER_CUR_VER  26 /* current bytecode version */
#define BCOMPILER_CAN_READ 7, 9, 11, 12, 14, 18, 21, 26 /* can read bytecodes */
#elif defined(ZEND_ENGINE_2)
#define BCOMPILER_CUR_VER  25 /* current bytecode version */
#define BCOMPILER_CAN_READ 4, 6, 8, 13, 17, 25 /* can read bytecodes */
#else
#define BCOMPILER_CUR_VER  30 /* current bytecode version */
#define BCOMPILER_CAN_READ 3, 5, 16, 30 /* can read bytecodes */
#endif

/* These are flags that define start points in the decompile code. */

#define BCOMPILER_CLASS_ENTRY         1 /* start of a class */
#define BCOMPILER_INCTABLE_ENTRY      2 /* list of included files */
#define BCOMPILER_FUNCTION_ENTRY      3 /* a function definition */
#define BCOMPILER_CONSTANT            4 /* a constant definition */
#define BCOMPILER_OP_ARRAY            9 /* raw opcodes array */
#define BCOMPILER_FUNCTION_ENTRY_EX  10 /* an extended function definition (including zend_hash key) */

/* Debugging */

#ifdef BCOMPILER_DEBUG_ON
# define BCOMPILER_DEBUG(args)  bcompiler_debug args
# define BCOMPILER_DEBUGFULL(args)  bcompiler_debug args
# define BCOMPILER_DUMPFULL bcompiler_dump
#else
# define BCOMPILER_DEBUG(args)  
# define BCOMPILER_DEBUGFULL(args)  
# define BCOMPILER_DUMPFULL(string,len)
# define bcompiler_opcode_name(op) ""
#endif

/* Standard scalar type sizes and debug format */

enum {
	BCSI_int, BCSI_long, BCSI_char, BCSI_double, BCSI_size_t,
	BCSI_uint, BCSI_ulong,
	BCSI_zend_uint, BCSI_zend_ushort, BCSI_zend_bool, BCSI_zend_uchar,
	BCSI_zend_ulong
     };

#define BCOMPILER_STDSIZE_03	4,4,4,8,4, 4,4, 4,4,4,4,4
#ifdef __x86_64__
#define BCOMPILER_STDSIZE_05	8,8,1,8,8, 8,8, 8,2,1,1,8
#else
#define BCOMPILER_STDSIZE_05	4,4,1,8,4, 4,4, 4,2,1,1,4
#endif

#define BCSD_int	"%08x"
#define BCSD_long	"%08x"
#define BCSD_char	"%02x"
#define BCSD_double	"%f"
#define BCSD_size_t	"%08x"

#define BCSD_uint	"%08x"
#define BCSD_ulong	"%08x"

#define BCSD_zend_uint	"%08x"
#define BCSD_zend_ushort "%04x"
#define BCSD_zend_bool	"%d"
#define BCSD_zend_uchar	"%02x"
#define BCSD_zend_ulong	"%08x"

/* Definitions that output stuff.  */

/* SERIALIZE_SCALAR: write a scalar value to dst */
#define SERIALIZE_SCALAR(x, type) {                        \
	if (BCOMPILERG(stream)  != NULL) {                     \
		BCOMPILER_DEBUGFULL(( "     scalar: " BCSD_##type " (size: %d, name: %s)\n", x, BCOMPILERG(bcompiler_stdsize)[BCSI_##type], #x ));  \
		memset(BCOMPILERG(_buf), 0, sizeof(BCOMPILERG(_buf))); \
		*((type*)BCOMPILERG(_buf)) = x;                        \
		php_stream_write(BCOMPILERG(stream), BCOMPILERG(_buf), BCOMPILERG(bcompiler_stdsize)[BCSI_##type]); \
	}                                                      \
}
/* DESERIALIZE_SCALAR: read a scalar value from src */
#define DESERIALIZE_SCALAR_X(xp, type, stype, dtype, code) {             \
	size_t act;                                                          \
	if (BCOMPILERG(parsing_error)) code;                                 \
	memset(BCOMPILERG(_buf), 0, sizeof(BCOMPILERG(_buf)));               \
	act = php_stream_read(BCOMPILERG(stream), BCOMPILERG(_buf), stype);  \
	if (act != stype) {                                                  \
		if (!BCOMPILERG(parsing_error)) php_error(E_WARNING, "bcompiler: Bad bytecode file format at %08x", (unsigned)php_stream_tell(BCOMPILERG(stream))); \
		BCOMPILERG(parsing_error) = 1;                                   \
		code;                                                            \
	}                                                                    \
	BCOMPILER_DEBUGFULL(( "     scalar: " dtype " (size: %d, name: %s)\n", *((type*)BCOMPILERG(_buf)), stype, #xp ));  \
	*(xp) = *((type*) BCOMPILERG(_buf));                                 \
	BCOMPILER_DEBUGFULL(( "     read : %i \n",(type) *(xp) ));           \
}
#define DESERIALIZE_SCALAR(xp, type)      DESERIALIZE_SCALAR_X(xp, type, BCOMPILERG(bcompiler_stdsize)[BCSI_##type], BCSD_##type, return)
#define DESERIALIZE_SCALAR_V(xp, type, v) DESERIALIZE_SCALAR_X(xp, type, BCOMPILERG(bcompiler_stdsize)[BCSI_##type], BCSD_##type, return v)

/* DESERIALIZE_VOID: skip a scalar value from src */
#define DESERIALIZE_VOID(type) {                                       \
	php_stream_read(BCOMPILERG(stream), BCOMPILERG(_buf), BCOMPILERG(bcompiler_stdsize)[BCSI_##type]); \
}

/* STORE_BYTES: memcpy wrapper, writes to dst buffer */
#define STORE_BYTES(bytes, n) {                         \
	if (BCOMPILERG(stream)  != NULL) {     \
	BCOMPILER_DEBUGFULL(("      STORE: [%s] (size: %d, name: %s) \n", bytes, n, #bytes));           \
	BCOMPILER_DUMPFULL((char*)bytes, n); \
	php_stream_write(BCOMPILERG(stream) , (char *)bytes, n);   \
	}       \
}
	
/* LOAD_BYTES: memcpy wrapper, reads from src buffer */
#define LOAD_BYTES_X(bytes, n, code) {                                   \
	size_t act;                                                          \
	if (BCOMPILERG(parsing_error)) code;                                 \
	if (BCOMPILERG(buffer_size) < n + 1) {                               \
		BCOMPILER_DEBUGFULL((" *** realloc buffer from %d to %d bytes\n", BCOMPILERG(buffer_size), n + 1)); \
		BCOMPILERG(buffer_size) = n + 1;                                 \
		BCOMPILERG(buffer) = (char*) erealloc(BCOMPILERG(buffer),  n + 1); \
	}                                                                    \
	act = php_stream_read(BCOMPILERG(stream), BCOMPILERG(buffer), n);    \
	if (act != n) {                                                      \
		if (!BCOMPILERG(parsing_error)) php_error(E_WARNING, "bcompiler: Bad bytecode file format at %08x", (unsigned)php_stream_tell(BCOMPILERG(stream))); \
		BCOMPILERG(parsing_error) = 1;                                   \
		code;                                                            \
	}                                                                    \
	memcpy((char*)bytes, BCOMPILERG(buffer), n); 	                     \
	BCOMPILERG(buffer)[n] = '\0';                                        \
	BCOMPILER_DEBUGFULL(("      LOAD: [%s] (size: %d, name: %s) \n", (char*)BCOMPILERG(buffer), n, #bytes));           \
	BCOMPILER_DUMPFULL((char*)bytes, n); \
}
#define LOAD_BYTES(bytes, n)   LOAD_BYTES_X(bytes, n, return)
#define LOAD_BYTES_V(bytes, n, v) LOAD_BYTES_X(bytes, n, return v)

/* Function prototypes and globals */

/* php_bcompiler.c */
extern ZEND_API zend_op_array *(*zend_compile_file)(zend_file_handle *file_handle, int type TSRMLS_DC);
extern zend_op_array *(*bcompiler_saved_zend_compile_file)(zend_file_handle *file_handle, int type TSRMLS_DC);

/* bcompiler.c */
inline void bcompiler_set_stdsize(int ver, const size_t **stdsize);
int bcompiler_can_read(int ver);
char *bcompiler_bc_version(int ver);
php_stream *bz2_aware_stream_open(char *file_name, int use_bz, char **opened_path TSRMLS_DC);
zend_op_array *bcompiler_compile_file(zend_file_handle *file_handle, int type TSRMLS_DC);
zend_op_array *bcompiler_read(TSRMLS_D);
zend_op_array *dummy_op_array(TSRMLS_D);
char *bcompiler_handle_filename(char *filename TSRMLS_DC);
void apc_serialize_string(char* string TSRMLS_DC);
void apc_serialize_zstring(char* string, int len TSRMLS_DC);
int apc_create_string(char** string TSRMLS_DC);
int apc_create_string_u(char** string, int unicode TSRMLS_DC);
void serialize_magic(int ver TSRMLS_DC);
int deserialize_magic(TSRMLS_D);

/* bcompiler_zend.c */
void apc_serialize_zend_class_entry(zend_class_entry* zce , char* force_parent_name, int force_parent_len, char* force_key, int force_key_len TSRMLS_DC);
void apc_serialize_zend_function(zend_function* zf TSRMLS_DC);
void apc_serialize_zend_constant(zend_constant* zc TSRMLS_DC);
void apc_serialize_zend_op_array(zend_op_array* zoa TSRMLS_DC);
void apc_create_zend_class_entry(zend_class_entry** zce, char** key, int* key_len TSRMLS_DC);
void apc_create_zend_function(zend_function** zf TSRMLS_DC);
void apc_create_zend_constant(zend_constant** zc TSRMLS_DC);
void apc_create_zend_op_array(zend_op_array** zoa TSRMLS_DC);
void apc_free_zend_function(zend_function** zf TSRMLS_DC);

#ifdef BCOMPILER_DEBUG_ON
/* bcompiler_debug.c */
inline const char *bcompiler_opcode_name(int op);
void bcompiler_debug(const char *s, ...);
void bcompiler_debugfull(const char *s, ...);
void bcompiler_dump(const char *s, size_t n);
#endif

#endif	/* PHP_BCOMPILER_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
