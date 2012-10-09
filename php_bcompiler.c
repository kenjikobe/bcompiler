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

/* $Id: php_bcompiler.c 305930 2010-12-03 00:49:25Z val $ */

#include "php_bcompiler.h"

PHP_MINIT_FUNCTION(bcompiler);
PHP_MSHUTDOWN_FUNCTION(bcompiler);
PHP_RINIT_FUNCTION(bcompiler);
PHP_RSHUTDOWN_FUNCTION(bcompiler);
PHP_MINFO_FUNCTION(bcompiler);

/* {{{ PHP_FUNCTION declarations */
PHP_FUNCTION(bcompiler_load);             /* load a file  */
PHP_FUNCTION(bcompiler_load_exe);         /* loads an exe style file */
PHP_FUNCTION(bcompiler_parse_class);      /* callback opcodes for a class */
PHP_FUNCTION(bcompiler_read);             /* Read a file handle */
PHP_FUNCTION(bcompiler_set_filename_handler);      /* Set a filename handler */
PHP_FUNCTION(bcompiler_write_class);      /* Write a class */
PHP_FUNCTION(bcompiler_write_constant);   /* Write a constant */
PHP_FUNCTION(bcompiler_write_exe_footer); /* Write the end zero byte */
PHP_FUNCTION(bcompiler_write_file);       /* Write file opcodes */
PHP_FUNCTION(bcompiler_write_footer);     /* Write the tail for a exe type file*/
PHP_FUNCTION(bcompiler_write_function);   /* Write a function */
PHP_FUNCTION(bcompiler_write_functions_from_file); /* Write a bunch of functions */
PHP_FUNCTION(bcompiler_write_header);     /* Write the header */
PHP_FUNCTION(bcompiler_write_included_filename);   /* Add an entry to the included filename list*/
/* }}} */

/* Globals */
ZEND_DECLARE_MODULE_GLOBALS(bcompiler)

zend_op_array *(*bcompiler_saved_zend_compile_file)(zend_file_handle *file_handle, int type TSRMLS_DC);
static void* (*apc_hook)(void *) = NULL;

/* {{{ bcompiler_functions[]
 *
 * All the functions in bcompiler
 */
zend_function_entry bcompiler_functions[] = {
	PHP_FE(bcompiler_load,                      NULL)
	PHP_FE(bcompiler_load_exe,                  NULL)
	PHP_FE(bcompiler_parse_class,               NULL)
	PHP_FE(bcompiler_read,                      NULL)
	PHP_FE(bcompiler_set_filename_handler,      NULL)
	PHP_FE(bcompiler_write_class,               NULL)
	PHP_FE(bcompiler_write_constant,            NULL)
	PHP_FE(bcompiler_write_exe_footer,          NULL)
	PHP_FE(bcompiler_write_file,                NULL)
	PHP_FE(bcompiler_write_footer,              NULL)
	PHP_FE(bcompiler_write_function,            NULL)
	PHP_FE(bcompiler_write_functions_from_file, NULL)
	PHP_FE(bcompiler_write_header,              NULL)
	PHP_FE(bcompiler_write_included_filename,   NULL)
	{NULL, NULL, NULL}
};
/* }}} */

/* {{{ bcompiler_module_entry
 */
zend_module_entry bcompiler_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"bcompiler",
	bcompiler_functions,
	PHP_MINIT(bcompiler),
	PHP_MSHUTDOWN(bcompiler),
	PHP_RINIT(bcompiler),
	PHP_RSHUTDOWN(bcompiler),
	PHP_MINFO(bcompiler),
#if ZEND_MODULE_API_NO >= 20010901
	BCOMPILER_VERSION  BCOMPILER_STREAMS, /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_BCOMPILER
ZEND_GET_MODULE(bcompiler)
#endif

/* ran out of other ideas on how to make these work */

ZEND_DLEXPORT int bcompiler_zend_startup(zend_extension *extension)
{
	return zend_startup_module(&bcompiler_module_entry);
}

ZEND_DLEXPORT void bcompiler_zend_shutdown(zend_extension *extension)
{

}

#ifndef ZEND_EXT_API
#define ZEND_EXT_API      ZEND_DLEXPORT
#endif
ZEND_EXTENSION();

