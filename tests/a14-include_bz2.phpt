--TEST--
Test include() with bzip2 bytecode
--SKIPIF--
<?php
  $dir = dirname(__FILE__).'/';
  require($dir.'test-helper.php');
  make_bytecode($dir.'a14-include_bz2.phpt', $dir.'a14-include.phb');
  system('bzip2 '.$dir.'a14-include.phb');
  @unlink($dir.'a14-include.phb');
  is_file($dir.'a14-include.phb.bz2') or die("skip bzip2 not found");
?>
--FILE--
<?php
include(dirname(__FILE__)."/a14-include.phb.bz2");
unlink(dirname(__FILE__).'/a14-include.phb.bz2');
exit;
//--CODE--
echo "ok\n";
?>
--EXPECT--
ok
