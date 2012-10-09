/*
   +----------------------------------------------------------------------+
   | PHP bcompiler                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 2000-2001 Community Connect, Inc. (apc_* functions)    |
   | Copyright (c) 1997-2010 The PHP Group (remaining code)               |
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

/* $Id: bcompiler.c 311913 2011-06-08 02:51:37Z alan_k $ */

#include "php_bcompiler.h"

/* --- standard size ------------------------------------------------------- */

static const size_t _stdsize_03[] = { BCOMPILER_STDSIZE_03 };
static const size_t _stdsize_05[] = { BCOMPILER_STDSIZE_05 };

inline void bcompiler_set_stdsize(int ver, const size_t **stdsize) {
	*stdsize = (ver < 0x0005) ? _stdsize_03 : _stdsize_05;
}

/* --- compatible bytecode versions ---------------------------------------- */

static int _can_read[] = { BCOMPILER_CAN_READ }; /* can read array */

int bcompiler_can_read(int ver) {
	int i, n = sizeof(_can_read) / sizeof(_can_read[0]);
	for (i = 0; i < n; i++)	if (ver == _can_read[i]) return 1;
	return 0;
}

static char *_bcompiler_vers(int v) {
	static char tmp[8];

	snprintf(tmp, 8, "%u.%u", (v >> 8) & 0xff, v & 0xff);
	tmp[7] = 0;
	return tmp;
}

char *bcompiler_bc_version(int ver) {
	char *tmp;
	int i, n = sizeof(_can_read) / sizeof(_can_read[0]);

	if (ver != 0) return _bcompiler_vers(ver);

	tmp = emalloc(10 * n); tmp[0] = 0;
	for (i = 0; i < n; i++) {
		strcat(tmp, _bcompiler_vers(_can_read[i]));
		if (i < n - 1) strcat(tmp, ", ");
	}

	return tmp;
}

/* --- I/O functions ------------------------------------------------------- */

static int is_valid_file_name(char *filename)
{
	int len = strlen(filename);

	if (len == 0) { /* empty filenames are invalid */
		return 0;
	}
	if (strncasecmp(filename, "http://", 7) == 0) {
		return 0;
	}
	if (strncmp(filename + len - 1, "-", 1) == 0) {
		return 0;
 	}
	return 1;
}

static inline int has_gzip_stream_support(TSRMLS_D)
{
	return php_stream_locate_url_wrapper("compress.zlib://", NULL, STREAM_LOCATE_WRAPPERS_ONLY TSRMLS_CC) != NULL;
}
static inline int has_bzip2_stream_support(TSRMLS_D)
{
	return php_stream_locate_url_wrapper("compress.bzip2://", NULL, STREAM_LOCATE_WRAPPERS_ONLY TSRMLS_CC) != NULL;
}