ZEND_DLEXPORT zend_extension zend_extension_entry = {
	"bcompiler",
	BCOMPILER_VERSION,
	"Alan Knowles and val khokhlov",
	"http://www.php.net",
	"Copyright (c) 1997-2010 The PHP Group",
	bcompiler_zend_startup,
	bcompiler_zend_shutdown,
	NULL, /* activate_func_t */
	NULL, /* deactivate_func_t */
	NULL, /* message_handler_func_t */
	NULL, /* op_array_handler_func_t */
	NULL, /* statement_handler_func_t */
	NULL, /* fcall_begin_handler_func_t */
	NULL, /* fcall_end_handler_func_t */
	NULL, /* op_array_ctor_func_t */
	NULL, /* op_array_dtor_func_t */
#ifdef COMPAT_ZEND_EXTENSION_PROPERTIES
	NULL, /* api_no_check */
	COMPAT_ZEND_EXTENSION_PROPERTIES
#else
	STANDARD_ZEND_EXTENSION_PROPERTIES
#endif
};

#ifdef ZEND_ENGINE_2
#define OnUpdateInt OnUpdateLong
#endif
/* {{{ PHP_INI */
PHP_INI_BEGIN()
STD_PHP_INI_BOOLEAN("bcompiler.enabled", "1", PHP_INI_ALL, OnUpdateBool, enabled, zend_bcompiler_globals, bcompiler_globals)
STD_PHP_INI_BOOLEAN("bcompiler.detect_filedir", "1", PHP_INI_ALL, OnUpdateBool, detect_filedir, zend_bcompiler_globals, bcompiler_globals)
STD_PHP_INI_ENTRY("bcompiler.debug", "0", PHP_INI_ALL, OnUpdateInt, debug_lvl, zend_bcompiler_globals, bcompiler_globals)
STD_PHP_INI_ENTRY("bcompiler.debugfile", NULL, PHP_INI_ALL, OnUpdateString, debug_file, zend_bcompiler_globals, bcompiler_globals)
PHP_INI_END()
/* }}} */

#ifdef ZEND_ENGINE_2
static void zend_destroy_property_info(zend_property_info *property_info)
{
    efree(ZS2S(property_info->name));
}
#endif

/* {{{ php_bcompiler_init_globals
 */
