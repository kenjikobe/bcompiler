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

/* $Id: bcompiler_zend.c 311909 2011-06-08 01:33:35Z alan_k $ */

#include "php_bcompiler.h"

void apc_serialize_zval(zval* zv, znode *zn TSRMLS_DC);
void apc_serialize_zval_ptr(zval** zv TSRMLS_DC);
void apc_create_zval(zval** zv TSRMLS_DC);

/* --- zend_llist ---------------------------------------------------------- */

static void store_zend_llist_element(void* arg, void* data  TSRMLS_DC) {
	int size = *((int*)arg);
	STORE_BYTES((char*)data, size);
}


void apc_serialize_zend_llist(zend_llist* list TSRMLS_DC) {
	char exists;

	exists = (list != NULL) ? 1 : 0;
	SERIALIZE_SCALAR(exists, char);  /* write twice to remove need for peeking. */

	if (!exists) {
		return;
	}
	SERIALIZE_SCALAR(exists, char);
	SERIALIZE_SCALAR(list->size, size_t);
	/** old code: SERIALIZE_SCALAR(list->dtor, void*); */
	if (BCOMPILERG(current_write) < 0x0005) SERIALIZE_SCALAR(0, ulong); /* dummy 4 bytes */
	/** old code: SERIALIZE_SCALAR(list->persistent, unsigned char); */
	SERIALIZE_SCALAR(list->persistent, zend_uchar);
	SERIALIZE_SCALAR(zend_llist_count(list), int);
	zend_llist_apply_with_argument(list, store_zend_llist_element, &list->size TSRMLS_CC);
}

void apc_deserialize_zend_llist(zend_llist* list TSRMLS_DC) {
	char exists;
	size_t size;
	void (*dtor)(void*);
	unsigned char persistent;
	int count, i;
	char* data;

	DESERIALIZE_SCALAR(&exists, char);
	assert(exists != 0);

	/* read the list parameters */
	DESERIALIZE_SCALAR(&size, size_t);
	/** old code: DESERIALIZE_SCALAR(&dtor, void*); */
	if (BCOMPILERG(current_version) < 0x0005) DESERIALIZE_VOID(ulong); /* dummy 4 bytes */
	dtor = NULL;
	/** old code: DESERIALIZE_SCALAR(&persistent, unsigned char); */
	DESERIALIZE_SCALAR(&persistent, zend_uchar);
	/* initialize the list */
	zend_llist_init(list, size, dtor, persistent);

	/* insert the list elements */
	DESERIALIZE_SCALAR(&count, int);
	data = (char*) emalloc(list->size);
	for (i = 0; i < count; i++) {
		LOAD_BYTES(data, list->size);
		zend_llist_add_element(list, data);
	}
	efree(data);
}

void apc_create_zend_llist(zend_llist** list TSRMLS_DC)
{
	char exists;

	DESERIALIZE_SCALAR(&exists, char);
	if (exists) {
		*list = (zend_llist*) ecalloc(1, sizeof(zend_llist));
		apc_deserialize_zend_llist(*list TSRMLS_CC);
	}
	else {
		*list = 0;
	}
}


/* --- HashTable ----------------------------------------------------------- */

static ulong pDestructor_idx(dtor_func_t p TSRMLS_DC) {
	if (BCOMPILERG(current_write) >= 0x0005) {
		if (p == ZVAL_PTR_DTOR) return 1;
		if (p == ZEND_FUNCTION_DTOR) return 2;
		if (p == ZEND_CLASS_DTOR) return 3;
#ifdef COMPILE_DL_BCOMPILER /* val: these may be missing in shared lib */
		if (p == module_registry.pDestructor) return 4;
		if (p == EG(zend_constants)->pDestructor) return 5;
#else
		if (p == ZEND_MODULE_DTOR) return 4;
		if (p == ZEND_CONSTANT_DTOR) return 5;
#endif
		if (p == (dtor_func_t)free_estring) return 6;
#ifdef COMPILE_DL_BCOMPILER /* val: these may be missing in shared lib */
		if (p == EG(regular_list).pDestructor) return 7;
		if (p == EG(persistent_list).pDestructor) return 8;
#else
		if (p == list_entry_destructor) return 7;
		if (p == plist_entry_destructor) return 8;
#endif
	}
	return 0;
}

#ifdef ZEND_ENGINE_2
/* since zend_function is copied in deserialize_hashtable, we need to update
   special class function pointers */
static void adjust_class_handler(zend_class_entry *zc, zend_function *old, zend_function *act) {
	if (zc) {
		if (zc->constructor == old) zc->constructor = act;
		if (zc->destructor == old) zc->destructor = act;
		if (zc->clone == old) zc->clone = act;
		if (zc->__get == old) zc->__get = act;
		if (zc->__set == old) zc->__set = act;
		if (zc->__call == old) zc->__call = act;
#ifdef ZEND_ENGINE_2_1
		if (zc->__unset == old) zc->__unset = act;
		if (zc->__isset == old) zc->__isset = act;
		if (zc->serialize_func == old) zc->serialize_func = act;
		if (zc->unserialize_func == old) zc->unserialize_func = act;
#endif
#ifdef ZEND_ENGINE_2_2
		if (zc->__tostring == old) zc->__tostring = act;
#endif
#ifdef ZEND_ENGINE_2_3
		if (zc->__callstatic == old) zc->__callstatic = act;
#endif
	}
}
#endif

void apc_serialize_hashtable(HashTable* ht, void* funcptr TSRMLS_DC)
{
	char exists;    /* for identifying null lists */
	Bucket* p;
	void (*serialize_bucket)(void* TSRMLS_DC);

	serialize_bucket = (void(*)(void* TSRMLS_DC)) funcptr;

	exists = (ht != NULL) ? 1 : 0;
	SERIALIZE_SCALAR(exists, char);
	if (!exists) {
		return;
	}

	/* Serialize the hash meta-data. */
	SERIALIZE_SCALAR(ht->nTableSize, uint);
	/** old code: SERIALIZE_SCALAR(ht->pDestructor, void*); */
	SERIALIZE_SCALAR(pDestructor_idx(ht->pDestructor TSRMLS_CC), ulong);
	SERIALIZE_SCALAR(ht->nNumOfElements, uint);
	SERIALIZE_SCALAR(ht->persistent, int);
#if PHP_MAJOR_VERSION >= 6
	SERIALIZE_SCALAR(ht->unicode, zend_bool);
#endif

	/* Iterate through the buckets of the hash, serializing as we go. */
	p = ht->pListHead;
	while(p != NULL) {
		SERIALIZE_SCALAR(p->h, ulong);
		SERIALIZE_SCALAR(p->nKeyLength,uint);
#if PHP_MAJOR_VERSION >= 6
		SERIALIZE_SCALAR(p->key.type,zend_uchar);
		apc_serialize_zstring(p->key.arKey.s, USTR_BYTES(p->key.type, p->nKeyLength) TSRMLS_CC);
#else
		apc_serialize_zstring(p->arKey, p->nKeyLength TSRMLS_CC);
#endif
		serialize_bucket(p->pData TSRMLS_CC );
		p = p->pListNext;
	}
}

void apc_deserialize_hashtable(HashTable* ht, void* funcptr, void* dptr, int datasize, char exists TSRMLS_DC)
{
	uint nSize;
	dtor_func_t pDestructor;
	uint nNumOfElements;
	int persistent;
	int j;
	ulong h;
	uint nKeyLength;
	char* arKey;
#if PHP_MAJOR_VERSION >= 6
	zend_uchar keyType;
#endif
	void* pData;
	int status;
	void (*deserialize_bucket)(void* TSRMLS_DC);
	void (*free_bucket)(void*);
	BCOMPILER_DEBUG(("-----------------------------\nHASH TABLE:\n"));
	deserialize_bucket = (void(*)(void* TSRMLS_DC)) funcptr;
	free_bucket = (void(*)(void*)) dptr;

	assert(exists != 0);

	DESERIALIZE_SCALAR(&nSize, uint);
	/** old code: DESERIALIZE_SCALAR(&pDestructor, void*); */
	DESERIALIZE_SCALAR(&h, ulong);
	pDestructor = NULL;
	if (BCOMPILERG(current_version) >= 0x0005) switch (h) {
		case 1: pDestructor = ZVAL_PTR_DTOR; break;
		case 2: pDestructor = ZEND_FUNCTION_DTOR; break;
		case 3: pDestructor = ZEND_CLASS_DTOR; break;
#ifdef COMPILE_DL_BCOMPILER /* val: these may be missing in shared lib */
		case 4: pDestructor = module_registry.pDestructor; break;
		case 5: pDestructor = EG(zend_constants)->pDestructor; break;
#else
		case 4: pDestructor = ZEND_MODULE_DTOR; break;
		case 5: pDestructor = ZEND_CONSTANT_DTOR; break;
#endif
		case 6: pDestructor = (dtor_func_t)free_estring; break;
#ifdef COMPILE_DL_BCOMPILER /* val: these may be missing in shared lib */
		case 7: pDestructor = EG(regular_list).pDestructor; break;
		case 8: pDestructor = EG(persistent_list).pDestructor; break;
#else
		case 7: pDestructor = list_entry_destructor; break;
		case 8: pDestructor = plist_entry_destructor; break;
#endif
	}
	DESERIALIZE_SCALAR(&nNumOfElements,uint);
	DESERIALIZE_SCALAR(&persistent, int);

	BCOMPILER_DEBUG(("-----------------------------\nBEGIN TABLE:\n"));

	/* Although the hash is already allocated (we're a deserialize, not a
	 * create), we still need to initialize it. If this fails, something
	 * very very bad happened. */
	status = zend_hash_init(ht, nSize, NULL, pDestructor, persistent);
	assert(status != FAILURE);

#if PHP_MAJOR_VERSION >= 6
	DESERIALIZE_SCALAR(&ht->unicode, zend_bool);
#endif

	/* Luckily, the number of elements in a hash is part of its struct, so
	 * we can just deserialize that many hashtable elements. */

	for (j = 0; j < (int) nNumOfElements; j++) {
	DESERIALIZE_SCALAR(&h, ulong);
	DESERIALIZE_SCALAR(&nKeyLength, uint);
#if PHP_MAJOR_VERSION >= 6
	DESERIALIZE_SCALAR(&keyType, zend_uchar);
	apc_create_string_u(&arKey, -1 TSRMLS_CC);
#else
	apc_create_string(&arKey TSRMLS_CC);
#endif
	deserialize_bucket(&pData TSRMLS_CC);
	/* val: return pData = NULL to skip element */
	if (!pData && arKey) { efree(arKey); continue; }

	/* If nKeyLength is non-zero, this element is a hashed key/value
	 * pair. Otherwise, it is an array element with a numeric index */
	BCOMPILER_DEBUG(("-----------------------------\nFUNC %s\n",arKey));

	if (nKeyLength != 0) {
		if(datasize == sizeof(void*)) {
#if PHP_MAJOR_VERSION >= 6
			BCOMPILER_DEBUGFULL(("adding element [ptr] type=%d len=%d ptr=%p\n", keyType, nKeyLength, &pData));
			_zend_u_hash_add_or_update(ht, keyType, ZSTR(arKey), nKeyLength, &pData,
				datasize, NULL, HASH_ADD ZEND_FILE_LINE_CC);
#elif defined(ZEND_ENGINE_2)
			BCOMPILER_DEBUGFULL(("adding element [ptr] %s = %p\n", arKey, &pData));
			_zend_hash_add_or_update(ht, arKey, nKeyLength, &pData,
				datasize, NULL, HASH_ADD ZEND_FILE_LINE_CC);
#else
			zend_hash_add_or_update(ht, arKey, nKeyLength, &pData,
				datasize, NULL, HASH_ADD);
#endif
		} else {
#ifdef ZEND_ENGINE_2
			void *pDest;
#endif
			BCOMPILER_DEBUGFULL(("adding element [cpy] %s = %p\n", arKey, pData));
#ifdef ZEND_ENGINE_2
# if PHP_MAJOR_VERSION >= 6
			_zend_u_hash_add_or_update(ht, keyType, ZSTR(arKey), nKeyLength, pData,
				datasize, &pDest, HASH_ADD ZEND_FILE_LINE_CC);
# else
			_zend_hash_add_or_update(ht, arKey, nKeyLength, pData,
				datasize, &pDest, HASH_ADD ZEND_FILE_LINE_CC);
# endif
			BCOMPILER_DEBUGFULL(("actual added element ptr=%p\n", pDest));
			adjust_class_handler(BCOMPILERG(cur_zc), pData, pDest);
#else
			zend_hash_add_or_update(ht, arKey, nKeyLength, pData,
				datasize, NULL, HASH_ADD);
#endif
		}
	} else {  /* numeric index, not key string */
		if(datasize == sizeof(void*)) {
			zend_hash_index_update(ht, h, &pData, datasize, NULL);
		} else {
			zend_hash_index_update(ht, h, pData, datasize, NULL);
		}
	}
	if (dptr != NULL) {
		free_bucket(&pData);
	}
	/* else {
	     BCOMPILER_DEBUG((stderr, "Do not free: %s\n", arKey));
	} */
	if (arKey) efree(arKey); /* ADDED BY MPB */
	}
}