php_stream *bz2_aware_stream_open(char *file_name, int use_bz, char **opened_path TSRMLS_DC)
{
	php_stream *stream;
	char *path_to_open;
	unsigned char magic[2];
	static int has_gz = -1, has_bz = -1;
	int old_error_reporting = EG(error_reporting);

	EG(error_reporting) = E_ERROR;
	if (has_gz == -1) has_gz = has_gzip_stream_support(TSRMLS_C);
	if (has_bz == -1) has_bz = has_bzip2_stream_support(TSRMLS_C);
	EG(error_reporting) = old_error_reporting;

	/* try to read magic */
	stream = php_stream_open_wrapper(file_name, "rb", ENFORCE_SAFE_MODE|USE_PATH|IGNORE_URL_WIN|STREAM_OPEN_FOR_INCLUDE, opened_path);
	if (!stream) {
		BCOMPILER_DEBUG(("error opening file '%s'..\n", file_name));
		return stream;
	}
	php_stream_read(stream, (char *) magic, sizeof(magic));
	BCOMPILER_DEBUG(("read magic: %02x %02x..\n", magic[0], magic[1]));

	/* it's a bzip2 compressed file */
	if (magic[0] == 'B' && magic[1] == 'Z') {
		php_stream_close(stream);
		if (!has_bz || !use_bz) {
			BCOMPILERG(parsing_error) = 1;
			BCOMPILER_DEBUG(("no bz2 support, giving up..\n"));
			return NULL; 
		}

		BCOMPILER_DEBUG(("got bz2 support - opening '%s'\n", file_name));
		spprintf(&path_to_open, 0, "compress.bzip2://%s", file_name);
		stream = php_stream_open_wrapper(path_to_open, "rb", ENFORCE_SAFE_MODE|USE_PATH|IGNORE_URL_WIN|STREAM_OPEN_FOR_INCLUDE, opened_path);
		efree(path_to_open);
		BCOMPILER_DEBUG(("returning stream (%p)..\n", stream));
		return stream;
	}
	/* it's a gzip compressed file */
	if (magic[0] == 0x1f && magic[1] == 0x8b) {
		php_stream_close(stream);
		if (!has_gz || !use_bz) {
			BCOMPILERG(parsing_error) = 1;
			BCOMPILER_DEBUG(("no gzip support, giving up..\n"));
			return NULL; 
		}

		BCOMPILER_DEBUG(("got gzip support - opening '%s'\n", file_name));
		spprintf(&path_to_open, 0, "compress.zlib://%s", file_name);
		stream = php_stream_open_wrapper(path_to_open, "rb", ENFORCE_SAFE_MODE|USE_PATH|IGNORE_URL_WIN|STREAM_OPEN_FOR_INCLUDE, opened_path);
		efree(path_to_open);
		BCOMPILER_DEBUG(("returning stream (%p)..\n", stream));
		return stream;
	}
	/* uncompressed file */
	php_stream_rewind(stream);
	return stream;
}

/* --- compile functions --------------------------------------------------- */

/* {{{ 
 */
zend_op_array *dummy_op_array(TSRMLS_D) {
	zval *zstring = NULL;
	char *eval_desc = NULL;
	zend_op_array *result = NULL;
	zend_op_array *original_active_op_array = CG(active_op_array);
	zend_bool original_in_compilation = CG(in_compilation);
	char *compiled_filename = CG(compiled_filename);
/*	TSRMLS_FETCH();*/

	CG(in_compilation) = 1;
	CG(active_op_array) = result;
	CG(compiled_filename) = "bcompiler code";

	MAKE_STD_ZVAL(zstring);
	ZVAL_STRING(zstring, "return true;", 1); 
	eval_desc = zend_make_compiled_string_description("compiled code" TSRMLS_CC);
	result = compile_string(zstring, eval_desc TSRMLS_CC);
	efree(eval_desc);
	zval_dtor(zstring);
	FREE_ZVAL(zstring);

	CG(in_compilation) = original_in_compilation;
	CG(active_op_array) = original_active_op_array;
	CG(compiled_filename) = compiled_filename;

	return result;
}
/* }}} */