static void php_bcompiler_init_globals(zend_bcompiler_globals *bcompiler_globals)
{
#ifdef ZEND_ENGINE_2
	HashTable ht;
	zend_hash_init(&ht, 0, NULL, (dtor_func_t)&zend_destroy_property_info, 0);
	zend_hash_destroy(&ht);
#endif

	bcompiler_globals->stream = NULL;
	bcompiler_globals->callback = NULL;
	bcompiler_globals->callback_value = NULL;
	bcompiler_globals->callback_key = "NONE";
	bcompiler_globals->current_version = BCOMPILER_CUR_VER;
	bcompiler_globals->is_unicode = 0;
	bcompiler_globals->current_write   = BCOMPILER_CUR_VER;
	bcompiler_globals->current_include = 0;
	bcompiler_globals->parsing_error   = 0;
	bcompiler_set_stdsize(BCOMPILER_CUR_VER, &(bcompiler_globals->bcompiler_stdsize));
	bcompiler_globals->cur_zc = NULL;
	bcompiler_globals->current_filename = NULL;
	bcompiler_globals->filename_handler = NULL;
	bcompiler_globals->filename_handler_name = NULL;
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(bcompiler)
{
	zend_module_entry *apc_lookup;
	zend_constant *apc_magic;

	ZEND_INIT_MODULE_GLOBALS(bcompiler, php_bcompiler_init_globals, NULL);
    REGISTER_INI_ENTRIES();

	if (BCOMPILERG(enabled)) {
		if (zend_hash_find(&module_registry, "apc", sizeof("apc"), (void**)&apc_lookup) != FAILURE &&
			zend_hash_find(EG(zend_constants), "\000apc_magic", 11, (void**)&apc_magic) != FAILURE) {
			apc_hook = (void* (*)(void*))apc_magic->value.value.lval;
			bcompiler_saved_zend_compile_file = apc_hook(NULL);
			apc_hook(bcompiler_compile_file);
		} else {
			bcompiler_saved_zend_compile_file = zend_compile_file;
			zend_compile_file = bcompiler_compile_file;
		}
	}
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(bcompiler)
{
	if (BCOMPILERG(enabled)) {
		if (apc_hook)
			apc_hook(bcompiler_saved_zend_compile_file);
		else
			zend_compile_file = bcompiler_saved_zend_compile_file;
#if BCOMPILER_DEBUG_ON || BCOMPILER_DEBUGFULL_ON
		/* close debug log */
		bcompiler_debug(NULL);
#endif
	}
    UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}
/* }}} */


/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(bcompiler)
{
	if (BCOMPILERG(enabled)) {
		/* looks like init_globals is supposed to do this, but it's not? */
		BCOMPILERG(buffer_size) = 1024;
		BCOMPILERG(buffer) = (char*) emalloc( BCOMPILERG(buffer_size) );
		BCOMPILERG(current_filename) = NULL;
		BCOMPILERG(filename_handler) = NULL;
		BCOMPILERG(filename_handler_name) = NULL;
	}
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(bcompiler)
{
	if (BCOMPILERG(enabled)) {
		efree(BCOMPILERG(buffer));
		if (BCOMPILERG(current_filename)) efree(BCOMPILERG(current_filename));
#if PHP_MAJOR_VERSION >= 6
		if (BCOMPILERG(filename_handler_name)) zval_ptr_dtor(&BCOMPILERG(filename_handler_name));
#else
		if (BCOMPILERG(filename_handler_name)) efree(BCOMPILERG(filename_handler_name));
#endif
		if (BCOMPILERG(filename_handler)) zval_ptr_dtor(&BCOMPILERG(filename_handler));
	}
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(bcompiler)
{
	char *tmp;

	php_info_print_table_start();
	php_info_print_table_header(2, "bcompiler support", BCOMPILERG(enabled) ? "enabled" : "disabled");
	php_info_print_table_row(2, "bcompiler version", BCOMPILER_VERSION BCOMPILER_STREAMS);
#ifdef BCOMPILER_DEBUG_ON
	{
	char buf[8];
	snprintf(buf, sizeof(buf)-1, "%d", BCOMPILERG(debug_lvl));
	php_info_print_table_row(2, "bcompiler debug level", buf);
	}
#endif
	tmp = bcompiler_bc_version(BCOMPILER_CUR_VER);
	php_info_print_table_row(2, "current bytecode version", tmp);
	tmp = bcompiler_bc_version(0);
	php_info_print_table_row(2, "can parse bytecode version", tmp);
	efree(tmp);
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();

}
/* }}} */

/* === FUNCTIONS =========================================================== */

/* --- class --------------------------------------------------------------- */

/* {{{ proto boolean bcompiler_write_class(stream out | filehandle, string class_name [, string extends] )
   Writes compiled data to stream*/
PHP_FUNCTION(bcompiler_write_class)
{
	char *class_name = NULL;
	char *extends_name = NULL;
	int class_len,extends_len = 0;
#ifdef ZEND_ENGINE_2
	zend_class_entry **pce = NULL;
#endif
	zend_class_entry *ce = NULL;
	zend_class_entry *cee = NULL;
	zval *rsrc;
	php_stream *stream;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs|s",
			&rsrc, &class_name, &class_len,&extends_name, &extends_len)
					== FAILURE) {
		return;
	}
	php_stream_from_zval(stream, &rsrc);

	/* DANGER! This modifies the class_name string passed by the script */
#ifdef ZEND_ENGINE_2
	if (FAILURE == zend_lookup_class(class_name, class_len, &pce TSRMLS_CC))
#else
	zend_str_tolower(class_name, class_len);
	if (SUCCESS != zend_hash_find(EG(class_table), class_name, class_len+1, (void **)&ce))
#endif
	{
		php_error(E_WARNING, "Could not find class %s", class_name);
		RETURN_NULL();
	}
	BCOMPILER_DEBUG(("EXTENDS LEN: %d",extends_len));
	if (extends_len > 0) {
		zend_str_tolower(extends_name, extends_len);
		if (SUCCESS != zend_hash_find(EG(class_table), extends_name, extends_len+1, (void **)&cee)) {
			php_error(E_WARNING, "Could not find extended class");
			RETURN_NULL();
		}
	}

	BCOMPILERG(stream)  = stream;
	BCOMPILERG(callback) = NULL;

	SERIALIZE_SCALAR(BCOMPILER_CLASS_ENTRY, char)
#ifdef ZEND_ENGINE_2
	ce = *pce;
#endif
	apc_serialize_zend_class_entry(ce, extends_name, extends_len, NULL, 0 TSRMLS_CC);

	RETURN_TRUE;
}
/* }}} */

/* --- included_filename --------------------------------------------------- */

/* {{{ proto boolean bcompiler_write_included_filename(stream out | filehandle, string filename)
   Writes compiled data to stream*/
PHP_FUNCTION(bcompiler_write_included_filename)
{
	char *file_name = NULL;
	int file_len;
	zval *rsrc;
	php_stream *stream;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &rsrc,&file_name, &file_len) == FAILURE) {
		return;
	}
	php_stream_from_zval(stream, &rsrc);

	BCOMPILERG(stream)  = stream;
	BCOMPILERG(callback) = NULL;

	SERIALIZE_SCALAR(BCOMPILER_INCTABLE_ENTRY, char);
	apc_serialize_string(file_name TSRMLS_CC);

	RETURN_TRUE;
}
/* }}} */

