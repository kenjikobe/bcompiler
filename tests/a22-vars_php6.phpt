--TEST--
Test variables
--SKIPIF--
<?php
  if (version_compare(PHP_VERSION, '6', '<')) die("skip This version is for PHP 6 or later");
  $dir = dirname(__FILE__).'/';
  require($dir.'test-helper.php');
  make_bytecode($dir.'a22-vars_php6.phpt', $dir.'a22-vars.phb');
?>
--INI--
unicode.script_encoding = koi8-r
unicode.output_encoding = koi8-r
--FILE--
<?php
include("a22-vars.phb");
unlink(dirname(__FILE__).'/a22-vars.phb');
exit;
//--CODE--
  $a = 2;
  $b = $a * $a;
  $Ê = ++$a;
  $d = $Ê * $a / 5;
  $s = "A string\n2nd line";
  $arr = array($a, $b, $d);
  $x = 'Ê';
  $$x *= 10;
  var_dump($Ê);
  var_dump($d);
  var_dump($s);
  var_dump($arr);
  $arr2[ $s{0} ] = &$a;
  $a = ($Ê % 10) ? "yes" : "no";
  echo $arr2['A'], "\n";
  $z = "_SELF";
  echo $_SERVER[(binary) "PHP{$z}"], "\n";
?>
--EXPECTREGEX--
int\(30\)
float\(1\.8\)
unicode\(17\) \"A string
2nd line\"
array\(3\) \{
  \[0\]=>
  int\(3\)
  \[1\]=>
  int\(4\)
  \[2\]=>
  float\(1\.8\)
\}
no
.*a22-vars_php6\.php$