/*
	type
	ZEND_EVAL           1<<0
	ZEND_INCLUDE        1<<1
	ZEND_INCLUDE_ONCE   1<<2
	ZEND_REQUIRE        1<<3
	ZEND_REQUIRE_ONCE   1<<4
*/
zend_op_array *bcompiler_compile_file(zend_file_handle *file_handle, int type TSRMLS_DC)
{
	int test = -1;
	php_stream *stream = NULL;
	zend_op_array *op_array = NULL;
	char *filename = NULL;
	 
	if (!BCOMPILERG(enabled)) {
		BCOMPILER_DEBUGFULL(("bcompiler disabled - passing through\n"));
		return bcompiler_saved_zend_compile_file(file_handle, type TSRMLS_CC);
	}

	test = -1; BCOMPILERG(parsing_error) = 0;
#if 0 /* val: it seems type doesn't matter */
	switch(file_handle->type)
	{
	case ZEND_HANDLE_FILENAME: /* 0 */
	case ZEND_HANDLE_FD: /* 1 */
	case ZEND_HANDLE_FP: /* 2 */
#endif
		if(file_handle->opened_path) {
			filename = file_handle->opened_path;
		} else { 
			filename = file_handle->filename;
		}
		if (is_valid_file_name(filename)) {
			stream = bz2_aware_stream_open(filename, 1, &(file_handle->opened_path) TSRMLS_CC);
		}
#if 0 /* val: it seems type doesn't matter */
		break;

	case ZEND_HANDLE_STDIOSTREAM: /* 3 */
		break;

	case ZEND_HANDLE_FSTREAM: /* 4 */
		break;
	}
#endif
	/* stream is ok */
	if (stream) {
		BCOMPILERG(stream) = stream;
		test = deserialize_magic(TSRMLS_C);
	}
	/* compressed stream and no decompression supported */
	else if (BCOMPILERG(parsing_error)) {
		php_error(E_WARNING, "bcompiler: Unable to open or can't decompress stream"); 
		return op_array;
	}
	/* we can't handle this type, so use fallback */
	else {
		BCOMPILER_DEBUGFULL((" *** error opening file, using fallback zend_compile_file()\n"));
		op_array = bcompiler_saved_zend_compile_file(file_handle, type TSRMLS_CC);
		return op_array;
	}
	/* can parse bytecodes */
	if (test == 0) {
		int dummy = 1;
		/* add file_handle to open_file so that zend will free it */
		if ( !(file_handle->type == ZEND_HANDLE_FP && file_handle->handle.fp == stdin) &&
		     !(file_handle->type == ZEND_HANDLE_FD && file_handle->handle.fd == STDIN_FILENO) 
		   ) zend_llist_add_element(&CG(open_files), file_handle);
		/* to make APC happy */
		if (!file_handle->opened_path)
			file_handle->opened_path = estrdup(file_handle->filename);
		zend_hash_add(&EG(included_files), file_handle->opened_path, strlen(file_handle->opened_path)+1, (void *)&dummy, sizeof(int), NULL);
		/* read bytecodes */
		if (BCOMPILERG(current_filename)) efree(BCOMPILERG(current_filename));
		BCOMPILERG(current_filename) = estrdup(file_handle->opened_path); /*expand_filepath(file_handle->filename, NULL TSRMLS_CC);*/
		BCOMPILERG(current_include) = 1;
		op_array = bcompiler_read(TSRMLS_C);
		if (!op_array) op_array = dummy_op_array(TSRMLS_C);
		/* this code should fix autoglobals with apache2 */
		else {
			zend_is_auto_global("_GET", sizeof("_GET")-1 TSRMLS_CC);
			zend_is_auto_global("_POST", sizeof("_POST")-1 TSRMLS_CC);
			zend_is_auto_global("_COOKIE", sizeof("_COOKIE")-1 TSRMLS_CC);
			zend_is_auto_global("_SERVER", sizeof("_SERVER")-1 TSRMLS_CC);
			zend_is_auto_global("_ENV", sizeof("_ENV")-1 TSRMLS_CC);
			zend_is_auto_global("_REQUEST", sizeof("_REQUEST")-1 TSRMLS_CC);
			zend_is_auto_global("_FILES", sizeof("_FILES")-1 TSRMLS_CC);
		}
#if 0 /* val: buffer allocation changed, so it can't be used anymore */
		BCOMPILERG(buffer) = (char *) emalloc(1); /* avoid 1 byte memory leak */
#endif
#if PHP_MAJOR_VERSION >= 6
	/* unicode modes don't match */
	} else if (test == -2) {
		php_error(E_WARNING, "bcompiler: Can't parse bytecodes created in %sunicode mode", UG(unicode) ? "non-" : ""); 
		op_array = dummy_op_array(TSRMLS_C);
#endif
	/* no bcompiler magic - just pass it to zend... */
	} else {
		op_array = bcompiler_saved_zend_compile_file(file_handle, type TSRMLS_CC);
	}

	if(stream) {
		php_stream_close(stream);
	}
	return op_array;
}

