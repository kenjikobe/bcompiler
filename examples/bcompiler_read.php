<?php
  if(!extension_loaded('bcompiler')) {
            dl('bcompiler.'.PHP_SHLIB_SUFFIX);
        }
/* note : bzopen only works with php4.3, just use fopen on php4.2.*

/* read the test */
echo "load {$_SERVER['argv'][1]}\n";
if (!file_exists($_SERVER['argv'][1])) {
    echo "does not exist\n";
    exit;
}                                   
$fh = fopen($_SERVER['argv'][1],"r");
bcompiler_read($fh);
fclose($fh);

/* the normal method - eg. include class */
//bcompiler_load("/tmp/testa");

/* unlikely to work as php-embeded has not be released yet! 
Although you could merge a php file with win_php.exe (from php-gtk cvs)
Modify it to load php with php.exe -r 'bcompiler("/tmp/testexe");'
*/
//bcompiler_load_exe("/tmp/testcompile2");
print_r(get_included_files());
print_r(get_declared_classes());

?>
Done

