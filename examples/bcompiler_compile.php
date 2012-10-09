#!/usr/bin/php 
<?php

require_once 'PEAR.php';
function test($data) {
    print_r($data);
}
/**
* Class to compile a given file into bytecodes.
*
* @abstract 
* Takes various input and generates a compiled file.
*
*
* @version    $Id: bcompiler_compile.php 163305 2004-07-14 12:44:29Z alan_k $
*/
class PHP_bcompiler_Compile {

    /**
    * load all the required extensions 
    * (eg. bcompiler and tokenizer)
    * 
    *
    * @return   none
    * @access   public|private
    * @TODO     support windows - if somebody wants to attempt it.
    */
  
    function _loadExtensions() {
        if(!extension_loaded('bcompiler')) {
            dl(((PHP_SHLIB_SUFFIX == 'dll') ? 'php_' :'') . 'bcompiler.'.PHP_SHLIB_SUFFIX);
        }
          
        if(!extension_loaded('tokenizer')) {
            dl('tokenizer.'.PHP_SHLIB_SUFFIX);
        }
    }
        
    /**
    * List of all classes to build into file
    *
    * @var array
    * @access private
    */
    
   
    /**
    * File handle to write data to
    *
    * @var resource stream (usually bz stream)
    * @access public
    */

    var $_fh;

    
    /**
    * Instantator - start the process
    *
    * read from file (or files later?)
    * output to compiled file usually original.phb
    * optionally build the exe file - currently I think only edlink has it.
    * 
    * usage:
    * $test = new PHP_bcompiler_compile('myfile.php');
    * // thats it!
    *
    * @param   mixed  filename    well string only at present - original file
    * @param   optional string             Output filename
    * @param   optional wrapfile           Filename of base 'exe' file 
    *
    * @return   none
    * @access   public
    * @see      see also methods.....
    */
  

    function PHP_bcompiler_Compile($filename,$wrapfile='') {
         
    
        $this->_loadExtensions();
        
        $output = $filename . ".phb";
        if ($wrapfile != '') {
            
            
            $output = $filename . ".exe.tmp";
            if (PHP_SHLIB_SUFFIX == 'dll') {
                $this->_fh  = fopen($output,'w');
            } else {
                $this->_fh  = bzopen($output,'w');
            }
            
            //$this->_fh =bzopen($fh,"w");
        } else {
             $this->_fh = fopen($output,"w");
        }
        echo "WRITING TO $output\n";
        
        bcompiler_write_header($this->_fh);
        $this->_compileFile($filename);
        bcompiler_write_footer($this->_fh);
        fclose($this->_fh);
        if ($wrapfile != '') {
            $output = $filename . ".exe";
            $outputbz = $filename . ".exe.tmp";
            $fh = fopen($output,"w");
            $fc = fopen($wrapfile,"r");
            $data = fread($fc,filesize($wrapfile));
            fclose($fc);
            fwrite($fh,$data, filesize($wrapfile));
            $start = ftell($fh);
            $fc = fopen($outputbz,"r");
            $data = fread($fc,filesize($outputbz));
            fclose($fc);
            fwrite($fh,$data, filesize($outputbz));
            bcompiler_write_exe_footer($fh,$start);
            fclose($fh);
        }
 
    }
    var $_classes = array();
    
    /**
    * compile a file.
    *
    * find all the classes, then write them
    * 
    * @param   string       Filename of file to compile
    *
    * @return   none
    * @access   private
    */
  
   
  
    function _compileFile($filename) {
        $this->_buildSource($filename);
        //print_r($this->_classSource);
        //print_r($this->_requires);
        //print_r($this->_requireErrors);
        //print_r($this->_extends);
        
        foreach($this->_defines as $k => $v)
        bcompiler_write_constant($this->_fh,$k);
        
        print_r(array_keys($this->_classSource));
        foreach(array_keys($this->_classSource) as $class) {
            $this->_loadClass($class);
            
        }
        //bcompiler_parse_class( 'test','vending_deliver' );
        //bcompiler_write_class($this->_fh,'vending_deliver');
        
        //print_r(get_declared_classes());
        //print_r(get_loaded_extensions());
         
    }
    
    function _loadClass($name,$compileit = TRUE) {
        if (!class_exists($name)) {
             
            if (@$this->_extends[$name]) {
                 $this->_loadClass($this->_extends[$name], TRUE);
            }
            echo "Compiling $name\n";
            //echo $this->_classSource[$name] ;
            eval( $this->_classSource[$name] );
        }
        
        if (in_array($name, $this->_classes)) {
            return;
        }
        if (!$compileit) {
            return;
        }
        echo "COMPILE $name extends '".$this->_extends[$name]."' \n";
        
        bcompiler_write_class($this->_fh,$name,@$this->_extends[$name]); 
        echo "DONE $name\n";
        $this->_classes[] = $name;
    }
    