/* --- main read function -------------------------------------------------- */

/* {{{ bcompiler_read - the real reading method.*/
#define BCOMPILER_READ_RESULT (BCOMPILERG(parsing_error) ? dummy_op_array(TSRMLS_C) : op_array)
zend_op_array* bcompiler_read(TSRMLS_D) {
	char item;
	zend_class_entry* zc = NULL;
	zend_class_entry *ce = NULL;
	zend_function *zf;
	zend_function *zf2 = NULL;
	zend_constant *zconst = NULL;
	zend_op_array *op_array = NULL;
	char *key = NULL;
	int key_len;

	BCOMPILERG(parsing_error) = 0;	

	DESERIALIZE_SCALAR_V(&item, char, BCOMPILER_READ_RESULT);
	
	while (item) {
	   
		switch (item) {
	
		case BCOMPILER_CLASS_ENTRY:
			BCOMPILER_DEBUG(("STARTING CLASS\n"));
			apc_create_zend_class_entry(&zc, &key, &key_len TSRMLS_CC); 
			if (BCOMPILERG(parsing_error)) break;
			if (!key) { 
#if PHP_MAJOR_VERSION >= 6
				zstr lcname = zend_u_str_case_fold(BCOMPILERG(is_unicode) ? IS_UNICODE : IS_STRING, zc->name, zc->name_length, 0, &key_len);
				key = lcname.s;
                key_len++;
#else
				key = estrndup(zc->name, zc->name_length);
				if (key) zend_str_tolower(key, zc->name_length);
				key_len = zc->name_length+1; 
#endif
			}
			/* is it in the class table already? */
		
#if PHP_MAJOR_VERSION >= 6
			zend_u_hash_find(EG(class_table), BCOMPILERG(is_unicode) ? IS_UNICODE : IS_STRING, ZSTR(key), key_len, (void **)&ce);
#else
			zend_hash_find(EG(class_table), key, key_len, (void **)&ce);
#endif
			if (ce) {
				php_error(E_WARNING, "Could not redefine class %s", zc->name);
				efree(zc);
				zc = NULL;
			} else {
#ifdef ZEND_ENGINE_2
/*				zc->refcount++;*/
#else
/*				(*zc->refcount)++;*/
#endif
				BCOMPILER_DEBUG(("ADDING %s [%c%s]\n", zc->name, *key, key_len>1 ? key+1 : ""));
#if PHP_MAJOR_VERSION >= 6
				if (zend_u_hash_add(EG(class_table), BCOMPILERG(is_unicode) ? IS_UNICODE : IS_STRING, ZSTR(key), key_len, &zc, sizeof(zend_class_entry*), NULL) == FAILURE) { 
/*					zc->refcount--;*/
#elif defined(ZEND_ENGINE_2)
				if (zend_hash_add(EG(class_table), key, key_len, &zc, sizeof(zend_class_entry*), NULL) == FAILURE) { 
/*					zc->refcount--;*/
#else
				if (zend_hash_add(EG(class_table), key, key_len, zc, sizeof(zend_class_entry), NULL) == FAILURE) { 
/*					(*zc->refcount)--;*/
#endif
					zend_hash_destroy(&zc->function_table);
#ifndef ZEND_ENGINE_2_4 /* todo */
					zend_hash_destroy(&zc->default_properties);
#endif
					php_error(E_ERROR, "bcompiler: Read Past End of File");
				}
#ifndef ZEND_ENGINE_2
				else { efree(zc); zc = NULL; } /* we don't need it */
#endif
			}
			if (key && (!zc || key != ZS2S(zc->name))) efree(key);
			break;
		case BCOMPILER_INCTABLE_ENTRY:
			BCOMPILER_DEBUG(("STARTING INC FILE ENTRY\n"));
			{
				char *file_name;
				int dummy = 1;
				apc_create_string(&file_name TSRMLS_CC);
				zend_hash_add(&EG(included_files), file_name, strlen(file_name)+1, (void *)&dummy, sizeof(int), NULL);
			}
			break;
		
		case BCOMPILER_FUNCTION_ENTRY:
		case BCOMPILER_FUNCTION_ENTRY_EX:
			BCOMPILER_DEBUG(("STARTING FUNCTION\n"));
			{
#if PHP_MAJOR_VERSION >= 6
				zstr fname;
#else
				char *fname;
#endif
				int fname_len; /* beware: it's a hash key len, strlen()+1 */

				/* Read index string for functions that are renamed at runtime -Zobo */
				if (item == BCOMPILER_FUNCTION_ENTRY_EX) {
					/* hack this for PHP6 unicode! */
					DESERIALIZE_SCALAR_V(&fname_len, int, BCOMPILER_READ_RESULT);
					apc_create_string_u(ZS2SP(fname), -1 TSRMLS_CC);
				}

				apc_create_zend_function(&zf TSRMLS_CC);
				if (BCOMPILERG(parsing_error)) break;

				if (item != BCOMPILER_FUNCTION_ENTRY_EX) {
#if PHP_MAJOR_VERSION >= 6
					fname = zend_u_str_case_fold(BCOMPILERG(is_unicode) ? IS_UNICODE : IS_STRING, zf->op_array.function_name, BCOMPILERG(is_unicode) ? u_strlen(zf->op_array.function_name.u) : strlen(zf->op_array.function_name.s), 0, &fname_len);
					fname_len++;
#elif defined(ZEND_ENGINE_2)
					fname = zend_str_tolower_dup(zf->op_array.function_name, strlen(zf->internal_function.function_name));
					fname_len = strlen(zf->op_array.function_name) + 1;
#else
					fname = estrdup(zf->op_array.function_name);
					fname_len = strlen(zf->op_array.function_name) + 1;
#endif
				}
				/* Is it in the functions table already? */
#if PHP_MAJOR_VERSION >= 6
				if (zend_u_hash_find(EG(function_table), BCOMPILERG(is_unicode) ? IS_UNICODE : IS_STRING, fname, fname_len, (void **)&zf2) == SUCCESS)
#else
				if (zend_hash_find(EG(function_table), fname, fname_len, (void **)&zf2) == SUCCESS)
#endif
				{
					php_error(E_WARNING, "Could not redefine function %s", zf->op_array.function_name);
					/* efree(zf); // Damjan Cvetko patch, 27 Dec 2007 */
				} else {
					BCOMPILER_DEBUG(("ADDING FUNCTION %s \n", zf->op_array.function_name));
#if PHP_MAJOR_VERSION >= 6
					zend_u_hash_add(EG(function_table), BCOMPILERG(is_unicode) ? IS_UNICODE : IS_STRING, fname, fname_len, zf, sizeof(zend_function), NULL);
#else
					zend_hash_add(EG(function_table), fname, fname_len, zf, sizeof(zend_function), NULL);
#endif
				}

				apc_free_zend_function(&zf TSRMLS_CC);
				efree(ZS2S(fname));
			}
			break;
		case BCOMPILER_CONSTANT:
			BCOMPILER_DEBUG(("STARTING CONSTANT\n"));
			apc_create_zend_constant(&zconst TSRMLS_CC);
#ifdef ZEND_ENGINE_2_4
			zend_register_constant(zconst TSRMLS_CC);
#else
			switch(zconst->value.type)
			{
				case IS_LONG:
			        zend_register_long_constant(zconst->name, zconst->name_len, zconst->value.value.lval, zconst->flags, 0 TSRMLS_CC);
					break;
			    case IS_STRING:
				    zend_register_stringl_constant(zconst->name, zconst->name_len, zconst->value.value.str.val, zconst->value.value.str.len, zconst->flags, 0 TSRMLS_CC);
					break;
				case IS_DOUBLE:
			        zend_register_double_constant(zconst->name, zconst->name_len, zconst->value.value.dval, zconst->flags, 0 TSRMLS_CC);
					break;
			}
#endif
			break;

		case BCOMPILER_OP_ARRAY:
			BCOMPILER_DEBUG(("STARTING OP_ARRAY\n"));
			apc_create_zend_op_array(&op_array TSRMLS_CC);
			break;
		}
		DESERIALIZE_SCALAR_V(&item, char, BCOMPILER_READ_RESULT);
	}
	return BCOMPILER_READ_RESULT;
}
/* }}} */

/* --- filename_handler ---------------------------------------------------- */

/* {{{ bcompiler_handle_filename - handle filename.*/
char *bcompiler_handle_filename(char *filename TSRMLS_DC) {
	zval *params[1];
	zval *retval = NULL;
	char *name;

	if (BCOMPILERG(filename_handler) == NULL) {
		return BCOMPILERG(filename_handler_name) == NULL ? estrdup(filename): NULL;
	}

	MAKE_STD_ZVAL(params[0]);
	ZVAL_STRING(params[0], filename, 1);
	MAKE_STD_ZVAL(retval);
	ZVAL_FALSE(retval);
	if (call_user_function(EG(function_table), NULL, BCOMPILERG(filename_handler), retval, 1, params TSRMLS_CC) == SUCCESS && retval) {
		convert_to_string_ex(&retval);
		name = estrdup(Z_STRVAL_P(retval));
	} else {
#if PHP_MAJOR_VERSION >= 6
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Error calling filename handler callback '%R'", Z_TYPE_P(BCOMPILERG(filename_handler_name)), Z_UNIVAL_P(BCOMPILERG(filename_handler_name)));
#else
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Error calling filename handler callback '%s'", BCOMPILERG(filename_handler_name));
#endif
		name = estrdup(filename);
	}
	zval_ptr_dtor(&(params[0]));
	zval_ptr_dtor(&retval);
	return name;
}
/* }}} */

/* --- string functions ---------------------------------------------------- */

void apc_serialize_string(char* string TSRMLS_DC)
{
	int len;

	/* by convention, mark null strings with a length of -1 */
	if (string == NULL) {
		SERIALIZE_SCALAR(-1, int);
		return;
	}

	len = strlen(string);
	if (len <= 0) {
		if (BCOMPILERG(current_write) < 0x0010) {
			SERIALIZE_SCALAR(-1, int);
		} else {
			SERIALIZE_SCALAR(0, int);
		}
		return;
	}
	BCOMPILER_DEBUG(("STRING: len, string : %s\n", string)); 
	SERIALIZE_SCALAR(len, int);
	STORE_BYTES(string, len);
}

void apc_serialize_zstring(char* string, int len TSRMLS_DC)
{
	/* by convention, mark null strings with a length of -1 */
	if (string == NULL) {
		SERIALIZE_SCALAR(-1, int);
		return;
	}
	if (len <= 0) {
		if (BCOMPILERG(current_write) < 0x0010) {
			SERIALIZE_SCALAR(-1, int);
		} else {
			SERIALIZE_SCALAR(0, int);
		}
		return;
	}
	SERIALIZE_SCALAR(len, int);
	STORE_BYTES(string, len);
}

int apc_create_string(char** string TSRMLS_DC)
{
	int len = 0;

	DESERIALIZE_SCALAR_V(&len, int, -1);
	if (len <= 0) {
		/* starting from 0.16, -1 len means NULL string */
		if (BCOMPILERG(current_version) >= 0x0010 && len == -1) {
			*string = NULL;
			return -1;
		}
		/* by convention, empty marked strings (len is -1)  are returned as an empty string */
		len = 0;
	}
	*string = (char*) emalloc(len + 1);
	if (len > 0) { LOAD_BYTES_V(*string, (unsigned int)len, -1); }
	(*string)[len] = '\0';
	return len;
}

int apc_create_string_u(char** string, int unicode TSRMLS_DC)
{
	int len = 0;

	if (unicode < 0) unicode = BCOMPILERG(is_unicode);
	DESERIALIZE_SCALAR_V(&len, int, -1);
	if (len <= 0) {
		/* starting from 0.16, -1 len means NULL string */
		if (BCOMPILERG(current_version) >= 0x0010 && len == -1) {
			*string = NULL;
			return -1;
		}
		/* by convention, empty marked strings (len is -1)  are returned as an empty string */
		len = 0;
	}
	*string = (char*) emalloc(len + (unicode ? 2 : 1));
	if (len > 0) { LOAD_BYTES_V(*string, (unsigned int)len, -1); }
	(*string)[len] = '\0';
	if (unicode) (*string)[len + 1] = '\0';
	return unicode ? len >> 1 : len;
}

void serialize_magic(int ver TSRMLS_DC)
{
	char *tmp;

	if (!ver) {
		tmp = BCOMPILER_MAGIC_HEADER;
#if PHP_MAJOR_VERSION >= 6
		if (UG(unicode)) tmp[strlen(tmp)-2] = *(BCOMPILER_UNICODE);
#endif
		apc_serialize_string(tmp TSRMLS_CC);
	} else {
		spprintf(&tmp, 1024, BCOMPILER_MAGIC_START "%u.%u%c", (ver >> 8) & 0xff, ver & 0xff, 
#if PHP_MAJOR_VERSION >= 6
					UG(unicode) ? *(BCOMPILER_UNICODE) : *(BCOMPILER_STREAMS)
#else
					*(BCOMPILER_STREAMS)
#endif
				);
		apc_serialize_string(tmp TSRMLS_CC);
		efree(tmp);
	}
}

/* A replacement for apc_deserialize_magic() */
int deserialize_magic(TSRMLS_D)
{
	char *tmp, c;
	int retval;
	int len = 0;
	unsigned int hi, lo;

	/* do not use macros because of empty file problem (bug #10742) */
	/* DESERIALIZE_SCALAR_V(&len, int, -1); */
	{
		size_t act = php_stream_read(BCOMPILERG(stream), (char *) &len, BCOMPILERG(bcompiler_stdsize)[BCSI_int]);
		if (act != BCOMPILERG(bcompiler_stdsize)[BCSI_int]) return -1;
	}
	/* just a sanity check */
	if (len <= 0 || len > 0x20) {
		return -1;
	}
	
	tmp = (char *) emalloc(len + 1);
	LOAD_BYTES_V(tmp, (unsigned int)len, -1);
	tmp[len] = '\0';
	BCOMPILER_DEBUGFULL(("got magic %s\n", tmp));
	
	retval = sscanf(tmp, BCOMPILER_MAGIC_START "%u.%u%c", &hi, &lo, &c);
#if PHP_MAJOR_VERSION >= 6
	if (retval == 3 && (c == *(BCOMPILER_STREAMS) || c == *(BCOMPILER_UNICODE)))
#else
	if (retval == 3 && c == *(BCOMPILER_STREAMS))
#endif
	{
		BCOMPILERG(current_version) = ((hi & 0xff) << 8) + (lo & 0xff);
		retval = bcompiler_can_read(BCOMPILERG(current_version)) ? 0 : -1;
#if PHP_MAJOR_VERSION >= 6
		BCOMPILERG(is_unicode) = (c == *(BCOMPILER_UNICODE));
		if (UG(unicode) != BCOMPILERG(is_unicode)) retval = -2;
#endif
		BCOMPILER_DEBUG(("bytecode version %u.%u%c (can parse: %s)\n", hi, lo, c, retval == 0 ? "yes" : "no"));
		bcompiler_set_stdsize(BCOMPILERG(current_version), &BCOMPILERG(bcompiler_stdsize));
	} else {
		if (retval == 3) {
			BCOMPILER_DEBUG(("bytecode version %u.%u%c (can parse: no)\n", hi, lo, c));
		}
		retval = -1;
	}
	
	efree(tmp);
	return retval;
}



/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
