--TEST--
Test include() with gzipped bytecode
--SKIPIF--
<?php
  $dir = dirname(__FILE__).'/';
  require($dir.'test-helper.php');
  make_bytecode($dir.'a13-include_gz.phpt', $dir.'a13-include.phb');
  system('gzip -c '.$dir.'a13-include.phb > '.$dir.'a13-include.phb.gz');
  @unlink($dir.'a13-include.phb');
  is_file($dir.'a13-include.phb.gz') or die("skip gzip not found");
?>
--FILE--
<?php
include(dirname(__FILE__)."/a13-include.phb.gz");
unlink(dirname(__FILE__).'/a13-include.phb.gz');
exit;
//--CODE--
echo "ok\n";
?>
--EXPECT--
ok