void apc_create_hashtable(HashTable** ht, void* funcptr, void* dptr, int datasize TSRMLS_DC)
{
	char exists;    /* for identifying null hashtables */

	/*PEEK_SCALAR(&exists, char); */
	DESERIALIZE_SCALAR(&exists, char);
	if (exists==1) {
		*ht = (HashTable*) ecalloc(1, sizeof(HashTable));
		apc_deserialize_hashtable(*ht, funcptr, dptr, datasize,exists TSRMLS_CC);
	} else {
		/* DESERIALIZE_SCALAR(&exists, char); */
		*ht = NULL;
	}
}


/* --- zvalue_value -------------------------------------------------------- */

#define IS_MANGLED_FILE 0x81
#define IS_MANGLED_DIR  0x82

/* serialize a suspected __FILE__ or __DIR__ entry */
static int bc_serialize_filedir(zvalue_value* zv, zend_uchar type TSRMLS_DC) {
	int done = 0;
	/* quick check for the string type and bytecode version */
	if (!BCOMPILERG(detect_filedir) || type != IS_STRING) return 0;
	if (BCOMPILERG(current_write) < 25) return 0;
	/* proceed if the current filename is known */
	if (BCOMPILERG(zoa) && BCOMPILERG(zoa)->filename) {
#ifdef ZEND_ENGINE_2_3
		char *dir = estrdup(BCOMPILERG(zoa)->filename);
		size_t n = zend_dirname(dir, strlen(dir));
#endif
		if (!strcmp(zv->str.val, BCOMPILERG(zoa)->filename)) {
			BCOMPILER_DEBUG(("Mangling possible __FILE__ entry: %s\n", zv->str.val));
			SERIALIZE_SCALAR(IS_MANGLED_FILE, zend_uchar);
			done = 1;
		}
#ifdef ZEND_ENGINE_2_3
		/* to prevent false dir detections, proceed with lengths >= 8 */
		else if (n >= 8 && !strcmp(zv->str.val, dir)) {
			BCOMPILER_DEBUG(("Mangling possible __DIR__ entry: %s\n", dir));
			SERIALIZE_SCALAR(IS_MANGLED_DIR, zend_uchar);
			done = 1;
		}
		efree(dir);
#endif
	}
	return done;
}

/* deserialize a stored __FILE__ or __DIR__ entry */
static int bc_deserialize_filedir(zvalue_value* zv, zend_uchar *type TSRMLS_DC) {
	/* quick check it's not we're looking for */
	if (BCOMPILERG(current_version) < 25) return 0;
	if (*type != IS_MANGLED_FILE && *type != IS_MANGLED_DIR) return 0;
	/* create a string constant */
	if (zv->str.val) efree(zv->str.val);
	if (*type == IS_MANGLED_FILE) {
		zv->str.val = estrdup( BCOMPILERG(current_filename) );
		BCOMPILER_DEBUG(("Demangling possible __FILE__ entry: %s\n", zv->str.val));
	}
	else if (*type == IS_MANGLED_DIR) {
#ifdef ZEND_ENGINE_2_3
		char *dir = estrdup( BCOMPILERG(current_filename) );
		zend_dirname(dir, strlen(dir));
		zv->str.val = dir;
		BCOMPILER_DEBUG(("Demangling possible __DIR__ entry: %s\n", zv->str.val));
#else
		zv->str.val = estrdup("__DIR__");
		BCOMPILER_DEBUG(("Error: __DIR__ is unavailable for the current Zend version\n"));
#endif
	}
	zv->str.len = strlen(zv->str.val);
	*type = IS_STRING;
	return 1;
}

void apc_serialize_zvalue_value(zvalue_value* zv, zend_uchar type, znode *zn TSRMLS_DC)
{
	/* A zvalue_value is a union, and as such we first need to
	 * determine exactly what it's type is, then serialize the
	 * appropriate structure. */
#if defined(IS_CONSTANT_TYPE_MASK)
	type &= IS_CONSTANT_TYPE_MASK;
#elif defined(IS_CONSTANT_INDEX)
	type &= ~IS_CONSTANT_INDEX;
#endif
	/* temp hack - PHP6 seems to mix unicode and non-unicode IS_CONSTANT */
#if PHP_MAJOR_VERSION >= 6
	if (type == IS_CONSTANT) type = UG(unicode) ? IS_UNICODE : IS_STRING;
#endif
	switch (type)
	{
	case IS_RESOURCE:
	case IS_BOOL:
	case IS_LONG:
		SERIALIZE_SCALAR(zv->lval, long);
		break;
	case IS_DOUBLE:
		SERIALIZE_SCALAR(zv->dval, double);
		break;
	case IS_NULL:
		/* null value - it's used for temp_vars */
#if defined(ZEND_ENGINE_2) || defined(ZEND_ENGINE_2_1)
		if (zn && BCOMPILERG(current_write) >= 0x0008) {
# ifdef ZEND_ENGINE_2_4
			SERIALIZE_SCALAR(zn->EA, zend_uint);
# else
			SERIALIZE_SCALAR(zn->u.EA.var,  zend_uint);
			SERIALIZE_SCALAR(zn->u.EA.type, zend_uint);
# endif
		}
#endif
		if (BCOMPILERG(current_write) >= 0x0005) SERIALIZE_SCALAR(zv->lval, long);
		break;
	case IS_CONSTANT:
	case IS_STRING:
#ifndef ZEND_ENGINE_2
	case FLAG_IS_BC:
#endif
		apc_serialize_zstring(zv->str.val, zv->str.len  TSRMLS_CC);
		SERIALIZE_SCALAR(zv->str.len, int);
		break;
#if PHP_MAJOR_VERSION >= 6
	case IS_UNICODE:
		apc_serialize_zstring((char*)zv->ustr.val, UBYTES(zv->ustr.len)  TSRMLS_CC);
		SERIALIZE_SCALAR(zv->ustr.len, int);
		break;
#endif
	case IS_ARRAY:
		apc_serialize_hashtable(zv->ht, apc_serialize_zval_ptr  TSRMLS_CC);
		break;
	case IS_CONSTANT_ARRAY:
		apc_serialize_hashtable(zv->ht, apc_serialize_zval_ptr TSRMLS_CC);
		break;
	case IS_OBJECT:
#ifdef ZEND_ENGINE_2
		/* not yet!
		apc_serialize_zend_class_entry(zv->obj.handlers, NULL, 0, NULL, 0 TSRMLS_CC);
		apc_serialize_hashtable(zv->obj.properties, apc_serialize_zval_ptr TSRMLS_CC);
		*/
#else
		apc_serialize_zend_class_entry(zv->obj.ce, NULL, 0, NULL, 0 TSRMLS_CC);
#endif
		break;
#ifdef ZEND_ENGINE_2_4
	/*case IS_CLASS:
	case IS_SCALAR:
	case IS_NUMERIC:
		break;*/
#endif
	default:
		/* The above list enumerates all types.  If we get here,
		 * something very very bad has happened. */
		assert(0);
	}
}

void apc_deserialize_zvalue_value(zvalue_value* zv, zend_uchar type, znode *zn TSRMLS_DC)
{
	/* We peeked ahead in the calling routine to deserialize the
	 * type. Now we just deserialize. */
#if defined(IS_CONSTANT_TYPE_MASK)
	type &= IS_CONSTANT_TYPE_MASK;
#elif defined(IS_CONSTANT_INDEX)
	type &= ~IS_CONSTANT_INDEX;
#endif
	/* temp hack - PHP6 seems to mix unicode and non-unicode IS_CONSTANT */
#if PHP_MAJOR_VERSION >= 6
	if (type == IS_CONSTANT) type = BCOMPILERG(is_unicode) ? IS_UNICODE : IS_STRING;
#endif
	switch(type)
	{
	case IS_RESOURCE:
	case IS_BOOL:
	case IS_LONG:
		DESERIALIZE_SCALAR(&zv->lval, long);
		break;
	case IS_NULL:
		/* null value - it's used for temp_vars */
#if defined(ZEND_ENGINE_2) || defined(ZEND_ENGINE_2_1)
		if (zn && BCOMPILERG(current_version) >= 0x0008) {
# ifdef ZEND_ENGINE_2_4
			DESERIALIZE_SCALAR(&zn->EA, zend_uint);
# else
			DESERIALIZE_SCALAR(&zn->u.EA.var,  zend_uint);
			DESERIALIZE_SCALAR(&zn->u.EA.type, zend_uint);
# endif
		}
#endif
		if (BCOMPILERG(current_version) >= 0x0005) DESERIALIZE_SCALAR(&zv->lval, long);
		break;
	case IS_DOUBLE:
		DESERIALIZE_SCALAR(&zv->dval, double);
		break;
	case IS_CONSTANT:
	case IS_STRING:
#ifndef ZEND_ENGINE_2
	case FLAG_IS_BC:
#endif
#if PHP_MAJOR_VERSION >= 6
		apc_create_string_u(&zv->str.val, 0 TSRMLS_CC);
#else
		apc_create_string(&zv->str.val TSRMLS_CC);
#endif
		DESERIALIZE_SCALAR(&zv->str.len, int);
		break;
#if PHP_MAJOR_VERSION >= 6
	case IS_UNICODE:
		apc_create_string_u((char**)&zv->ustr.val, 1 TSRMLS_CC);
		DESERIALIZE_SCALAR(&zv->ustr.len, int);
		break;
#endif
	case IS_ARRAY:
		apc_create_hashtable(&zv->ht, apc_create_zval, NULL, sizeof(void*) TSRMLS_CC);
		break;
	case IS_CONSTANT_ARRAY:
		apc_create_hashtable(&zv->ht, apc_create_zval, NULL, sizeof(void*) TSRMLS_CC);
		break;
	case IS_OBJECT:
#ifndef ZEND_ENGINE_2
		apc_create_zend_class_entry(&zv->obj.ce, NULL, NULL TSRMLS_CC);
		apc_create_hashtable(&zv->obj.properties, apc_create_zval, NULL, sizeof(zval *) TSRMLS_CC);
#endif
		break;
#ifdef ZEND_ENGINE_2_4
	/*case IS_CLASS:
	case IS_SCALAR:
	case IS_NUMERIC:
		break;*/
#endif
	default:
		/* The above list enumerates all types.  If we get here,
		 * something very very bad has happened. */
		assert(0);
	}
}


