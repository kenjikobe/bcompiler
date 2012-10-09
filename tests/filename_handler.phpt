--TEST--
Test bcompiler_set_filename_handler
--SKIPIF--
<?php
  $dir = dirname(__FILE__).'/';
  require($dir.'test-helper.php');
  check_bcompiler('');
  bcompiler_set_filename_handler();
  make_bytecode($dir.'filename_handler.phpt', $dir.'filename_handler1.phb');
  bcompiler_set_filename_handler('basename');
  make_bytecode($dir.'filename_handler.phpt', $dir.'filename_handler2.phb');
  bcompiler_set_filename_handler('');
  make_bytecode($dir.'filename_handler.phpt', $dir.'filename_handler3.phb');
?>
--FILE--
<?php
for ($i = 1; $i <= 3; $i++) {
  include("filename_handler{$i}.phb");
  unlink(dirname(__FILE__)."/filename_handler{$i}.phb");
}
exit;
--CODE--
$z = 0;
echo "error: ", (5 / $z), "\n";
--EXPECTREGEX--
error: 
Warning: Division by zero in .*\S+filename_handler1\.phb\.in on line 3

error: 
Warning: Division by zero in filename_handler2\.phb\.in on line 3

error: 
Warning: Division by zero in .*\S+filename_handler3\.phb on line 3
