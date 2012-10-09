--TEST--
Bug #5693 - Handling of __FILE__ constant
--SKIPIF--
<?php
  $dir = dirname(__FILE__).'/';
  require($dir.'test-helper.php');
  bcompiler_set_filename_handler('');
  ini_set('bcompiler.detect_filedir', 1);
  make_bytecode($dir.'bug-5693.phpt', $dir.'bug-5693-on.phb');
  ini_set('bcompiler.detect_filedir', 0);
  make_bytecode($dir.'bug-5693.phpt', $dir.'bug-5693-off.phb');
?>
--FILE--
<?php
$s = dirname(__FILE__);
include("bug-5693-on.phb");
unlink(dirname(__FILE__).'/bug-5693-on.phb');
include("bug-5693-off.phb");
unlink(dirname(__FILE__).'/bug-5693-off.phb');
exit;
--CODE--
$s1 = str_replace($s, '', __FILE__);
if ($s1{0} != 'b') $s1 = substr($s1, 1);
echo "filename=[$s1]\n";
--EXPECT--
filename=[bug-5693-on.phb]
filename=[bug-5693-off.phb.in]