/* --- zval ---------------------------------------------------------------- */

void apc_serialize_zval_ptr(zval** zv TSRMLS_DC)
{
	apc_serialize_zval(*zv, NULL TSRMLS_CC);
}

void apc_serialize_zval(zval* zv, znode *zn TSRMLS_DC)
{
	/* type is the switch for serializing zvalue_value */
	if (!bc_serialize_filedir(&zv->value, zv->type TSRMLS_CC)) {
		SERIALIZE_SCALAR(zv->type, zend_uchar);
		apc_serialize_zvalue_value(&zv->value, zv->type, zn TSRMLS_CC);
	}
#ifdef ZEND_ENGINE_2_3
	SERIALIZE_SCALAR(zv->is_ref__gc, zend_uchar);
	SERIALIZE_SCALAR(zv->refcount__gc, zend_ushort);
#else
	SERIALIZE_SCALAR(zv->is_ref, zend_uchar);
	SERIALIZE_SCALAR(zv->refcount, zend_ushort);
#endif
}

void apc_deserialize_zval(zval* zv, znode *zn TSRMLS_DC)
{
	/* type is the switch for deserializing zvalue_value */
	DESERIALIZE_SCALAR(&zv->type, zend_uchar);
	if (!bc_deserialize_filedir(&zv->value, &zv->type TSRMLS_CC)) {
		apc_deserialize_zvalue_value(&zv->value, zv->type, zn TSRMLS_CC);
	}
#ifdef ZEND_ENGINE_2_3
	DESERIALIZE_SCALAR(&zv->is_ref__gc, zend_uchar);
	DESERIALIZE_SCALAR(&zv->refcount__gc, zend_ushort);
#else
	DESERIALIZE_SCALAR(&zv->is_ref, zend_uchar);
	DESERIALIZE_SCALAR(&zv->refcount, zend_ushort);
#endif
}

void apc_create_zval(zval** zv TSRMLS_DC)
{
#ifdef ZEND_ENGINE_2_2
	ALLOC_ZVAL(*zv);
#else
	*zv = (zval*) emalloc(sizeof(zval));
#endif
	memset(*zv, 0, sizeof(zval));
	apc_deserialize_zval(*zv, NULL TSRMLS_CC);
}


#ifdef ZEND_ENGINE_2
/* --- arg_info ------------------------------------------------------------ */

void apc_serialize_arg_info(zend_arg_info* arg_info TSRMLS_DC)
{
	if (arg_info == NULL) {
		SERIALIZE_SCALAR(0, char);
		return; /* arg_types is null */
	}
	SERIALIZE_SCALAR(1, char);
	SERIALIZE_SCALAR(arg_info->name_len       , zend_uint);
/*	apc_serialize_string(arg_info->name TSRMLS_CC);*/
	apc_serialize_zstring(ZS2S(arg_info->name), ZLEN(arg_info->name_len) TSRMLS_CC);
	SERIALIZE_SCALAR(arg_info->class_name_len  , zend_uint);
	if (arg_info->class_name_len) {
/*		apc_serialize_string(arg_info->class_name TSRMLS_CC);*/
		apc_serialize_zstring(ZS2S(arg_info->class_name), ZLEN(arg_info->class_name_len) TSRMLS_CC);
	}
	SERIALIZE_SCALAR(arg_info->allow_null       , zend_bool);
	SERIALIZE_SCALAR(arg_info->pass_by_reference, zend_bool);
	//SERIALIZE_SCALAR(arg_info->return_reference , zend_bool);
	//SERIALIZE_SCALAR(arg_info->required_num_args, int);
#ifdef ZEND_ENGINE_2_1
	if (BCOMPILERG(current_write) > 20) {
# ifdef ZEND_ENGINE_2_4
		SERIALIZE_SCALAR(arg_info->type_hint, zend_uint);
# else
		SERIALIZE_SCALAR(arg_info->array_type_hint, zend_bool);
# endif
	}
#endif
}

void apc_create_arg_info(zend_arg_info* arg_info TSRMLS_DC)
{
	char exists;

	DESERIALIZE_SCALAR(&exists, char);
	if (!exists) {
		arg_info = NULL;
		return; /* arg_types is null */
	}
	DESERIALIZE_SCALAR(&arg_info->name_len, zend_uint);
	apc_create_string_u(ZS2SP(arg_info->name), -1 TSRMLS_CC);
	DESERIALIZE_SCALAR(&arg_info->class_name_len, zend_uint);
	if (arg_info->class_name_len) {
		apc_create_string_u(ZS2SP(arg_info->class_name), -1 TSRMLS_CC);
	}
	else ZS2SR(arg_info->class_name) = NULL;
	DESERIALIZE_SCALAR(&arg_info->allow_null, zend_bool);
	DESERIALIZE_SCALAR(&arg_info->pass_by_reference, zend_bool);
	//DESERIALIZE_SCALAR(&arg_info->return_reference, zend_bool);
	//DESERIALIZE_SCALAR(&arg_info->required_num_args, int);
#ifdef ZEND_ENGINE_2_1
	if (BCOMPILERG(current_version) > 20) {
# ifdef ZEND_ENGINE_2_4
		DESERIALIZE_SCALAR(&arg_info->type_hint, zend_uint);
# else
		DESERIALIZE_SCALAR(&arg_info->array_type_hint, zend_bool);
# endif
	}
#endif
}


#else /* arg_info */
/* --- arg_types ----------------------------------------------------------- */

void apc_serialize_arg_types(zend_uchar* arg_types TSRMLS_DC)
{
	if (arg_types == NULL) {
		SERIALIZE_SCALAR(0, char);
		return; /* arg_types is null */
	}
	SERIALIZE_SCALAR(1, char);
	SERIALIZE_SCALAR(arg_types[0], zend_uchar);
	STORE_BYTES(arg_types + 1, arg_types[0]*sizeof(zend_uchar));
}

void apc_create_arg_types(zend_uchar** arg_types TSRMLS_DC)
{
	char exists;
	zend_uchar count;

	DESERIALIZE_SCALAR(&exists, char);
	if (!exists) {
		*arg_types = NULL;
		return; /* arg_types is null */
	}
	DESERIALIZE_SCALAR(&count, zend_uchar);
	*arg_types = emalloc(count + 1);
	(*arg_types)[0] = count;
	LOAD_BYTES((*arg_types) + 1, count*sizeof(zend_uchar));
}
#endif /* arg_types */


/* --- zend_function_entry ------------------------------------------------- */

void apc_serialize_zend_function_entry(zend_function_entry* zfe TSRMLS_DC)
{
#ifdef ZEND_ENGINE_2
	int i;
#endif
#if PHP_MAJOR_VERSION >= 6
	int len;

	if (zfe->fname == NULL) len = -1;
	else if (UG(unicode)) len = u_strlen((UChar*)zfe->fname) * sizeof(UChar);
	else len = strlen(zfe->fname);
	apc_serialize_zstring((char*)zfe->fname, len TSRMLS_CC);
#else
	apc_serialize_string((char*)zfe->fname TSRMLS_CC);
#endif
	BCOMPILER_DEBUG(("serializing function %s\n",zfe->fname ));
	/** old code: SERIALIZE_SCALAR(zfe->handler, void*); */
	if (BCOMPILERG(current_write) < 0x0005) SERIALIZE_SCALAR(0, ulong); /* dummy 4 bytes */
#ifdef ZEND_ENGINE_2
	SERIALIZE_SCALAR(zfe->num_args, int);
	for (i=0; i< zfe->num_args; i++) {
		apc_serialize_arg_info((zend_arg_info *)&zfe->arg_info[i] TSRMLS_CC);
	}
#else
	apc_serialize_arg_types(zfe->func_arg_types TSRMLS_CC);
#endif
}

void apc_deserialize_zend_function_entry(zend_function_entry* zfe TSRMLS_DC)
{
#ifdef ZEND_ENGINE_2
	int i;
#endif
	apc_create_string_u((char**)&zfe->fname, -1 TSRMLS_CC);
	/** old code: DESERIALIZE_SCALAR(&zfe->handler, void*); */
	if (BCOMPILERG(current_version) < 0x0005) DESERIALIZE_VOID(ulong); /* dummy 4 bytes */
	zfe->handler = NULL;
	BCOMPILER_DEBUG(("deserializing function %s\n",zfe->handler));
#ifdef ZEND_ENGINE_2
	DESERIALIZE_SCALAR(&zfe->num_args, int);
	zfe->arg_info = (zend_arg_info *) ecalloc(zfe->num_args, sizeof(zend_arg_info));
	for (i=0; i< zfe->num_args; i++) {
		apc_create_arg_info((zend_arg_info*)&zfe->arg_info[i] TSRMLS_CC);
	}
#else
	apc_create_arg_types(&zfe->func_arg_types TSRMLS_CC);
#endif
}


#ifdef ZEND_ENGINE_2
/* --- zend_property_info -------------------------------------------------- */

void apc_serialize_zend_property_info(zend_property_info* zpr TSRMLS_DC)
{
	SERIALIZE_SCALAR(zpr->flags, zend_uint);
#if PHP_MAJOR_VERSION >= 6
	apc_serialize_zstring(ZS2S(zpr->name), ZLEN(zpr->name_length) TSRMLS_CC);
#else
	apc_serialize_zstring(zpr->name, zpr->name_length TSRMLS_CC);
#endif
	SERIALIZE_SCALAR(zpr->name_length, uint);
	SERIALIZE_SCALAR(zpr->h, ulong);
}

void apc_deserialize_zend_property_info(zend_property_info* zpr TSRMLS_DC)
{
	DESERIALIZE_SCALAR(&zpr->flags, zend_uint);
	apc_create_string_u(ZS2SP(zpr->name), -1 TSRMLS_CC);
	DESERIALIZE_SCALAR(&zpr->name_length, uint);
	DESERIALIZE_SCALAR(&zpr->h, ulong);
#ifdef ZEND_ENGINE_2_2
	zpr->ce = BCOMPILERG(cur_zc);
#endif

}

