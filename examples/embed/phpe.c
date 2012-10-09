#include <php_embed.h>

 
int main(int argc, char **argv) {
	 
	char *my_filename = NULL;
	char *code = " $s =((PHP_SHLIB_SUFFIX == 'dll') ? 'php_' : '') . 'bcompiler.' .PHP_SHLIB_SUFFIX; "
		"dl($s); "
		"if (!extension_loaded('bcompiler')) { echo 'bcompiler not loaded - dll/so should be in same directory as exe'; exit; }"
		"bcompiler_load_exe($_SERVER['argv'][0]);"
		"if (!class_exists('main')) { echo 'main class does not exist';   }"
		"main::main();";
	
	/* this doesnt work as php.gtk needs an ini option set that doesnt want to work any other way.. */
	/* php_embed_module.php_ini_ignore = 1; */
	

	PHP_EMBED_START_BLOCK(argc,argv);
	 
	
	my_filename = strdup(argv[0]);
	php_dirname(my_filename, strlen(my_filename));
	 
	zend_alter_ini_entry("extension_dir", 14, my_filename, strlen(my_filename), PHP_INI_ALL, PHP_INI_STAGE_ACTIVATE);
	zend_alter_ini_entry("error_reporting", 16, "1024", 4, PHP_INI_ALL, PHP_INI_STAGE_ACTIVATE);
	
	
	zend_eval_string(code, NULL, argv[0] TSRMLS_CC);
	 
	
	
	PHP_EMBED_END_BLOCK();
	return 0;
}