    /**
    * associative array of class to code
    *
    * @var array
    * @access private
    */
    var $_classSource = array();
    var $_doneFiles = array();
    /**
    * tokenize a file and get details..
    *
    * call  getClass($tokens,$pos) for classes
    * calls getInclude() for include/
    * 
    * @param   string  Filename
    * 
    *
    * @return   none
    * @access   private
    */
  
    function _buildSource($filename) {
        // a little unix specific...
        if ($filename{0} != "/") {
            $filename = $this->_getRealFilename($filename);
        }
        if (in_array($filename, $this->_doneFiles)) {
            return;
        }
        
        
        
        $fp = fopen($filename, 'r');
        echo "PARSE: $filename\n";
        $content  = fread($fp, filesize($filename));
        fclose($fp);
        $tokens = token_get_all($content);
        $count = count($tokens);
        
        $buffer = "";
        
        for ($i = 0; $i < $count; $i++) {
            $token = $tokens[$i];
            if (is_array($token)) {
                switch ($token[0]) {
                    case T_CLASS:
                        $i = $this->_parseClass($filename,$tokens,$i);
                        continue 2;
                    case T_REQUIRE_ONCE:
                        $i = $this->_parseRequire($filename,$tokens,$i);
                        continue 2;
                    case T_COMMENT:
                        $buffer .= "\n";
                        continue 2;
                    case T_STRING:
                        if (strtolower($token[1]) == 'dl') {
                            $i = $this->_parseDl($tokens,$i);
                        }
                        if (strtolower($token[1]) == 'define') {
                            $i = $this->_parseDefine($tokens,$i);
                        }
                                                
                        break;
                    default: 
                       // echo "Unknown tag: " . token_name($token[0]) . ": {$token[1]}\n";
                        
                }
                $t = token_name($token[0]) . ":".$token[1];
                
                $buffer .= $token[1];
                
            } else {
                //echo "TEXT: $token\n";
                $buffer .= $token;
               $t = $token;
            }
            //echo "$t\n"; 
        }
       
        $this->_doneFiles[] = $filename;
        foreach($this->_requires as $f) {
            $this->_buildSource($f);
        }
         
        
    }
    
    var $_extends = array();
    
