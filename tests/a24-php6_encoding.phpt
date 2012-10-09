--TEST--
Test non-Latin script encoding in PHP 6
--SKIPIF--
<?php
  if (version_compare(PHP_VERSION, '6', '<')) die("skip PHP 6 or later required");
  $dir = dirname(__FILE__).'/';
  require($dir.'test-helper.php');
  make_bytecode($dir.'a24-php6_encoding.phps', $dir.'a24-php6_encoding.phb');
?>
--INI--
unicode.script_encoding = koi8-r
unicode.output_encoding = iso-8859-1
--FILE--
<?php
include("a24-php6_encoding.phb");
unlink(dirname(__FILE__).'/a24-php6_encoding.phb');
?>
--EXPECT--
Name: John Doe
(child class)(base class)
unicode(8) "john doe"
array(2) {
  [u"?????????"]=>
  unicode(13) "(child class)"
  ["john Doe"]=>
  unicode(8) "john Doe"
}
