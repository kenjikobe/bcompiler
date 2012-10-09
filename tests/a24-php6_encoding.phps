<?php
//--FILE--
/* NOTE! This file contains some names in Russian (encoding: koi8-r) */
//--CODE--
abstract class Класс {
  var $ыыы = "(base class)";
  function тест() { return $this->ыыы; }
  abstract function show();
}
class ТожеКласс extends Класс {
  var $sss;
  function ТожеКласс($v) {
    $this->sss[__CLASS__] = "(child class)"; 
    $this->sss[(binary)$v] = $v;
  }
  function тест() { return $this->sss["ТожеКласс"].parent::тест(); }
  function show() { echo $this->тест(), "\n"; }
}

function Функ($s) { return ucfirst($s); }

$name = "john doe";
$имя  = &$name;
$имя{5} = strtoupper($name{5});
$ref  = 'имя';
echo "Name: ".Функ($$ref)."\n";

$obj = new ТожеКласс($имя);
$obj->show();
var_dump( strtolower($obj->sss[(binary)$name]) );

var_dump($obj->sss);
?>
--EXPECT--