void apc_create_zend_property_info(zend_property_info** zf TSRMLS_DC)
{
	*zf = (zend_property_info*) ecalloc(1, sizeof(zend_property_info));
	apc_deserialize_zend_property_info(*zf TSRMLS_CC);
}

void apc_free_zend_property_info(zend_property_info** zf TSRMLS_DC)
{
	if (*zf != NULL) {
		efree(*zf);
	}
	*zf = NULL;
}

#else /* zend_property_info */
/* --- zend_property_reference --------------------------------------------- */

void apc_serialize_zend_property_reference(zend_property_reference* zpr TSRMLS_DC)
{
	SERIALIZE_SCALAR(zpr->type, int);
	apc_serialize_zval(zpr->object, NULL TSRMLS_CC);
	apc_serialize_zend_llist(zpr->elements_list TSRMLS_CC);
}

void apc_deserialize_zend_property_reference(zend_property_reference* zpr TSRMLS_DC)
{
	DESERIALIZE_SCALAR(&zpr->type, int);
	apc_deserialize_zval(zpr->object, NULL TSRMLS_CC);
	apc_create_zend_llist(&zpr->elements_list TSRMLS_CC);
}
#endif /* zend_property_reference */



#ifndef ZEND_ENGINE_2
/* --- zend_overloaded_element --------------------------------------------- */

void apc_serialize_zend_overloaded_element(zend_overloaded_element* zoe TSRMLS_DC)
{
	SERIALIZE_SCALAR(zoe->type, zend_uchar);
	apc_serialize_zval(&zoe->element, NULL TSRMLS_CC);
}

void apc_deserialize_zend_overloaded_element(zend_overloaded_element* zoe TSRMLS_DC)
{
	DESERIALIZE_SCALAR(&zoe->type, zend_uchar);
	apc_deserialize_zval(&zoe->element, NULL TSRMLS_CC);
}
#endif


/* --- zend_class_entry ---------------------------------------------------- */

void apc_serialize_zend_class_entry(zend_class_entry* zce , char* force_parent_name, int force_parent_len, char* force_key, int force_key_len TSRMLS_DC)
{
	zend_function_entry* zfe;
	int count, i;

	BCOMPILER_DEBUG(("adding type : %i\n",zce->type ));
	SERIALIZE_SCALAR(zce->type, char);
#if PHP_MAJOR_VERSION >= 6
	apc_serialize_zstring(ZS2S(zce->name), ZLEN(zce->name_length) TSRMLS_CC);
#else
	apc_serialize_zstring(zce->name, zce->name_length TSRMLS_CC);
#endif
	SERIALIZE_SCALAR(zce->name_length, uint);

	/* Serialize the name of this class's parent class (if it has one)
	 * so that we can perform inheritance during deserialization (see
	 * apc_deserialize_zend_class_entry). */

	if (zce->parent != NULL) {
		/*Parent*/
		SERIALIZE_SCALAR(1, char);
#if PHP_MAJOR_VERSION >= 6
		apc_serialize_zstring(ZS2S(zce->parent->name), ZLEN(zce->parent->name_length) TSRMLS_CC);
#else
		apc_serialize_zstring(zce->parent->name, zce->parent->name_length TSRMLS_CC);
#endif
	} else if (force_parent_len > 0) {
		SERIALIZE_SCALAR(1, char);
		apc_serialize_zstring(force_parent_name, force_parent_len TSRMLS_CC);
	} else {
		SERIALIZE_SCALAR(0, char);
	}

#ifdef ZEND_ENGINE_2
	SERIALIZE_SCALAR(zce->refcount  	     , int);
#else
	SERIALIZE_SCALAR(*(zce->refcount)  	     , int);
#endif
	//SERIALIZE_SCALAR(zce->constants_updated, zend_bool);
#ifdef ZEND_ENGINE_2
	SERIALIZE_SCALAR(zce->ce_flags, 	 zend_uint);
#endif

	BCOMPILER_DEBUG(("-----------------------------\nFUNC TABLE:\n"));
	BCOMPILERG(cur_zc) = zce;
	apc_serialize_hashtable(&zce->function_table, apc_serialize_zend_function TSRMLS_CC);
	BCOMPILERG(cur_zc) = NULL;

#ifndef ZEND_ENGINE_2_4 /* todo */
	BCOMPILER_DEBUG(("-----------------------------\nVARS:\n"));
	apc_serialize_hashtable(&zce->default_properties, apc_serialize_zval_ptr TSRMLS_CC);
#endif

#ifdef ZEND_ENGINE_2
	BCOMPILER_DEBUG(("-----------------------------\nPROP INFO:\n"));
	apc_serialize_hashtable(&zce->properties_info, apc_serialize_zend_property_info TSRMLS_CC);
# ifndef ZEND_ENGINE_2_4 /* todo */
#  ifdef ZEND_ENGINE_2_1
	/* new for 0.12 */
	if (BCOMPILERG(current_write) >= 0x000c) {
		BCOMPILER_DEBUG(("-----------------------------\nDEFAULT STATIC MEMBERS:\n"));
		apc_serialize_hashtable(&zce->default_static_members, apc_serialize_zval_ptr TSRMLS_CC);
	}
#  endif
# endif

# ifndef ZEND_ENGINE_2_4 /* todo */
	/* not sure if statics should be dumped... (val: surely not for ZE v2.1) */
	BCOMPILER_DEBUG(("-----------------------------\nSTATICS?:\n"));
#  ifdef ZEND_ENGINE_2_1
	apc_serialize_hashtable(NULL, apc_serialize_zval_ptr TSRMLS_CC);
#  else
	apc_serialize_hashtable(zce->static_members, apc_serialize_zval_ptr TSRMLS_CC);
#  endif
# endif

	BCOMPILER_DEBUG(("-----------------------------\nCONSTANTS?:\n"));
	apc_serialize_hashtable(&zce->constants_table, apc_serialize_zval_ptr TSRMLS_CC);
#endif
	/* zend_class_entry.builtin_functions: this array appears to be
	 * terminated by an element where zend_function_entry.fname is null
	 * First we count the number of elements, then we serialize that count
	 * and all the functions. */
	BCOMPILER_DEBUG(("-----------------------------\nBUILTIN:\n"));
	count = 0;
	if (zce->type == ZEND_INTERNAL_CLASS && zce->info.internal.builtin_functions) {
		for (zfe = (zend_function_entry*)zce->info.internal.builtin_functions; zfe->fname != NULL; zfe++) {
			count++;
		}
	}

	SERIALIZE_SCALAR(count, int);
	for (i = 0; i < count; i++) {
		apc_serialize_zend_function_entry((zend_function_entry *)&zce->info.internal.builtin_functions[i] TSRMLS_CC);
	}
#ifndef ZEND_ENGINE_2
	/** old code: SERIALIZE_SCALAR(zce->handle_function_call, void*); */
	/** old code: SERIALIZE_SCALAR(zce->handle_property_get, void*); */
	/** old code: SERIALIZE_SCALAR(zce->handle_property_set, void*); */
	if (BCOMPILERG(current_write) < 0x0005) {
		SERIALIZE_SCALAR(0, ulong); /* zce->handle_function_call */
		SERIALIZE_SCALAR(0, ulong); /* zce->handle_property_get */
		SERIALIZE_SCALAR(0, ulong); /* zce->handle_property_set */
	}
#endif

	/* serialize interfaces */
#ifdef ZEND_ENGINE_2
	if (BCOMPILERG(current_write) >= 0x0005) {
		SERIALIZE_SCALAR(zce->num_interfaces, zend_uint);
		if (BCOMPILERG(current_write) >= 25) {
			for (i = 0; i < zce->num_interfaces; i++) {
				if (!zce->interfaces[i]) {
					apc_serialize_string(NULL TSRMLS_CC);
					continue;
				}
#if PHP_MAJOR_VERSION >= 6
				apc_serialize_zstring(ZS2S(zce->interfaces[i]->name), ZLEN(zce->interfaces[i]->name_length) TSRMLS_CC);
#else
				apc_serialize_zstring(zce->interfaces[i]->name, zce->interfaces[i]->name_length TSRMLS_CC);
#endif
			}
		}
	}
#endif

	/* do reflection info (file/start/end) !! TODO: check for PHP6 !! */
#ifdef ZEND_ENGINE_2
	if (BCOMPILERG(current_write) >= 25) {
		char* fname = bcompiler_handle_filename(zce->info.user.filename TSRMLS_CC);
		apc_serialize_string(fname TSRMLS_CC);
		SERIALIZE_SCALAR(zce->info.user.line_start, int);
		SERIALIZE_SCALAR(zce->info.user.line_end, int);
		apc_serialize_zstring(zce->info.user.doc_comment, zce->info.user.doc_comment_len TSRMLS_CC);
		SERIALIZE_SCALAR(zce->info.user.doc_comment_len, int);
	}
#endif

	/* val: new!! add hash key for the class if set, 0 otherwise */
	if (BCOMPILERG(current_write) >= 0x0005) {
		if (force_key && force_key_len) {
			char tmp = force_key_len > 0x7f ? 1 : force_key_len;
#if PHP_MAJOR_VERSION >= 6
			if (force_key_len < 0) {
				if (force_key_len < -0x7f) tmp = -1;
				SERIALIZE_SCALAR(-tmp, char);
				apc_serialize_zstring(force_key, UBYTES(-force_key_len) TSRMLS_CC);
			} else {
#endif
			SERIALIZE_SCALAR(tmp, char);
			apc_serialize_zstring(force_key, force_key_len TSRMLS_CC);
#if PHP_MAJOR_VERSION >= 6
			}
#endif
		}
		else SERIALIZE_SCALAR(0, char);
	}
}

#ifdef ZEND_ENGINE_2
static void zend_destroy_property_info(zend_property_info *property_info)
{
    efree(ZS2S(property_info->name));
}
#endif

#ifndef ZEND_ENGINE_2
/* copy of zend_do_inheritance */
void bcompiler_do_inheritance(zend_class_entry *ce, zend_class_entry *parent_ce)
{
	zend_function tmp_zend_function;
	zval *tmp;

	/* Perform inheritance */
	zend_hash_merge(&ce->default_properties, &parent_ce->default_properties, (void (*)(void *)) zval_add_ref, (void *) &tmp, sizeof(zval *), 0);
	zend_hash_merge(&ce->function_table, &parent_ce->function_table, (void (*)(void *)) function_add_ref, &tmp_zend_function, sizeof(zend_function), 0);
	ce->parent = parent_ce;
	if (!ce->handle_property_get)
	   ce->handle_property_get      = parent_ce->handle_property_get;
	if (!ce->handle_property_set)
		ce->handle_property_set = parent_ce->handle_property_set;
	if (!ce->handle_function_call)
		ce->handle_function_call = parent_ce->handle_function_call;
	/* leave this out for the time being? - inherit constructor.. */
	/* do_inherit_parent_constructor(ce); */

}
#endif