/* --- function ------------------------------------------------------------ */

/* {{{ proto boolean bcompiler_write_function(stream out | filehandle, string function_name)
   Writes compiled data to stream*/
PHP_FUNCTION(bcompiler_write_function)
{
	char *function_name = NULL;
	int function_len;
	zend_function *fe = NULL;
	zval *rsrc;
	php_stream *stream;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &rsrc, &function_name, &function_len) == FAILURE) {
		return;
	}
	php_stream_from_zval(stream, &rsrc);

	/* DANGER! modifies function_name */
	zend_str_tolower(function_name, function_len);
	zend_hash_find(CG(function_table), function_name, function_len+1, (void **)&fe);
	if (!fe) {
		php_error(E_WARNING, "Could not find function");
		RETURN_NULL();
	}
	BCOMPILERG(stream)  = stream;
	BCOMPILERG(callback) = NULL;

	SERIALIZE_SCALAR(BCOMPILER_FUNCTION_ENTRY, char)
	apc_serialize_zend_function(fe TSRMLS_CC);

	RETURN_TRUE;
}
/* }}} */

/* --- constant ------------------------------------------------------------ */

/* {{{ proto boolean bcompiler_write_constant(stream out | filehandle, string constant)
   Writes compiled data to stream*/
PHP_FUNCTION(bcompiler_write_constant)
{
	char *constant_name = NULL;
	int constant_len;
	zend_constant *zc = NULL;
	zval *rsrc;
	php_stream *stream;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &rsrc, &constant_name, &constant_len) == FAILURE) {
		return;
	}
	php_stream_from_zval(stream, &rsrc);

	zend_hash_find(EG(zend_constants), constant_name, constant_len + 1, (void **)&zc);
	if (!zc) {
		php_error(E_WARNING, "Could not find constant");
		RETURN_NULL();
	}
	BCOMPILERG(stream)  = stream;
	BCOMPILERG(callback) = NULL;

	SERIALIZE_SCALAR(BCOMPILER_CONSTANT, char)
	apc_serialize_zend_constant(zc TSRMLS_CC);

	RETURN_TRUE;
}
/* }}} */

/* --- functions from file ------------------------------------------------- */

/* write functions defined in file `real_path' to stream */
static void _bcompiler_write_functions_from_file(char *real_path TSRMLS_DC)
{
	zend_function *zf = NULL;
	HashPosition   pos;

	/* Cycle through the functions HashTable looking for user defined functions */
	zend_hash_internal_pointer_reset_ex(EG(function_table), &pos);
	while (zend_hash_get_current_data_ex(EG(function_table), (void **)&zf, &pos) == SUCCESS)
	{
		if (zf->type == ZEND_USER_FUNCTION) {
			if (strcmp(zf->op_array.filename, real_path) == 0) {
				if (BCOMPILERG(current_write) < 0x0010) {
					SERIALIZE_SCALAR(BCOMPILER_FUNCTION_ENTRY, char)
				} else {
					SERIALIZE_SCALAR(BCOMPILER_FUNCTION_ENTRY_EX, char)
					SERIALIZE_SCALAR(pos->nKeyLength, int);
#if PHP_MAJOR_VERSION >= 6
					apc_serialize_zstring(pos->key.arKey.s, pos->key.type == IS_UNICODE ? UBYTES(pos->nKeyLength) : pos->nKeyLength TSRMLS_CC);
#else
					apc_serialize_zstring(pos->arKey, pos->nKeyLength TSRMLS_CC);
#endif
				}
				apc_serialize_zend_function(zf TSRMLS_CC);
			}
		}
		zend_hash_move_forward_ex(EG(function_table), &pos);
	}
}