    function _parseClass($filename,$tokens,$j) {
        $count = count($tokens);
        
        $curClass = "";
        $buffer = "";
        $indent = 0; // curly brackets
        $prepend = "";
        $extends = "";
        
        for ($i = $j; $i < $count; $i++) {
            $token = $tokens[$i];
            if (is_array($token)) {
              
                switch ($token[0]) {
                    case T_DOLLAR_OPEN_CURLY_BRACES:
                    case T_CURLY_OPEN:
                        $indent++;
                        break;
                    case T_STRING;
                        if (!$curClass) {
                            $curClass = $token[1];
                            $this->_extends[$curClass] = '';
                            break;
                        } else if (!$indent && !$extends) {
                            $this->_extends[$curClass] = $token[1];
                            continue 2;
                        }
                        break;
                        
                    case T_INCLUDE_ONCE:
                    case T_REQUIRE_ONCE:
                        $i = $this->_parseRequire($filename,$tokens,$i);
                        $buffer .= $this->_getActiveRequireMapping();
                        $prepend = "";
                        continue 2;
                    case T_EXTENDS;
                        // remove extends..
                        continue 2;
                }
                $buffer .= $prepend .$token[1];
                $prepend = "";
                $t = token_name($token[0]) . ":".$token[1];
            } else {
                if ($token == '{') {
                    $indent++;
                }
                if ($token == '}') {
                    $indent--;
                    if ($indent == 0) {
                        // change parent:: to realextendsname::
                        $buffer = preg_replace('/parent\s*::/',$this->_extends[$curClass] .'::',$buffer);
                        $buffer = preg_replace('/__FILE__/','$_SERVER[\'argv\'][0]',$buffer);
                    
                        $this->_classSource[$curClass] = $buffer . "}";
                        $this->_classFiles[$curClass] = $filename;
                        echo "ADDED CLASS: $curClass\n";
                        return $i;
                    }
                }
                if ($token == "@") {
                    $prepend = "@";
                    $token = "";
                } else {
                    $buffer .= $prepend . $token;
                    $prepend = "";
                }
                $t = $token;
            }
            //echo "INDENT: $indent: $t\n";
                 
        }
    }
    var $_requires = array();
    var $_requireErrors = array();
    var $_activeRequire = "";
    function _parseRequire($filename,$tokens,$j) {
        $count = count($tokens);
        $j++;
        $curClass = "";
        $buffer = "";
        $indent = 0;
        
        for ($i = $j; $i < $count; $i++) {
            $token = $tokens[$i];
            if (is_array($token)) {
                
                $buffer .= $token[1];
                
            } else {
                if ($token == '(') {
                    $indent++;
                }
                if ($token == ')') {
                    $indent--;
                    
                }
                if ($token == ';') {
                    if (strpos($buffer,'$') !== FALSE) {
                        echo "Error: unable to parse include/require  $buffer \n";
                        $this->_requireErrors[] = $buffer;
                        $this->_activeRequire = $buffer;
                        return $i;
                    }
                    $buffer = preg_replace("/dirname\s*\(\s*__FILE__\s*\)/i", '"'.dirname($filename).'"', $buffer);
                    $buffer = preg_replace("/^\s*[\(]?\s*[\"\']{1}(.*)[\"\']{1}\s*[\)]?\s*$/i", "\\1", $buffer);
                    $buffer = preg_replace("/[\"\']{1}\s*\.\s*[\"\']{1}/i", "", $buffer);
                    $this->_requires[] = $buffer;
                    $this->_activeRequire = $buffer;
                    return $i;
                }
                $buffer .= $token;
                 
            }
            //echo "INDENT: $indent\n";
                 
        }   
    }
    function _parseDl($tokens,$j) {
        $count = count($tokens);
        
        $curClass = "";
        $buffer = "";
        $indent = 0;
        
        for ($i = $j; $i < $count; $i++) {
            $token = $tokens[$i];
            if (is_array($token)) {
                $buffer .= $token[1];
            } else {
                if ($token == '(') {
                    $indent++;
                }
                if ($token == ')') {
                    $indent--;
                    
                }
                if ($token == ';') {
                    echo $buffer;
                    eval($buffer.";");
                     
                    return $i;
                }
                $buffer .= $token;
                 
            }
                 
        }   
    }
    var $_defines = array();
    function _parseDefine($tokens,$j) {
        $count = count($tokens);
        
        $curClass = "";
        $buffer = "";
        $indent = 0;
        $j++;
        
        for ($i = $j; $i < $count; $i++) {
            $token = $tokens[$i];
            if (is_array($token)) {
                $buffer .= $token[1];
            } else {
                
                if ($token == '(') {
                    if ($indent > 0) {
                        $buffer .= '(';
                    }
                    $indent++;
                    continue;
                }
                if ($token == ')') {
                    if ($indent > 1) {
                        $buffer .= ')';
                    }
                    $indent--;
                    continue;
                }
                if ($token == ';') {
                    
                    if (isset($this->_defines[$definekey])) {
                        return $i;
                    }
                    $this->_defines[$definekey] = true;
                    // echo "DEFINE: $definekey => $value\n";
                    if (!defined($definekey)) {
                        eval('define("'.$definekey.'",'. $buffer.');');
                    }
                    return $i;
                }
                if ($token == ',') {
                    if ($indent > 1) {
                        $buffer .= ',';
                        continue;
                    }
                    $test = '$definekey = ' .$buffer .';';
                    //echo "$test\n";
                    eval($test);
                    $buffer= '';
                    continue;
                }
                
                $buffer .= $token;
                 
            }
                 
        }   
    }
    
    function _getActiveRequireMapping() {
        $mappings = PEAR::getStaticProperty('PHP_bcompiler_compile','mappings');
        return @$options[$this->_activeRequire];
    }
    
    function _getRealFilename($filename) {
        // unix specific again
        $paths = explode(":", ini_get('include_path'));
        // add a few helpers..
        $paths[] = dirname($_SERVER['argv'][1]);
        $paths[] = dirname($_SERVER['argv'][1]) . "/..";
        // override just in case the application is doing something fun with ini_set
        $paths = array_reverse($paths); 
        foreach($paths as $path) {
            
            //echo "TRY {$path}/{$filename}\n";
            if (file_exists("{$path}/{$filename}")) {
                return "{$path}/{$filename}";
            }
            
        }
        echo "COULD NOT FIND $filename";
        exit;
    }
    
    
}

// this can be a very memory hungry script.!
ini_set('memory_limit', '128M');

 
// load mappings
// file of ("DB/${type}.php") == bcompiler_include('.....');
/// just load it up and explode by ==, then  store it.
 
if (isset($_SERVER['argv'][1])) {
    $test = new PHP_bcompiler_compile($_SERVER['argv'][1],@$_SERVER['argv'][2]);
}else {
    echo "\nCompiler Test Usage: \n".
        "to compile the classes in XXXXXXXX.php into XXXXXXXXX.php.phb\n\n".
         ">C:\php4\php.exe ".basename(__FILE__)." XXXXXXXXX.php \nor\n".
         "#/usr/bin/php ".basename(__FILE__)." XXXXXXXXX.php \n\n";
        "to compile the classes in XXXXXXXX.php and use exe wrapping file YYY.exe into XXXXXXXXX.php.exe\n\n".
         ">C:\php4\php.exe ".basename(__FILE__)." XXXXXXXXX.php YYY.exe\nor\n".
         "#/usr/bin/php ".basename(__FILE__)." XXXXXXXXX.php YYY.exe\n\n";
}


?>