void apc_deserialize_zend_class_entry(zend_class_entry* zce, char **key, int *key_len TSRMLS_DC)
{

	int count, i, exists;
	char* parent_name = NULL;  /* name of parent class */
#ifdef ZEND_ENGINE_2
	char* interface_name = NULL;
	zend_class_entry **pce;
#endif

	DESERIALIZE_SCALAR(&zce->type, char);
	apc_create_string_u(ZS2SP(zce->name), -1 TSRMLS_CC);
	DESERIALIZE_SCALAR(&zce->name_length, uint);

	zce->parent = NULL;
	DESERIALIZE_SCALAR(&exists, char);
	if (exists > 0) {
/*
		// This class knows the name of its parent, most likely because
		// its parent is defined in the same source file. Therefore, we
		// can simply restore the parent link, and not worry about
		// manually inheriting methods from the parent (PHP will perform
		// the inheritance).
*/

		apc_create_string_u(&parent_name, -1 TSRMLS_CC);
		BCOMPILER_DEBUG(("PARENT %s\n",parent_name));
#if PHP_MAJOR_VERSION >= 6
		if (zend_u_lookup_class(BCOMPILERG(is_unicode) ? IS_UNICODE : IS_STRING, ZSTR(parent_name), BCOMPILERG(is_unicode) ? u_strlen((UChar*)parent_name) : strlen(parent_name), &pce TSRMLS_CC) == FAILURE)
#elif defined(ZEND_ENGINE_2)
		if (zend_lookup_class(parent_name, strlen(parent_name), &pce TSRMLS_CC) == FAILURE)
#else
		if ((zend_hash_find(CG(class_table), parent_name,
			strlen(parent_name) + 1, (void**) &zce->parent) == FAILURE))
#endif
		{
			php_error(E_ERROR, "unable to inherit %s", parent_name);
		}
#ifdef ZEND_ENGINE_2
		else zce->parent = *pce; /* *((zend_class_entry**)zce->parent); */
#endif
	}

#ifdef ZEND_ENGINE_2
	DESERIALIZE_SCALAR(&zce->refcount, int);
	if (BCOMPILERG(current_include)) {
		zce->refcount = 1; /* val: to avoid memort leaks */
	}
#else
	zce->refcount = (int*) emalloc(sizeof(int));
	DESERIALIZE_SCALAR(zce->refcount, int);
	if (BCOMPILERG(current_include)) {
		*zce->refcount = 1; /* val: to avoid memort leaks */
	}
#endif
	//DESERIALIZE_SCALAR(&zce->constants_updated, zend_bool);
#ifdef ZEND_ENGINE_2
	DESERIALIZE_SCALAR(&zce->ce_flags, zend_uint);
#endif
	BCOMPILERG(cur_zc) = zce;
	BCOMPILER_DEBUG(("-----------------------------\nFUNC TABLE:\n"));
	DESERIALIZE_SCALAR(&exists, char);
	apc_deserialize_hashtable(
		&zce->function_table,
		(void*) apc_create_zend_function,
		(void*) apc_free_zend_function,
		(int) sizeof(zend_function) ,
		(char) exists TSRMLS_CC);
	BCOMPILERG(cur_zc) = NULL;

#ifndef ZEND_ENGINE_2_4 /* todo */
	BCOMPILER_DEBUG(("-----------------------------\nVARS:\n"));
	DESERIALIZE_SCALAR(&exists, char);
	apc_deserialize_hashtable(
		&zce->default_properties,
		(void*) apc_create_zval,
		(void*) NULL,
		(int) sizeof(zval *),
		(char)exists TSRMLS_CC);
#endif

#ifdef ZEND_ENGINE_2
	BCOMPILERG(cur_zc) = zce;
	BCOMPILER_DEBUG(("-----------------------------\nPROP INFO:\n"));
	DESERIALIZE_SCALAR(&exists, char);
	apc_deserialize_hashtable(
		&zce->properties_info,
		(void*) apc_create_zend_property_info,
		(void*) apc_free_zend_property_info,
		(int) sizeof(zend_property_info) ,
		(char) exists TSRMLS_CC);
	zce->properties_info.pDestructor = (dtor_func_t)&zend_destroy_property_info;
	BCOMPILERG(cur_zc) = NULL;

# ifndef ZEND_ENGINE_2_4 /* todo */
#  ifdef ZEND_ENGINE_2_1
	if (BCOMPILERG(current_version) >= 0x000c) {
		BCOMPILER_DEBUG(("-----------------------------\nDEFAULT STATIC MEMBERS:\n"));
		DESERIALIZE_SCALAR(&exists, char);
		apc_deserialize_hashtable(
			&zce->default_static_members,
			(void*) apc_create_zval,
			(void*) NULL,
			(int) sizeof(zval *),
			(char)exists TSRMLS_CC);
	}
#  endif
# endif

# ifndef ZEND_ENGINE_2_4 /* todo */
	BCOMPILER_DEBUG(("-----------------------------\nSTATICS?:\n"));
	DESERIALIZE_SCALAR(&exists, char);
	if (exists) {
		ALLOC_HASHTABLE(zce->static_members);
		apc_deserialize_hashtable(
			zce->static_members,
			(void*) apc_create_zval,
			(void*) NULL,
			(int) sizeof(zval *),
			(char)exists TSRMLS_CC);

#  ifdef ZEND_ENGINE_2_1
		/* seems that this hash is not used any more */
		zend_hash_destroy(zce->static_members);
		FREE_HASHTABLE(zce->static_members);
		zce->static_members = &zce->default_static_members;
#  endif

	}
	else
#  ifdef ZEND_ENGINE_2_1
		zce->static_members = &zce->default_static_members;
#  else
		zce->static_members = NULL;
#  endif
# endif

	BCOMPILER_DEBUG(("-----------------------------\nCONSTANTS:\n"));
	DESERIALIZE_SCALAR(&exists, char);
	apc_deserialize_hashtable(
		&zce->constants_table,
		(void*) apc_create_zval,
		(void*) NULL,
		(int) sizeof(zval *),
		(char)exists TSRMLS_CC);
#endif
	/* see apc_serialize_zend_class_entry() for a description of the
	   zend_class_entry.builtin_functions member */

	DESERIALIZE_SCALAR(&count, int);
	zce->info.internal.builtin_functions = NULL;
	if (count > 0) {
		zce->info.internal.builtin_functions = (zend_function_entry*) ecalloc(count+1, sizeof(zend_function_entry));
		for (i = 0; i < count; i++) {
			apc_deserialize_zend_function_entry((zend_function_entry*)&zce->info.internal.builtin_functions[i] TSRMLS_CC);
		}
	}
#ifndef ZEND_ENGINE_2
	/** old code: DESERIALIZE_SCALAR(&zce->handle_function_call, void*); */
	/** old code: DESERIALIZE_SCALAR(&zce->handle_property_get, void*); */
	/** old code: DESERIALIZE_SCALAR(&zce->handle_property_set, void*); */
	if (BCOMPILERG(current_version) < 0x0005) {
		DESERIALIZE_VOID(ulong); /* &zce->handle_function_call */
		DESERIALIZE_VOID(ulong); /* &zce->handle_property_get */
		DESERIALIZE_VOID(ulong); /* &zce->handle_property_set */
	}
	zce->handle_function_call = NULL;
	zce->handle_property_get = NULL;
	zce->handle_property_set = NULL;
#endif

	/* deserialize interfaces */
#ifdef ZEND_ENGINE_2
	if (BCOMPILERG(current_version) >= 0x0005) {
		DESERIALIZE_SCALAR(&zce->num_interfaces, zend_uint);
		if (zce->num_interfaces) {
			zce->interfaces = ecalloc(zce->num_interfaces, sizeof(*zce->interfaces));
		}
		if (BCOMPILERG(current_version) >= 25) {
			for (i = 0; i < zce->num_interfaces; i++) {
				apc_create_string_u(&interface_name, -1 TSRMLS_CC);
				if (!interface_name || !*interface_name) continue;
				BCOMPILER_DEBUG(("INTERFACE %s\n", interface_name));
#if PHP_MAJOR_VERSION >= 6
				if (zend_u_lookup_class(BCOMPILERG(is_unicode) ? IS_UNICODE : IS_STRING, ZSTR(interface_name), BCOMPILERG(is_unicode) ? u_strlen((UChar*)interface_name) : strlen(interface_name), &pce TSRMLS_CC) == FAILURE)
#else
				if (zend_lookup_class(interface_name, strlen(interface_name), &pce TSRMLS_CC) == FAILURE)
#endif
				{
					php_error(E_ERROR, "unable to inherit %s", interface_name);
				}
				else zce->interfaces[i] = *pce;
				efree(interface_name);
			}
		}
	}
#endif

	/* deserialize reflection info */
#ifdef ZEND_ENGINE_2
	if (BCOMPILERG(current_version) >= 25) {
		char* fname;
		apc_create_string_u(&fname, -1 TSRMLS_CC);
		zce->info.user.filename = fname ? fname : estrdup(BCOMPILERG(current_filename));
		DESERIALIZE_SCALAR(&zce->info.user.line_start, int);
		DESERIALIZE_SCALAR(&zce->info.user.line_end, int);
		apc_create_string_u(ZS2SP(zce->info.user.doc_comment), -1 TSRMLS_CC);
		DESERIALIZE_SCALAR(&zce->info.user.doc_comment_len, int);
	}
#endif

	/* val: new!! restore key in class hash */
	if (key_len) *key_len = 0;
	if (key) *key = NULL;
	if (BCOMPILERG(current_version) >= 0x0005) {
		DESERIALIZE_SCALAR(&exists, char);
		if (exists && key_len && key) {
			*key_len = apc_create_string_u(key, -1 TSRMLS_CC);
		}
	}

	/* Resolve the inheritance relationships that have been delayed and
	 * are waiting for this class to be created, i.e., every child of
	 * this class that has already been compiled needs to be inherited
	 * from this class. Call inherit() with this class as the base class
	 * (first parameter) and as the current parent class (second parameter).
	 * inherit will recursively resolve every inheritance relationships
	 * in which this class is the base. */

	  /*
	  Since we now can do inheritance by striping - this is usefull..

	  */
	if (zce->parent) {
#ifdef ZEND_ENGINE_2
		zend_do_inheritance(zce, zce->parent TSRMLS_CC);
#else
		bcompiler_do_inheritance(zce, zce->parent);
#endif
	}
#if VAL_0 /* val: all PHP versions work fine without it */
	if (zce->parent) {
		 bcompiler_inherit(zce TSRMLS_CC);
	}
#endif
	if (parent_name) {
		efree(parent_name);
	}
}

void apc_create_zend_class_entry(zend_class_entry** zce, char** key, int* key_len TSRMLS_DC)
{

	*zce = (zend_class_entry*) ecalloc(1, sizeof(zend_class_entry));
	apc_deserialize_zend_class_entry(*zce, key, key_len TSRMLS_CC);

}


/* --- znode --------------------------------------------------------------- */