/* {{{ proto boolean bcompiler_write_functions_from_file(stream out | filehandle, string filename)
   Writes compiled data from procedural code to stream */
PHP_FUNCTION(bcompiler_write_functions_from_file)
{
	zval *rsrc;
	char *filename = NULL;
	int filename_length;
	char *real_path = NULL;
	php_stream *stream;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &rsrc, &filename, &filename_length) == FAILURE) {
		return;
	}
	php_stream_from_zval(stream, &rsrc);

	BCOMPILERG(stream)  = stream;
	BCOMPILERG(callback) = NULL;

	real_path = expand_filepath(filename, NULL TSRMLS_CC);
	if (!real_path) {
		RETURN_FALSE;
	}

	_bcompiler_write_functions_from_file(real_path TSRMLS_CC);

	efree(real_path);
	RETURN_TRUE;
}
/* }}} */

/* --- file ---------------------------------------------------------------- */

/* {{{ proto boolean bcompiler_write_file(stream out | filehandle, string filename)
   Writes compiled file data to stream */
PHP_FUNCTION(bcompiler_write_file)
{
	zend_file_handle file_handle = {0};
	zval *rsrc;
	char *filename = NULL;
	int filename_length;
	char *real_path = NULL;
	php_stream *stream;
	zend_op_array *op_array = NULL;
#ifndef ZEND_ENGINE_2
	int i = 0;
	int n_old_class;
#endif
	HashPosition pos;
	zend_class_entry *zc;
	zend_function *zf = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &rsrc, &filename, &filename_length) == FAILURE) {
		return;
	}
	php_stream_from_zval(stream, &rsrc);

	BCOMPILERG(stream)  = stream;
	BCOMPILERG(callback) = NULL;

	real_path = expand_filepath(filename, NULL TSRMLS_CC);
	if (!real_path) {
		RETURN_FALSE;
	}

	file_handle.filename = real_path;
	file_handle.free_filename = 0;
	file_handle.type = ZEND_HANDLE_FILENAME;
	file_handle.opened_path = NULL;
#ifndef ZEND_ENGINE_2
	n_old_class = zend_hash_num_elements(CG(class_table));
#endif
	CG(zend_lineno) = 0; /* Damjan Cvetko: to prevent -1 lineno offset */
	/* compile (as include) */
	op_array = bcompiler_saved_zend_compile_file(&file_handle, ZEND_INCLUDE/*type*/ TSRMLS_CC);
	zend_destroy_file_handle(&file_handle TSRMLS_CC);
	/* check for errors */
	if (!op_array) {
		efree(real_path);
		RETURN_FALSE;
	}
	/* following line will execute just compiled script */
	/*zend_execute(op_array TSRMLS_CC);*/

	/* write classes */
	zend_hash_internal_pointer_reset_ex(CG(class_table), &pos);
	while (zend_hash_get_current_data_ex(CG(class_table), (void **)&zc, &pos) == SUCCESS)
	{
#ifdef ZEND_ENGINE_2
		zc = *((zend_class_entry**)zc);
#else
		i++;
#endif
		if (zc->type == ZEND_USER_CLASS) {
#ifndef ZEND_ENGINE_2
			if (i > n_old_class)
#else
			if (zc->info.user.filename && strcmp(zc->info.user.filename, real_path) == 0)
#endif
			{
				SERIALIZE_SCALAR(BCOMPILER_CLASS_ENTRY, char)
#if PHP_MAJOR_VERSION >= 6
				apc_serialize_zend_class_entry(zc, NULL, 0, pos->key.arKey.s, pos->key.type == IS_UNICODE ? -pos->nKeyLength : pos->nKeyLength TSRMLS_CC);
#else
				apc_serialize_zend_class_entry(zc, NULL, 0, pos->arKey, pos->nKeyLength TSRMLS_CC);
#endif
			}
		}
		zend_hash_move_forward_ex(CG(class_table), &pos);
	}

	/* write functions */
	_bcompiler_write_functions_from_file(real_path TSRMLS_CC);

	/* write script body opcodes */
	SERIALIZE_SCALAR(BCOMPILER_OP_ARRAY, char);
	apc_serialize_zend_op_array(op_array TSRMLS_CC);

	/* free opcode array */
