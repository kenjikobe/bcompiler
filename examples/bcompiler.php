<?php

/* simple example of compiler used different ways.*/ 

if(!extension_loaded('bcompiler')) {
	dl('bcompiler.so');
}
 
require_once('DB.php');


$fh = fopen("/tmp/testa","w");
bcompiler_write_header($fh);
bcompiler_write_included_filename($fh,__FILE__);
bcompiler_write_class($fh,'DB');   
bcompiler_write_class($fh,'PEAR_error');   
bcompiler_write_class($fh,'DB_error');   
fclose($fh);
 
 
$fh = bzopen("/tmp/testb","w");
bcompiler_write_header($fh);
bcompiler_write_class($fh,'DB');   
bcompiler_write_class($fh,'PEAR_error');   
bcompiler_write_class($fh,'DB_error');   
fclose($fh);
 

$fh = fopen("/tmp/testexe","w");
$fc = fopen(dirname(__FILE__)."/code","r");
$data = fread($fc,filesize(dirname(__FILE__)."/code"));
fwrite($fh,$data, filesize(dirname(__FILE__)."/code"));
fclose($fc);
$start = ftell($fh);
$bz = bzopen($fh,"w");
bcompiler_write_header($bz);
bcompiler_write_class($bz,'DB');   
bcompiler_write_class($bz,'PEAR_error');   
bcompiler_write_class($bz,'DB_error');   
bcompiler_write_footer($bz);
fclose($bz);

$fh= fopen("/tmp/testcompile2","a");
fseek($fh,0,SEEK_END);
bcompiler_write_exe_footer($fh,$start);

fclose($fh);
 


?>

Done
 