void apc_serialize_znode(znode* zn TSRMLS_DC)
{
	SERIALIZE_SCALAR(zn->op_type, int);

	/* If the znode's op_type is IS_CONST, we know precisely what it is.
	 * otherwise, it is too case-dependent (or inscrutable), so we do
	 * a bitwise copy. */

	switch(zn->op_type) {
		case IS_CONST:
			apc_serialize_zval(&zn->u.constant, zn TSRMLS_CC);
			break;

		case IS_VAR:
		case IS_TMP_VAR:
		case IS_UNUSED:
		default:
			if (BCOMPILERG(current_write) >= 0x0005) {
				/* make zn->u's size-safe */
#ifdef ZEND_ENGINE_2_4
				SERIALIZE_SCALAR(zn->EA, zend_uint);
#else
				SERIALIZE_SCALAR(zn->u.EA.var,  zend_uint);
				SERIALIZE_SCALAR(zn->u.EA.type, zend_uint);
#endif
			} else {
#ifndef ZEND_ENGINE_2_4
				STORE_BYTES(&zn->u, sizeof(zn->u));
#endif
			}
			break;
	}
}

void apc_deserialize_znode(znode* zn TSRMLS_DC)
{
	DESERIALIZE_SCALAR(&zn->op_type, int);

	/* If the znode's op_type is IS_CONST, we know precisely what it is.
	 * otherwise, it is too case-dependent (or inscrutable), so we do
	 * a bitwise copy. */

	switch(zn->op_type)
	{
	case IS_CONST:
		apc_deserialize_zval(&zn->u.constant, zn TSRMLS_CC);
		break;

	default:
		/* make zn->u's size-safe */
		if (BCOMPILERG(current_version) >= 0x0005) {
#ifdef ZEND_ENGINE_2_4
			DESERIALIZE_SCALAR(&zn->EA, zend_uint);
#else
			DESERIALIZE_SCALAR(&zn->u.EA.var,  zend_uint);
			DESERIALIZE_SCALAR(&zn->u.EA.type, zend_uint);
#endif
		} else {
#ifndef ZEND_ENGINE_2_4
			LOAD_BYTES(&zn->u, sizeof(zn->u));
#endif
		}
		break;
	}
}


#ifdef ZEND_ENGINE_2_4
/* --- znode_op ------------------------------------------------------------ */

void bc_serialize_znode_op(zend_uchar type, znode_op op, zend_op_array *zoa TSRMLS_DC)
{
	SERIALIZE_SCALAR(type, zend_uchar);
	if (type != IS_CONST) { SERIALIZE_SCALAR(op.var, zend_uint); }
	else {
		zend_uint i;
		for (i = 0; i < zoa->last_literal; i++) {
			if (&zoa->literals[i].constant == op.zv) break;
		}
		assert(i < zoa->last_literal);
		SERIALIZE_SCALAR(i, zend_uint);
	}
}

void bc_deserialize_znode_op(zend_uchar *type, znode_op *op, zend_op_array *zoa TSRMLS_DC)
{
	DESERIALIZE_SCALAR(type, zend_uchar);
	DESERIALIZE_SCALAR(& (*op).var, zend_uint);
	if (*type == IS_CONST) {
		assert(op->constant < zoa->last_literal);
		op->zv = &zoa->literals[op->constant].constant;
	}
}
#endif


/* --- zend_op ------------------------------------------------------------- */

void apc_serialize_zend_op(int id, zend_op* zo, zend_op_array* zoa TSRMLS_DC)
{

	BCOMPILER_DEBUG(("Outputting %s\n", bcompiler_opcode_name(zo->opcode)));
	SERIALIZE_SCALAR(zo->opcode, zend_uchar);

#ifdef ZEND_ENGINE_2_4
	bc_serialize_znode_op(zo->result_type, zo->result, zoa TSRMLS_CC);
#else
	apc_serialize_znode(&zo->result TSRMLS_CC);
#endif

#ifdef ZEND_ENGINE_2
	/* convert raw jump address to opline number */
	switch (zo->opcode)
	{
		case ZEND_JMP:
#ifdef ZEND_GOTO
		case ZEND_GOTO:
#endif
#ifdef ZEND_ENGINE_2_4
			zo->op1.opline_num = zo->op1.jmp_addr - zoa->opcodes;
#else
			zo->op1.u.opline_num = zo->op1.u.jmp_addr - zoa->opcodes;
#endif
			break;
		case ZEND_JMPZ:
		case ZEND_JMPNZ:
		case ZEND_JMPZ_EX:
		case ZEND_JMPNZ_EX:
#ifdef ZEND_JMP_SET
		case ZEND_JMP_SET:
#endif
#ifdef ZEND_ENGINE_2_4
			zo->op2.opline_num = zo->op2.jmp_addr - zoa->opcodes;
#else
			zo->op2.u.opline_num = zo->op2.u.jmp_addr - zoa->opcodes;
#endif
			break;
	}
#endif

#ifdef ZEND_ENGINE_2_4
	bc_serialize_znode_op(zo->op1_type, zo->op1, zoa TSRMLS_CC);
	bc_serialize_znode_op(zo->op2_type, zo->op2, zoa TSRMLS_CC);
#else
	apc_serialize_znode(&zo->op1 TSRMLS_CC);
	apc_serialize_znode(&zo->op2 TSRMLS_CC);
#endif

	SERIALIZE_SCALAR(zo->extended_value, ulong);
	if (zo->extended_value != 0) {

	}

	SERIALIZE_SCALAR(zo->lineno, uint);
}

void apc_deserialize_zend_op(zend_op* zo, zend_op_array* zoa TSRMLS_DC)
{

	DESERIALIZE_SCALAR(&zo->opcode, zend_uchar);
	BCOMPILER_DEBUG(("Outputting %s\n", bcompiler_opcode_name(zo->opcode)));

#ifdef ZEND_ENGINE_2_4
	bc_deserialize_znode_op(&zo->result_type, &zo->result, zoa TSRMLS_CC);
	bc_deserialize_znode_op(&zo->op1_type, &zo->op1, zoa TSRMLS_CC);
	bc_deserialize_znode_op(&zo->op2_type, &zo->op2, zoa TSRMLS_CC);
#else
	apc_deserialize_znode(&zo->result TSRMLS_CC);
	apc_deserialize_znode(&zo->op1 TSRMLS_CC);
	apc_deserialize_znode(&zo->op2 TSRMLS_CC);
#endif

#if defined(ZEND_ENGINE_2_1)
    ZEND_VM_SET_OPCODE_HANDLER(zo);
#elif defined(ZEND_ENGINE_2)
	zo->handler = zend_opcode_handlers[zo->opcode];
#endif
#ifdef ZEND_ENGINE_2
	/* convert opline number back to raw jump address */
	switch (zo->opcode)
	{
		case ZEND_JMP:
#ifdef ZEND_GOTO
		case ZEND_GOTO:
#endif
#ifdef ZEND_ENGINE_2_4
			zo->op1.jmp_addr = zoa->opcodes + zo->op1.opline_num;
#else
			zo->op1.u.jmp_addr = zoa->opcodes + zo->op1.u.opline_num;
#endif
			break;
		case ZEND_JMPZ:
		case ZEND_JMPNZ:
		case ZEND_JMPZ_EX:
		case ZEND_JMPNZ_EX:
#ifdef ZEND_JMP_SET
		case ZEND_JMP_SET:
#endif
#ifdef ZEND_ENGINE_2_4
			zo->op2.jmp_addr = zoa->opcodes + zo->op2.opline_num;
#else
			zo->op2.u.jmp_addr = zoa->opcodes + zo->op2.u.opline_num;
#endif
			break;
	}
#endif
	DESERIALIZE_SCALAR(&zo->extended_value, ulong);
	DESERIALIZE_SCALAR(&zo->lineno, uint);
}


#ifdef ZEND_ENGINE_2_4
/* --- zend_literal -------------------------------------------------------- */

void bc_serialize_zend_literal(zend_literal *zl TSRMLS_DC)
{
	apc_serialize_zval(&zl->constant, NULL TSRMLS_CC);
	SERIALIZE_SCALAR(zl->hash_value, zend_ulong);
	SERIALIZE_SCALAR(zl->cache_slot, zend_uint);
}

void bc_deserialize_zend_literal(zend_literal *zl TSRMLS_DC)
{
	apc_deserialize_zval(& (zl->constant), NULL TSRMLS_CC);
	DESERIALIZE_SCALAR(& (zl->hash_value), zend_ulong);
	DESERIALIZE_SCALAR(& (zl->cache_slot), zend_uint);
}

void bc_create_zend_literal(zend_literal **zl TSRMLS_DC)
{
	*zl = (zend_literal*) ecalloc(1, sizeof(zend_literal));
	bc_deserialize_zend_literal(*zl TSRMLS_CC);
}
#endif


/* --- zend_op_array ------------------------------------------------------- */