#ifdef ZEND_ENGINE_2
	destroy_op_array(op_array TSRMLS_CC);
#else
	destroy_op_array(op_array);
#endif
	efree(op_array);

	/* clean-up created functions */
	zend_hash_internal_pointer_reset_ex(CG(function_table), &pos);
	while (zend_hash_get_current_data_ex(CG(function_table), (void **)&zf, &pos) == SUCCESS)
	{
		if (zf->type == ZEND_USER_FUNCTION) {
			if (strcmp(zf->op_array.filename, real_path) == 0) {
#if PHP_MAJOR_VERSION >= 6
				zend_u_hash_del(CG(function_table), pos->key.type, ZSTR(pos->key.arKey.s), pos->nKeyLength);
#else
				zend_hash_del(CG(function_table), pos->arKey, pos->nKeyLength);
#endif
			}
		}
		zend_hash_move_forward_ex(CG(function_table), &pos);
	}

	/* clean-up created classes */
#ifndef ZEND_ENGINE_2
	i = 0;
#endif
	zend_hash_internal_pointer_reset_ex(CG(class_table), &pos);
	while (zend_hash_get_current_data_ex(CG(class_table), (void **)&zc, &pos) == SUCCESS)
	{
#ifdef ZEND_ENGINE_2
		zc = *((zend_class_entry**)zc);
#else
		i++;
#endif
		if (zc->type == ZEND_USER_CLASS) {
#ifndef ZEND_ENGINE_2
			if (i > n_old_class)
#else
			if (zc->info.user.filename && strcmp(zc->info.user.filename, real_path) == 0)
#endif
			{
#if PHP_MAJOR_VERSION >= 6
				zend_u_hash_del(CG(class_table), pos->key.type, ZSTR(pos->key.arKey.s), pos->nKeyLength);
#else
				zend_hash_del(CG(class_table), pos->arKey, pos->nKeyLength);
#endif
			}
		}
		zend_hash_move_forward_ex(CG(class_table), &pos);
	}

	efree(real_path);
	RETURN_TRUE;
}
/* }}} */

/* --- parse_class --------------------------------------------------------- */

/* {{{ proto boolean bcompiler_parse_class(callback, string class_name)
   Writes compiled data to g*/
PHP_FUNCTION(bcompiler_parse_class)
{
	char *class_name = NULL;
	int class_len;
	zend_class_entry *ce = NULL;
	zval *callback = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zs",   &callback, &class_name, &class_len) == FAILURE) {
		return;
	}

	zend_str_tolower(class_name, class_len);
	zend_hash_find(EG(class_table), class_name, class_len+1, (void **)&ce);
	if (!ce) {
		RETURN_NULL();
	}

	BCOMPILERG(callback) = callback;
	BCOMPILERG(stream) = NULL;

	apc_serialize_zend_class_entry(ce, NULL, 0, NULL, 0 TSRMLS_CC);

	RETURN_TRUE;
}
/* }}} */

/* --- header -------------------------------------------------------------- */

/* {{{ proto boolean bcompiler_write_header(stream out[, string write_ver])
   Writes the start flag*/
PHP_FUNCTION(bcompiler_write_header)
{
	zval *rsrc;
	char *s = NULL;
	int slen = 0, ver = BCOMPILER_CUR_VER;
	php_stream *stream;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r|s", &rsrc, &s, &slen) == FAILURE) {
		return;
	}

	php_stream_from_zval(stream, &rsrc);
	/* check desired bytecode version */
	if (s) {
		unsigned int rc, hi, lo;
		rc = sscanf(s, "%u.%u", &hi, &lo);
		if (rc == 2) {
			ver = ((hi & 0xff) << 8) + (lo & 0xff);
			if (!bcompiler_can_read(ver)) {
				zend_error(E_WARNING, "unsupported version, using defaults");
				ver = BCOMPILER_CUR_VER;
			}
		}
	}
	BCOMPILERG(current_write) = ver;
	bcompiler_set_stdsize(ver, &BCOMPILERG(bcompiler_stdsize));

	BCOMPILERG(stream)  = stream;
	serialize_magic(ver TSRMLS_CC);

	RETURN_TRUE;
}
/* }}} */

/* --- footer -------------------------------------------------------------- */

/* {{{ proto boolean bcompiler_write_footer(stream out)
   Writes the end zero*/
PHP_FUNCTION(bcompiler_write_footer)
{
	zval *rsrc;
	php_stream *stream;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &rsrc) == FAILURE) {
		return;
	}

	php_stream_from_zval(stream, &rsrc);
	BCOMPILERG(stream)  = stream;

	SERIALIZE_SCALAR(0, char);
	RETURN_TRUE;
}
/* }}} */

/* --- exe_footer ---------------------------------------------------------- */

/* {{{ proto boolean bcompiler_write_exe_footer(stream out, int start_pos)
   Writes compiled data to stream*/
PHP_FUNCTION(bcompiler_write_exe_footer)
{
	zval *rsrc;
	int start;
	php_stream *stream;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rl", &rsrc, &start) == FAILURE) {
		return;
	}

	php_stream_from_zval(stream, &rsrc);
	BCOMPILERG(stream)  = stream;

	SERIALIZE_SCALAR(start, int);
	serialize_magic(0 TSRMLS_CC);

	RETURN_TRUE;
}
/* }}} */

/* --- read ---------------------------------------------------------------- */

/* {{{ proto boolean bcompiler_read(stream in)
   Reads compiled data from stream*/
PHP_FUNCTION(bcompiler_read)
{
	zval *rsrc;
	int test =0;
	php_stream *stream;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &rsrc) == FAILURE) {
		return;
	}

	php_stream_from_zval(stream, &rsrc);
	BCOMPILERG(stream) = stream;

	test = deserialize_magic(TSRMLS_C);
	if (test != 0) {
		php_error(E_WARNING, "Could not find Magic header in stream");
		return;
	}

	BCOMPILERG(current_include) = 0;
	bcompiler_read(TSRMLS_C);

	RETURN_TRUE;
}
/* }}} */

/* --- load ---------------------------------------------------------------- */

/* {{{ proto boolean bcompiler_load(string bziped_file)
   Reads compiled data bziped file*/
PHP_FUNCTION(bcompiler_load)
{
	int test =0;
	char *filename;
	int filename_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s",  &filename,&filename_len) == FAILURE) {
		return;
	}

	BCOMPILERG(stream) = bz2_aware_stream_open(filename, 1, NULL TSRMLS_CC);

	if (!BCOMPILERG(stream)) {
		php_error(E_WARNING, "Could not open stream");
	}

	test = deserialize_magic(TSRMLS_C);

	if (test != 0) {
		php_error(E_WARNING, "Could not find Magic header in stream");
		return;
	}

	if (BCOMPILERG(current_filename)) efree(BCOMPILERG(current_filename));
	BCOMPILERG(current_filename) = estrdup(filename);
	BCOMPILERG(current_include) = 0;
	bcompiler_read(TSRMLS_C);
	php_stream_close( BCOMPILERG(stream) );

	RETURN_TRUE;
}
/* }}} */

/* --- load_exe ------------------------------------------------------------ */

/* {{{ proto boolean bcompiler_load_exe(string bziped_file)
   Reads compiled data bziped file*/
PHP_FUNCTION(bcompiler_load_exe)
{

	php_stream *stream = NULL;
	int test =0;
	int seekreturn = 0;
	char *filename;
	int filename_len;
	long pos;       /* offset to use */

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s",  &filename,&filename_len) == FAILURE) {
		return;
	}

	stream = bz2_aware_stream_open(filename, 0, NULL TSRMLS_CC);

	if (!stream) {
		BCOMPILER_DEBUG(("could not open stream: %s\n", filename));
		php_error(E_WARNING, "Failed to open %s",filename);
		return;
	}

	pos = (long) ((strlen(BCOMPILER_MAGIC_HEADER)+4) * -1); /* +4 cause we need to read the string size */
	BCOMPILER_DEBUG(("move to start: %d\n", pos));
	php_stream_seek(stream, pos , SEEK_END);
	BCOMPILER_DEBUG(("done seek - try deserialize\n"));

	BCOMPILERG(stream) = stream;
	test = deserialize_magic(TSRMLS_C);