void apc_serialize_zend_op_array(zend_op_array* zoa TSRMLS_DC)
{
	char exists;
	char *fname;
	int i;
#if PHP_MAJOR_VERSION >= 6
	int len;
#endif

	BCOMPILERG(zoa) = zoa;
	SERIALIZE_SCALAR(zoa->type, zend_uchar);
#ifdef ZEND_ENGINE_2
	SERIALIZE_SCALAR(zoa->num_args, int);
	for (i=0; i< zoa->num_args; i++) {
		apc_serialize_arg_info(&zoa->arg_info[i] TSRMLS_CC);
	}
#else
	apc_serialize_arg_types(zoa->arg_types TSRMLS_CC);
#endif

#if PHP_MAJOR_VERSION >= 6
	if (zoa->function_name.s == NULL) len = -1;
	else if (UG(unicode)) len = u_strlen(zoa->function_name.u) * sizeof(UChar);
	else len = strlen(zoa->function_name.s);
	apc_serialize_zstring(zoa->function_name.s, len TSRMLS_CC);
#else
	apc_serialize_string(zoa->function_name TSRMLS_CC);
#endif

#ifdef ZEND_ENGINE_2_4
	/*SERIALIZE_SCALAR(zoa->last_literal, zend_uint);
	SERIALIZE_SCALAR(zoa->size_literal, zend_uint);
	for (i = 0; i < (int) zoa->last_literal; i++) {
		bc_serialize_zend_literal(&zoa->literals[i] TSRMLS_CC);
	}*/
#endif

	SERIALIZE_SCALAR(zoa->refcount[0], zend_uint);
	SERIALIZE_SCALAR(zoa->last, zend_uint);
	//SERIALIZE_SCALAR(zoa->size, zend_uint);

	/* If a file 'A' is included twice in a single request, the following
	 * situation can occur: A is deserialized and its functions added to
	 * the global function table. On its next call, A is expired (either
	 * forcibly removed or removed due to an expired ttl). Now when A is
	 * compiled, its functions can't be added to the global function_table
	 * (they are already present) so they are serialized as an opcode
	 * ZEND_DECLARE_FUNCTION_OR_CLASS. This means that the functions will
	 * be declared at execution time. Since they are present in the global
	 * function_table, they will will also be serialized. This will cause
	 * a fatal 'failed to redclare....' error.  We avoid this by simulating
	 * the action of the parser and changing all
	 * ZEND_DECLARE_FUNCTION_OR_CLASS opcodes to ZEND_NOPs. */


	for (i = 0; i < (int) zoa->last; i++) {

		apc_serialize_zend_op(i, &zoa->opcodes[i], zoa TSRMLS_CC);
	}

	SERIALIZE_SCALAR(zoa->T, zend_uint);
	//SERIALIZE_SCALAR(zoa->last_brk_cont, zend_uint);
	//SERIALIZE_SCALAR(zoa->current_brk_cont, zend_uint);
#ifndef ZEND_ENGINE_2
	SERIALIZE_SCALAR(zoa->uses_globals, zend_bool);
#endif

	exists = (zoa->brk_cont_array != NULL) ? 1 : 0;
	SERIALIZE_SCALAR(exists, char);
	if (exists) {
		STORE_BYTES(zoa->brk_cont_array, zoa->last_brk_cont *
			sizeof(zend_brk_cont_element));
	}
	apc_serialize_hashtable(zoa->static_variables, apc_serialize_zval_ptr TSRMLS_CC);
	//assert(zoa->start_op == NULL);
	//SERIALIZE_SCALAR(zoa->return_reference, zend_bool);
	//SERIALIZE_SCALAR(zoa->done_pass_two, zend_bool);
	fname = bcompiler_handle_filename(zoa->filename TSRMLS_CC);
	apc_serialize_string(fname TSRMLS_CC);
	if (fname) efree(fname);

#ifdef ZEND_ENGINE_2
	if (BCOMPILERG(current_write) >= 0x0005) {
#if PHP_MAJOR_VERSION >= 6
		int len;
		if (zoa->scope == NULL) len = -1;
		else if (zoa->scope->name.s == NULL) len = -1;
		else if (UG(unicode)) len = u_strlen(zoa->scope->name.u) * sizeof(UChar);
		else len = strlen(zoa->scope->name.s);
		apc_serialize_zstring(zoa->scope ? zoa->scope->name.s : NULL, len TSRMLS_CC);
#else
		apc_serialize_string(zoa->scope ? zoa->scope->name : NULL TSRMLS_CC);
#endif
		SERIALIZE_SCALAR(zoa->fn_flags, zend_uint);
		SERIALIZE_SCALAR(zoa->required_num_args, zend_uint);
		//SERIALIZE_SCALAR(zoa->pass_rest_by_reference, zend_bool);

		//SERIALIZE_SCALAR(zoa->backpatch_count, int);
#ifdef ZEND_ENGINE_2_3
		SERIALIZE_SCALAR(zoa->this_var, zend_uint);
#else
		SERIALIZE_SCALAR(zoa->uses_this, zend_bool);
#endif
	}
#endif

#ifdef ZEND_ENGINE_2_1
	if (BCOMPILERG(current_write) >= 0x0007) {
		SERIALIZE_SCALAR(zoa->last_var, int);
		//SERIALIZE_SCALAR(zoa->size_var, int);
		for (i = 0; i < zoa->last_var; i++) {
			SERIALIZE_SCALAR(zoa->vars[i].name_len, int);
			apc_serialize_zstring(ZS2S(zoa->vars[i].name), ZLEN(zoa->vars[i].name_len) TSRMLS_CC);
			SERIALIZE_SCALAR(zoa->vars[i].hash_value, ulong);
		}
	}
#endif

#ifdef ZEND_ENGINE_2
	if (BCOMPILERG(current_write) >= 0x000d) {
		if (zoa->try_catch_array != NULL && zoa->last_try_catch > 0) {
			SERIALIZE_SCALAR(zoa->last_try_catch, int);
			STORE_BYTES(zoa->try_catch_array, zoa->last_try_catch * sizeof(zend_try_catch_element));
		} else {
			SERIALIZE_SCALAR(0, int);
		}
	}
#endif

#if PHP_MAJOR_VERSION >= 6
	if (BCOMPILERG(current_write) >= 20) {
		apc_serialize_string(zoa->script_encoding TSRMLS_CC);
	}
#endif

#ifdef ZEND_ENGINE_2_4
	SERIALIZE_SCALAR(zoa->last_cache_slot, int);
#endif

	/* do reflection info (file/start/end) */
#ifdef ZEND_ENGINE_2
	if (BCOMPILERG(current_write) >= 25) {
		SERIALIZE_SCALAR(zoa->line_start, int);
		SERIALIZE_SCALAR(zoa->line_end, int);
		apc_serialize_zstring(zoa->doc_comment, zoa->doc_comment_len TSRMLS_CC);
		SERIALIZE_SCALAR(zoa->doc_comment_len, int);
	}
#endif

	BCOMPILERG(zoa) = NULL;
}

void apc_deserialize_zend_op_array(zend_op_array* zoa, int master TSRMLS_DC)
{
	char *fname;
	char exists;
	int i;

	DESERIALIZE_SCALAR(&zoa->type, zend_uchar);
#ifdef ZEND_ENGINE_2
	DESERIALIZE_SCALAR(&zoa->num_args, int);
	zoa->arg_info = (zend_arg_info *) ecalloc(zoa->num_args, sizeof(zend_arg_info));
	for (i=0; i< zoa->num_args; i++) {
		apc_create_arg_info(&zoa->arg_info[i] TSRMLS_CC);
	}
#else
	apc_create_arg_types(&zoa->arg_types TSRMLS_CC);
#endif

	apc_create_string_u(ZS2SP(zoa->function_name), -1 TSRMLS_CC);

#ifdef ZEND_ENGINE_2_4
	/*DESERIALIZE_SCALAR(&zoa->last_literal, zend_uint);
	DESERIALIZE_SCALAR(&zoa->size_literal, zend_uint);
	zoa->literals = NULL;
	if (zoa->last_literal) {
		zoa->literals = (zend_literal*) ecalloc(zoa->last_literal, sizeof(zend_literal));
		for (i = 0; i < (int) zoa->last_literal; i++) {
			bc_deserialize_zend_literal(&zoa->literals[i] TSRMLS_CC);
		}
	}*/
#endif

	zoa->refcount = (zend_uint *) emalloc(sizeof(zend_uint));
	DESERIALIZE_SCALAR(zoa->refcount, zend_uint);
	if (BCOMPILERG(current_include)) {
		*zoa->refcount = 1; /* val: to avoid memort leaks */
	}
	DESERIALIZE_SCALAR(&zoa->last, zend_uint);
	//DESERIALIZE_SCALAR(&zoa->size, zend_uint);

	zoa->opcodes = NULL;

	if (zoa->last > 0) {
		zoa->opcodes = (zend_op*) ecalloc(zoa->last, sizeof(zend_op));

		for (i = 0; i < (int) zoa->last; i++) {
			apc_deserialize_zend_op(&zoa->opcodes[i], zoa TSRMLS_CC);
		}
	}

	DESERIALIZE_SCALAR(&zoa->T, zend_uint);
	DESERIALIZE_SCALAR(&zoa->last_brk_cont, zend_uint);
	//DESERIALIZE_SCALAR(&zoa->current_brk_cont, zend_uint);
#ifndef ZEND_ENGINE_2
	DESERIALIZE_SCALAR(&zoa->uses_globals, zend_bool);
#endif

	DESERIALIZE_SCALAR(&exists, char);
	zoa->brk_cont_array = NULL;
	if (exists) {
		zoa->brk_cont_array = (zend_brk_cont_element*) ecalloc(zoa->last_brk_cont, sizeof(zend_brk_cont_element));
		LOAD_BYTES(zoa->brk_cont_array, zoa->last_brk_cont * sizeof(zend_brk_cont_element));
	}
	apc_create_hashtable(&zoa->static_variables, apc_create_zval, NULL, sizeof(zval *) TSRMLS_CC);

	//zoa->start_op = NULL;

	//DESERIALIZE_SCALAR(&zoa->return_reference, zend_bool);
	//DESERIALIZE_SCALAR(&zoa->done_pass_two, zend_bool);
	apc_create_string_u(&fname, -1 TSRMLS_CC);
	if (fname) {
		zoa->filename = zend_set_compiled_filename(fname TSRMLS_CC);
		efree(fname);
	}
	else zoa->filename = zend_set_compiled_filename(BCOMPILERG(current_filename) TSRMLS_CC);
#ifdef ZEND_ENGINE_2
	if (BCOMPILERG(current_version) >= 0x0005) {
		apc_create_string_u(&fname, -1 TSRMLS_CC);
		zoa->scope = NULL;
		if (fname && *fname) {
			zend_class_entry **ce = NULL;
#if PHP_MAJOR_VERSION >= 6
			int cmp = BCOMPILERG(is_unicode) ?
						u_strcmp((UChar*)fname, BCOMPILERG(cur_zc)->name.u) :
						strcmp(fname, BCOMPILERG(cur_zc)->name.s);
#else
			int cmp = strcmp(fname, BCOMPILERG(cur_zc)->name);
#endif
			if (BCOMPILERG(cur_zc) && !cmp) {
				ce = &(BCOMPILERG(cur_zc));
			} else {
#if PHP_MAJOR_VERSION >= 6
				if (zend_u_lookup_class_ex(BCOMPILERG(is_unicode) ? IS_UNICODE : IS_STRING, ZSTR(fname), BCOMPILERG(is_unicode) ? u_strlen((UChar*)fname) : strlen(fname), NULL_ZSTR, 1, &ce TSRMLS_CC) != SUCCESS)
#elif defined(ZEND_ENGINE_2_4)
				if (zend_lookup_class_ex(fname, strlen(fname), NULL, 0, &ce TSRMLS_CC) != SUCCESS)
#else
				if (zend_lookup_class_ex(fname, strlen(fname), 0, &ce TSRMLS_CC) != SUCCESS)
#endif
				{
					BCOMPILER_DEBUG(("Could not find class scope: %s\n", fname));
				}
			}

			if (ce && *ce) {
				BCOMPILER_DEBUG(("Found class scope: %s [%p]\n", (*ce)->name, *ce));
				zoa->scope = *ce;
			}
		}
		if (fname) efree(fname);
		DESERIALIZE_SCALAR(&zoa->fn_flags, zend_uint);
#ifdef ZEND_ACC_IMPLEMENTED_ABSTRACT
		zoa->fn_flags &= ~ZEND_ACC_IMPLEMENTED_ABSTRACT; /* this is set later by zend_do_inheritance() */
#endif
		DESERIALIZE_SCALAR(&zoa->required_num_args, zend_uint);
		//DESERIALIZE_SCALAR(&zoa->pass_rest_by_reference, zend_bool);
		//DESERIALIZE_SCALAR(&zoa->backpatch_count, int);
#ifdef ZEND_ENGINE_2_3
		DESERIALIZE_SCALAR(&zoa->this_var, zend_uint);
#else
		DESERIALIZE_SCALAR(&zoa->uses_this, zend_bool);
#endif
	}
#endif

#ifdef ZEND_ENGINE_2_1
	if (BCOMPILERG(current_version) >= 0x0007) {
		DESERIALIZE_SCALAR(&zoa->last_var, int);
		//DESERIALIZE_SCALAR(&zoa->size_var, int);
		/*if (zoa->size_var > 0) {
			zoa->vars = ecalloc(zoa->size_var, sizeof(zoa->vars[0]));
			for (i = 0; i < zoa->last_var; i++) {
				DESERIALIZE_SCALAR(&(zoa->vars[i].name_len), int);
				apc_create_string_u(ZS2SP(zoa->vars[i].name), -1 TSRMLS_CC);
				DESERIALIZE_SCALAR(&(zoa->vars[i].hash_value), ulong);
			}
		}*/
	}
#endif

#ifdef ZEND_ENGINE_2
	if (BCOMPILERG(current_version) >= 0x000d) {
		DESERIALIZE_SCALAR(&zoa->last_try_catch, int);
		if (zoa->last_try_catch > 0) {
			zoa->try_catch_array = (zend_try_catch_element*) ecalloc(zoa->last_try_catch, sizeof(zend_try_catch_element));
			LOAD_BYTES(zoa->try_catch_array, zoa->last_try_catch * sizeof(zend_try_catch_element));
		}
	}
#endif

#if PHP_MAJOR_VERSION >= 6
	if (BCOMPILERG(current_version) >= 20) {
		char *enc;
		apc_create_string_u(&enc, 0 TSRMLS_CC);
		zoa->script_encoding = zend_set_compiled_script_encoding(enc);
		efree(enc);
	}
#endif

#ifdef ZEND_ENGINE_2_4
	DESERIALIZE_SCALAR(&zoa->last_cache_slot, int);
	if (zoa->last_cache_slot) {
		zoa->run_time_cache = ecalloc(zoa->last_cache_slot, sizeof(void*));
	}
#endif

	/* deserialize reflection info */
#ifdef ZEND_ENGINE_2
	if (BCOMPILERG(current_version) >= 25) {
		DESERIALIZE_SCALAR(&zoa->line_start, int);
		DESERIALIZE_SCALAR(&zoa->line_end, int);
		apc_create_string_u(ZS2SP(zoa->doc_comment), -1 TSRMLS_CC);
		DESERIALIZE_SCALAR(&zoa->doc_comment_len, int);
	}
#endif

}