#if DISABLED_EXE
	if (test != 0) {
		BCOMPILER_DEBUG(("No magic header found\n"));
		php_error(E_WARNING, "Could not find Magic header in stream");
		php_stream_close(stream);
		return;
	}
#endif

	/* now read the length */
	pos = pos - 4;
	BCOMPILER_DEBUG(("move to end: %d\n", pos));
	php_stream_seek(stream, pos , SEEK_END);
	DESERIALIZE_SCALAR(&pos, int);
	/* seek the beginning of the bytecodes */
	BCOMPILER_DEBUG(("move to start: %d\n", pos));
	seekreturn = php_stream_seek(stream, pos , SEEK_SET);

	if (seekreturn != 0) {
		php_error(E_WARNING, "Failed to seek to stored position");
		php_stream_close(stream);
		return;
	}

	BCOMPILERG(stream)  = stream;
	test = deserialize_magic(TSRMLS_C);

	if (test != 0) {
		php_error(E_ERROR, "Could not find Magic header in stream");
		php_stream_close(stream);
		return;
	}

	if (BCOMPILERG(current_filename)) efree(BCOMPILERG(current_filename));
	BCOMPILERG(current_filename) = estrdup(filename);
	BCOMPILERG(current_include) = 0;
	bcompiler_read( TSRMLS_C);
	php_stream_close(stream);

	RETURN_TRUE;
}
/* }}} */

/* --- set_filename_handler ------------------------------------------------ */

/* {{{ proto boolean bcompiler_set_filename_handler(callback handler)
   Reads compiled data bziped file*/
PHP_FUNCTION(bcompiler_set_filename_handler)
{
	zval *func = NULL;
#if PHP_MAJOR_VERSION >= 6
	zval *name = NULL;
#else
	char *name = NULL;
#endif

	if (ZEND_NUM_ARGS() == 0) {
#if PHP_MAJOR_VERSION >= 6
		if (BCOMPILERG(filename_handler_name)) zval_ptr_dtor(&BCOMPILERG(filename_handler_name));
#else
		if (BCOMPILERG(filename_handler_name)) efree(BCOMPILERG(filename_handler_name));
#endif
		if (BCOMPILERG(filename_handler)) zval_ptr_dtor(&BCOMPILERG(filename_handler));
		BCOMPILERG(filename_handler_name) = NULL;
		BCOMPILERG(filename_handler) = NULL;
		RETURN_TRUE;
	}
	else if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &func) != FAILURE) {
#if PHP_MAJOR_VERSION < 6
		if (Z_TYPE_P(func) != IS_STRING && Z_TYPE_P(func) != IS_ARRAY) {
			SEPARATE_ZVAL(&func);
			convert_to_string_ex(&func);
		}
#endif
	}
	else {
		WRONG_PARAM_COUNT;
	}

#if PHP_MAJOR_VERSION >= 6
	MAKE_STD_ZVAL(name);
	if (!zend_is_callable(func, 0, name TSRMLS_CC)) {
		if (name && Z_UNILEN(*name) == 0) {
			func = NULL;
		} else {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Argument is expected to be a valid callback, '%R' was given", Z_TYPE_P(name), Z_UNIVAL_P(name));
			if (name) zval_ptr_dtor(&name);
			RETURN_FALSE;
		}
	}
#else
	if (!zend_is_callable(func, 0, &name TSRMLS_CC)) {
		if (name && *name == 0) {
			func = NULL;
		} else {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Argument is expected to be a valid callback, '%s' was given", name);
			if (name) efree(name);
			RETURN_FALSE;
		}
	}
#endif

#if PHP_MAJOR_VERSION >= 6
	if (BCOMPILERG(filename_handler_name)) zval_ptr_dtor(&BCOMPILERG(filename_handler_name));
#else
	if (BCOMPILERG(filename_handler_name)) efree(BCOMPILERG(filename_handler_name));
#endif
	if (BCOMPILERG(filename_handler)) zval_ptr_dtor(&BCOMPILERG(filename_handler));
	BCOMPILERG(filename_handler_name) = name;
	BCOMPILERG(filename_handler) = func;
	if (func) zval_add_ref(&func);
	RETURN_TRUE;
}
/* }}} */



/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