void apc_create_zend_op_array(zend_op_array** zoa TSRMLS_DC)
{
	*zoa = (zend_op_array*) ecalloc(1, sizeof(zend_op_array));
	apc_deserialize_zend_op_array(*zoa, 0 TSRMLS_CC);
}


/* --- zend_function ------------------------------------------------------- */

void apc_serialize_zend_function(zend_function* zf TSRMLS_DC)
{
	/* ZEND_INTERNAL_FUNCTION means function inherited from parent w/o change */
	if (BCOMPILERG(cur_zc) && BCOMPILERG(current_write) >= 0x0005
	    && zf->type == ZEND_INTERNAL_FUNCTION) {
		SERIALIZE_SCALAR(0xff, zend_uchar);
		return;
	}

#if 0 /* ifdef ZEND_ENGINE_2 - val: seems this is not needed anymore */
	/* val: don't include functions of other classes that are bound to this */
	if (BCOMPILERG(cur_zc) && BCOMPILERG(current_write) >= 0x0005) {
# if PHP_MAJOR_VERSION >= 6
		int cmp = BCOMPILERG(is_unicode) ?
						u_strcmp(BCOMPILERG(cur_zc)->name.u, zf->common.scope->name.u) :
						strcmp(BCOMPILERG(cur_zc)->name.s, zf->common.scope->name.s);
# else
		int cmp = strcmp(BCOMPILERG(cur_zc)->name, zf->common.scope->name);
# endif
		BCOMPILER_DEBUGFULL(("current class: %s; scope: %s\n", ZS2S(BCOMPILERG(cur_zc)->name), ZS2S(zf->common.scope->name)));
		if (cmp != 0) {
			SERIALIZE_SCALAR(0xff, zend_uchar);
			return;
		}
	}
#endif

	BCOMPILER_DEBUG(("start serialize zend function type:%d\n", zf->type));
	SERIALIZE_SCALAR(zf->type, zend_uchar);
#ifdef ZEND_ENGINE_2
	if (BCOMPILERG(current_write) >= 0x0005 && BCOMPILERG(cur_zc)) {
		int ftype = 0;
		zend_class_entry *zc = BCOMPILERG(cur_zc);

		if (zf == zc->constructor) {
			BCOMPILER_DEBUG(("> it's a constructor\n"));
			ftype |= 0x01;
	    }
		if (zf == zc->destructor) {
			BCOMPILER_DEBUG(("> it's a destructor\n"));
			ftype |= 0x02;
		}
		if (zf == zc->clone) {
			BCOMPILER_DEBUG(("> it's a clone handler\n"));
			ftype |= 0x04;
		}
		if (zf == zc->__get) {
			BCOMPILER_DEBUG(("> it's a __get handler\n"));
			ftype |= 0x08;
		}
		if (zf == zc->__set) {
			BCOMPILER_DEBUG(("> it's a __set handler\n"));
			ftype |= 0x10;
		}
		if (zf == zc->__call) {
			BCOMPILER_DEBUG(("> it's a __call handler\n"));
			ftype |= 0x20;
		}
#ifdef ZEND_ENGINE_2_1
	if (BCOMPILERG(current_write) >= 0x000a) {
		if (zf == zc->__unset) {
			BCOMPILER_DEBUG(("> it's an __unset handler\n"));
			ftype |= 0x40;
		}
		if (zf == zc->__isset) {
			BCOMPILER_DEBUG(("> it's an __isset handler\n"));
			ftype |= 0x80;
		}
		if (zf == zc->serialize_func) {
			BCOMPILER_DEBUG(("> it's a serialize_func handler\n"));
			ftype |= 0x100;
		}
		if (zf == zc->unserialize_func) {
			BCOMPILER_DEBUG(("> it's a unserialize_func handler\n"));
			ftype |= 0x200;
		}
#ifdef ZEND_ENGINE_2_2
		if (zf == zc->__tostring) {
			BCOMPILER_DEBUG(("> it's a __tostring handler\n"));
			ftype |= 0x400;
		}
#endif
#ifdef ZEND_ENGINE_2_3
		if (zf == zc->__callstatic) {
			BCOMPILER_DEBUG(("> it's a __callstatic handler\n"));
			ftype |= 0x800;
		}
#endif
	}
#endif
		if (BCOMPILERG(current_write) >= 0x000a) {
			SERIALIZE_SCALAR(ftype, int);
		} else {
			SERIALIZE_SCALAR(ftype, char);
		}
	}
#endif
	BCOMPILER_DEBUG(("start serialize zend function\n"));

	switch(zf->type)
	{
		case ZEND_INTERNAL_FUNCTION:
			break;
		case ZEND_USER_FUNCTION:
		case ZEND_EVAL_CODE:
			BCOMPILER_DEBUG(("start serialize op array\n"));
			apc_serialize_zend_op_array(&zf->op_array TSRMLS_CC);
			break;
		default:
			/* the above are all valid zend_function types.  If we hit this
			 * case something has gone very very wrong. */
			assert(0);
	}
}

int apc_deserialize_zend_function(zend_function* zf TSRMLS_DC)
{
	DESERIALIZE_SCALAR_V(&zf->type, zend_uchar, -1);
	/* type 0xFF is reserved not to include a function */
	if (zf->type == 0xff) { return -1; }

#ifdef ZEND_ENGINE_2
	if (BCOMPILERG(current_version) >= 0x0005 && BCOMPILERG(cur_zc)) {
		int ftype;
		zend_class_entry *zc = BCOMPILERG(cur_zc);

		if (BCOMPILERG(current_version) >= 0x000a) {
			DESERIALIZE_SCALAR_V(&ftype, int, -1);
		} else {
			DESERIALIZE_SCALAR_V(&ftype, char, -1);
		}
		if (ftype & 0x01) zc->constructor = zf;
		if (ftype & 0x02) zc->destructor = zf;
		if (ftype & 0x04) zc->clone = zf;
		if (ftype & 0x08) zc->__get = zf;
		if (ftype & 0x10) zc->__set = zf;
		if (ftype & 0x20) zc->__call = zf;
#ifdef ZEND_ENGINE_2_1
		if (ftype & 0x40) zc->__unset = zf;
		if (ftype & 0x80) zc->__isset = zf;
		if (ftype & 0x100) zc->serialize_func = zf;
		if (ftype & 0x200) zc->unserialize_func = zf;
#endif
#ifdef ZEND_ENGINE_2_2
		if (ftype & 0x400) zc->__tostring = zf;
#endif
#ifdef ZEND_ENGINE_2_3
		if (ftype & 0x800) zc->__callstatic = zf;
#endif
	}
#endif

	switch(zf->type)
	{
		case ZEND_INTERNAL_FUNCTION:
			break;
		case ZEND_USER_FUNCTION:
		case ZEND_EVAL_CODE:
			apc_deserialize_zend_op_array(&zf->op_array, 0 TSRMLS_CC);
			break;
		default:
			/* the above are all valid zend_function types.  If we hit this
			 * case something has gone very very wrong. */
			if (!BCOMPILERG(parsing_error)) {
				php_error(E_WARNING, "bcompiler: Bad bytecode file format at %08x", (unsigned)php_stream_tell(BCOMPILERG(stream)));
				BCOMPILERG(parsing_error) = 1;
			}
			break;
	}
	/* do not create functions that are processed by zend_do_inheritance() */
	if (BCOMPILERG(cur_zc) && zf->type == ZEND_INTERNAL_FUNCTION) return -1;
	return 0;
}

void apc_create_zend_function(zend_function** zf TSRMLS_DC)
{
	*zf = (zend_function*) ecalloc(1, sizeof(zend_function));
	if (apc_deserialize_zend_function(*zf TSRMLS_CC) == -1) {
		efree(*zf);
		*zf = NULL;
	}
}

void apc_free_zend_function(zend_function** zf TSRMLS_DC)
{
	if (*zf != NULL) efree(*zf);
	*zf = NULL;
}


/* --- zend_constant ------------------------------------------------------- */

void apc_serialize_zend_constant(zend_constant* zc TSRMLS_DC)
{
	apc_serialize_zval(&zc->value, NULL TSRMLS_CC);
	SERIALIZE_SCALAR(zc->flags, int);
	apc_serialize_zstring(ZS2S(zc->name), ZLEN(zc->name_len) TSRMLS_CC);
	BCOMPILER_DEBUG(("serializing constant %s\n",zc->name));
	SERIALIZE_SCALAR(zc->name_len, uint);
	SERIALIZE_SCALAR(zc->module_number, int);
}

void apc_deserialize_zend_constant(zend_constant* zc TSRMLS_DC)
{
	apc_deserialize_zval(&zc->value, NULL TSRMLS_CC);
	DESERIALIZE_SCALAR(&zc->flags, int);
	apc_create_string_u(ZS2SP(zc->name), -1 TSRMLS_CC);
	BCOMPILER_DEBUG(("deserializing constant %s\n",zc->name));
	DESERIALIZE_SCALAR(&zc->name_len, uint);
	DESERIALIZE_SCALAR(&zc->module_number, int);
}

void apc_create_zend_constant(zend_constant** zc TSRMLS_DC)
{
	*zc = (zend_constant*) ecalloc(1, sizeof(zend_constant));
	apc_deserialize_zend_constant(*zc TSRMLS_CC);
}



/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
