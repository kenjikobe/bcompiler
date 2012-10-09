<?php
if(!extension_loaded('bcompiler')) {
	dl('bcompiler.so');
}
  

  
require_once('DB.php');




define('ZEND_FETCH_GLOBAL',		0);
define('ZEND_FETCH_LOCAL',		1);
define('ZEND_FETCH_STATIC',		2);

/* var status for backpatching */
define('BP_VAR_R',			0);
define('BP_VAR_W',			1);
define('BP_VAR_RW',			2);
define('BP_VAR_IS',			3);
define('BP_VAR_NA',			4);	/* if not applicable */
define('BP_VAR_FUNC_ARG',		5);
define('BP_VAR_UNSET',		6);

define('ZEND_IS_CONST'	,1);
define('ZEND_IS_TMP_VAR'	,2);
define('ZEND_IS_VAR'		,4);
define('ZEND_IS_UNUSED'	,8);	/* Unused variable */



function test($data) {
    global $generator;
    if (@$data['ISEND']) {
        //print_r($data);
        if ($generator->_opcodes) {
            $generator->_pass_two();
        }
        
        //echo "NEW GENERATOR";
        $generator = new generator;
    }
    /*
    echo "    ";
    echo @str_pad($data['LINE'],4) . ":";
    echo str_pad($data['OPCODE'],4);
    echo str_pad($data['OPCODE_STR'],20);
    if (@$data['HASH']  || @$data['CLASS']) {
        echo " ".@$data['HASH'] . " ". @$data['CLASS'] ;

    }
    echo ":";
    echo str_pad(@$data['RESULT'] ,15);
    echo "= ";
    echo str_pad(@$data['OP1'] ,15);
    echo str_pad(@$data['OP2'] ,15);
     */
    $generator->_add_op($data);
 
    
    echo "\n";
    
    //print_r($data);
}


$generator = new generator;
$classname = "vending_omnitor";
   
bcompiler_parse_class('test','DB');   



class generator {
    
    var $zend_to_realname;
    var $realname_to_zend;
    var $args;
    var $function_name; 
    var $_fcall; // funciton about to be called.
    var $send_vars = array();
    var $_gotos = array();    
    var $init_opcode; // initilized opcode
    var $Ts = array();
    var $_init_static_vars = FALSE; 
    
    var $_string_buffer; // deprecieated
    
    function op_begin_hash_def($d) {
        //
        //$this->function_name = $d['HASH'];
    }            
    function op_opcode_array($d) {    // expects an op array...
        if (!$d) {
            return;
        }
        global $classname;
        $this->init_opcode = unserialize(serialize($d));
        $this->function_name = $d['FUNCTION_NAME'];
        $this->_out2("",TRUE);
        $this->_out2("PHP_FUNCTION({$classname}_".$d['FUNCTION_NAME']. ")\n{");
        
       
        
## TODO = the base_function stuff needs sorting out - re: pointer or what...
        $out = "
            /* dummy op codes */
            znode* result= (zend_op*) emalloc(sizeof(znode));
            znode* op1= (zend_op*) emalloc(sizeof(znode));
            znode* op2= (zend_op*) emalloc(sizeof(znode));
            
            zval* result_zv = (zval*) emalloc(sizeof(zval));
            zval* op1_zv = (zval*) emalloc(sizeof(zval));
            zval* op2_zv = (zval*) emalloc(sizeof(zval));
            
            zvalue* result_zvalue = (zval*) emalloc(sizeof(zvalue));
            zvalue* op1_zvalue = (zval*) emalloc(sizeof(zvalue));
            zvalue* op2_zvalue = (zval*) emalloc(sizeof(zvalue));
            int extended_value = 0;
            
            zend_execute_data execute_data;
            zend_function base_function; /* not really sure how this is needed.....*/
            
            
            base_function.type =  {$d['TYPE']} ;
            base_function.function_name = \"{$d['FUNCTION_NAME']}\";
            
            EG(execute_data_ptr) = &execute_data;
        
            /* Initialize execute_data */
            execute_data.fbc = NULL;
            execute_data.object.ptr = NULL;
            execute_data.Ts = (temp_variable *) do_alloca(sizeof(temp_variable)*".$d['T'].");
            execute_data.original_in_execution=EG(in_execution);
        
            EG(in_execution) = 1;
            
        
            /* EG(opline_ptr) = &execute_data.opline; */
        
            execute_data.function_state.function = base_function;
            EG(function_state_ptr) = &execute_data.function_state;
            
        ";
        
        
        
        $this->_out2($out);
                     
        
        if (@$this->args) {
             $out = "           if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
                          \"";
            $start_optional = FALSE;
            foreach($this->args as $d) {
                //print_r($d);
                
                if ($d['OPCODE'] == 64) { // RECV_INIT
                    
                    if (!$start_optional) {
                        $out .= "|";
                        $start_optional = TRUE;
                    }
                }
                $out .= "z";
            }
            $out .= "\" ";
            foreach($this->args as $d) {
                $out .= ", &". $this->_get_zval_ptr_ptr("RESULT", $d) ;
            }
            $out .=")== FAILURE)  return;";
            $this->_out2($out);
            
            /* initialize values */
            foreach($this->args as $d) {
                if ($d['OPCODE'] != 64) { // RECV_INIT
                    continue;
                }
                if (($d["OP2.u.constant.type"] == IS_CONSTANT)  || 
                    ($d["OP2.u.constant.type"]==IS_CONSTANT_ARRAY)) {
						echo "NOT SUPPORTED YET";
                        exit;
                } else {
                    // lets hope it was a string!
                    $this->_out2("
                        if (ZEND_NUM_ARGS() < {$d['OP1']}) {
                    ");
                    $this->_assign_zval("execute_data.Ts[{$d['RESULT.u.var']}].tmp_var",$d['OP2.u.constant.val']);
                    $this->_out2("
                        }
                    ");
                }
            }
        }
        
        
    }
    function op_add_string($d) {
        list($free,$var) = $this->_get_zval_ptr("OP1",$d);
        $this->_out2("
                add_string_to_string(	 execute_data.Ts[". $d["RESULT.u.var"]."].tmp_var, ".
										  $var . ", \"". $d["OP2.u.constant.val"] . "\")"); 
			
         
         
         
    }
    function op_add_var($d) {
            list($free, $var2) = $this->_get_zval_ptr("OP2",$d);
            list($free, $var1) = $this->_get_zval_ptr("OP1",$d);

            $this->_out2("
			{
                zval var_copy;
                int use_copy;
                zval *var = $var2
                zend_make_printable_zval(var, &var_copy, &use_copy);
                if (use_copy) {
                    var = &var_copy;
                }
                add_string_to_string(	&execute_data.Ts[". $d["RESULT.u.var"]."].tmp_var,
									$var1, var);
                if (use_copy) {
                    zval_dtor(var);
                }
                 
                
			}
            ");
            
            
            // FREE_OP(execute_data.Ts, &EX(opline)->op2, EG(free_op2)); 
       
       
       
       
       
       
       
       
    }
    function op_assign($d) {
        //print_r($d);
        /* ??? what if its an int or something? */
        //$string_to_use = $this->_string_buffer[$d['OP2']];
        //$args_to_use = $this->_string_buffer_args[$d['OP2']];
        //$var = $this->zend_to_realname[$d['OP1']];
         
        //$this->_out( "    $var = sprintf(\"{$string_to_use}\",". implode(",",$args_to_use).");");
        $this->_out2("
                    {
        ");
        $this->_make_opcode("RESULT",$d);
        $this->_make_opcode("OP1",$d);
        $this->_make_opcode("OP2",$d);
        $this->_out2("   
                    zval *value;
                    value = get_zval_ptr(op2, execute_data.Ts, &EG(free_op2), BP_VAR_R);
                    zend_assign_to_variable(result, op1, op2, value, (EG(free_op2)?IS_TMP_VAR:{$d['OP2.op_type']}), 
                        execute_data.Ts TSRMLS_CC);
                    }
        ");
 
    
    }
    function op_assign_ref($d) {
        $this->_make_opcode("RESULT",$d);
        $this->_make_opcode("OP1",$d);
        $this->_make_opcode("OP2",$d);
        $this->_out2("
            zend_assign_to_variable_reference(result, 
                get_zval_ptr_ptr(&op1, execute_data.Ts, BP_VAR_W), 
                get_zval_ptr_ptr(&op2, execute_data.Ts, BP_VAR_W), 
                execute_data.Ts TSRMLS_CC);
        ");
                
    
    }
    
    function op_bool_not($d) {
        
        $this->_make_opcode('OP1',$d);
        $this->_out2("
            EG(unary_op) = get_unary_op({$d['OPCODE']});
            EG(unary_op)(&execute_data.Ts[{$d['RESULT.u.var']}].tmp_var,
                                get_zval_ptr(op1, execute_data.Ts, &EG(free_op1), BP_VAR_R) );
            FREE_OP(execute_data.Ts, &op1, EG(free_op1));
        ");
        
    }
    function op_do_fcall($d) {
    
        $this->_make_opcode("OP1",$d);
        $this->_out2("
                {
                    zval *fname = get_zval_ptr(&op1, execute_data.Ts, &EG(free_op1), BP_VAR_R);

					if (zend_hash_find(EG(function_table), fname->value.str.val, fname->value.str.len+1, (void **) &execute_data.function_state.function)==FAILURE) {
						zend_error(E_ERROR, \"Unknown function:  %s()\n\", fname->value.str.val);
					}
					FREE_OP(execute_data.Ts, &op1, EG(free_op1));
					zend_ptr_stack_push(&EG(arg_types_stack), execute_data.object.ptr);
					execute_data.object.ptr = NULL;
        ");
        $this->_do_fcall_common($d);
        $this->_out2("
                }
        ");
        
    }
    function op_do_fcall_by_name($d) {
        
        if ($this->_fcall) {
            /* object + array! */
            
            $this->_out2("
                call_user_function_ex(CG(function_table),
                        execute_data.Ts[{$this->_fcall['OP1.u.var']}].var.ptr,
                        \"{$this->_fcall['OP2.u.constant.val']}\",
                        execute_data.Ts[{$d['RESULT.u.var']}].var.ptr,
                        0, NULL, 0);
                   
                 
            ");
        }
        print_r($d);
        return;
        
        
        $this->_out2("
            execute_data.function_state.function = execute_data.fbc;
        ");
        $this->_do_fcall_common($d);
    }
    
    function op_echo($d) {
            list ($free, $var) = $this->_get_zval_ptr("OP1",$d);
            //zend_print_variable(get_zval_ptr(&EX(opline)->op1, EX(Ts), &EG(free_op1), BP_VAR_R));
            if ($var{0} == '"') {
                $this->_out2("
                    zend_write({$var}, ".(strlen($var)-1) .");
                ");
            
            } else {
                $this->_out2("
                    zend_print_variable($var);
                ");
                //FREE_OP(EX(Ts), &EX(opline)->op1, EG(free_op1));
            }
			
    
    
    }
    
    function op_fetch_w ($d) {
        // zend_fetch_var_address(EX(opline), execute_data.Ts, BP_VAR_W TSRMLS_CC);
        // initialize a new variable of type $d[op2].type
        // result: is set to refer to it..
       
        $this->_fetch_var_address($d, BP_VAR_W);
				
        //$this->zend_to_realname[$d['RESULT']] = $d['OP1'];
        //$this->realname_to_zend[$d['OP1']] = $d['RESULT'];
        
     
    }
    function op_fetch_r($d) {
        $this->_fetch_var_address($d, BP_VAR_R);
        //$this->zend_to_realname[$d['RESULT']] = $d['OP1'];
        //if (!@$this->realname_to_zend[$d['OP1']]) {
        //    $this->realname_to_zend[$d['OP1']] = $d['RESULT'];
        //}
    }
    function op_fetch_constant($d) {
        $starterror = $GLOBALS['php_errormsg'];
        $const = @constant($d['OP1.u.constant.val']);
        if ($starterror != $GLOBALS['php_errormsg']) {
            echo "NOT A CONSTANT: {$d['OP1.u.constant.val']}";
            exit;
        }
        
        $this->_assign_zval("execute_data.Ts[{$d['RESULT.u.var']}].tmp_var",$const);
         
        
    }
    function op_init_string($d) {
       
        $this->_out2( "
            execute_data.Ts[". $d['RESULT.u.var'] . "].tmp_var.value.str.val = emalloc(1);
            execute_data.Ts[". $d['RESULT.u.var'] . "].tmp_var.value.str.val[0] = 0;
            execute_data.Ts[". $d['RESULT.u.var'] . "].tmp_var.value.str.len = 0;
            execute_data.Ts[". $d['RESULT.u.var'] . "].tmp_var.refcount = 1;
        ");
        
        
        
        
        
    }
    function op_init_fcall_by_name($d) {
    
        
        /* handle these types: */
        $this->_fcall = unserialize(serialize($d));
        return;
        
        
        
        
        
        $this->_make_opcode("OP1",$d);
        $this->_make_opcode("OP2",$d);
    
        $this->_out2("
                    zval *function_name;
					zend_function *function;
					HashTable *active_function_table;
					zval tmp;

					zend_ptr_stack_n_push(&EG(arg_types_stack), 2, execute_data.fbc, execute_data.object.ptr);
					if ({$d['EXTENDED']} & ZEND_CTOR_CALL) {
						/* constructor call */

						if ({$d['OP1.op_type']} == IS_VAR) {
							SELECTIVE_PZVAL_LOCK(*execute_data.Ts[{$d['OP1.u.var']}].var.ptr_ptr, &op1);
						}
						if ({$d['OP2.op_type']} == IS_VAR) {
							PZVAL_LOCK(*execute_data.Ts[{$d['OP1.u.var']}].var.ptr_ptr);
						}
					}
					function_name = get_zval_ptr(&op2, execute_data.Ts, &EG(free_op2), BP_VAR_R);

					tmp = *function_name;
					zval_copy_ctor(&tmp);
					convert_to_string(&tmp);
					function_name = &tmp;
					zend_str_tolower(tmp.value.str.val, tmp.value.str.len);
						
					if ({$d['OP1.op_type']} != IS_UNUSED) {
						if ({$d['OP1.op_type']}==IS_CONST) { /* used for class_name::function() */
							zend_class_entry *ce;
							zval **object_ptr_ptr;

							if (zend_hash_find(EG(active_symbol_table), \"this\", sizeof(\"this\"), (void **) &object_ptr_ptr)==FAILURE) {
								execute_data.object.ptr=NULL;
							} else {
								/* We assume that \"this\" is already is_ref and pointing to the object.
								   If it isn't then tough */
								execute_data.object.ptr = *object_ptr_ptr;
								execute_data.object.ptr->refcount++; /* For this pointer */
							}
							if (zend_hash_find(EG(class_table), \"{$d['OP1.u.constant.val']}\", ". (strlen($d['OP1.u.constant.val'])+1). ", (void **) &ce)==FAILURE) { /* class doesn't exist */
								zend_error(E_ERROR, \"Undefined class name '%s'\", {$d['OP1.u.constant.val']});
							}
							active_function_table = &ce->function_table;
						} else { /* used for member function calls */
							execute_data.object.ptr = _get_object_zval_ptr(&op1, execute_data.Ts, &EG(free_op1) TSRMLS_CC);
							
							if ((!execute_data.object.ptr && execute_data.Ts[{$d['OP1.u.var']}].EA.type==IS_OVERLOADED_OBJECT)								
								|| ((execute_data.object.ptr && execute_data.object.ptr->type==IS_OBJECT) && (execute_data.object.ptr->value.obj.ce->handle_function_call))) { /* overloaded function call */
								zend_overloaded_element overloaded_element;

								overloaded_element.element = *function_name;
								overloaded_element.type = OE_IS_METHOD;

								if (execute_data.object.ptr) {
									execute_data.Ts[{$d['OP1.u.var']}].EA.data.overloaded_element.object = execute_data.object.ptr;
									execute_data.Ts[{$d['OP1.u.var']}].EA.data.overloaded_element.type = BP_VAR_NA;
									execute_data.Ts[{$d['OP1.u.var']}].EA.data.overloaded_element.elements_list = (zend_llist *) emalloc(sizeof(zend_llist));
									zend_llist_init(execute_data.Ts[{$d['OP1.u.var']}].EA.data.overloaded_element.elements_list, sizeof(zend_overloaded_element), NULL, 0);
								}
								zend_llist_add_element(execute_data.Ts[{$d['OP1.u.var']}].EA.data.overloaded_element.elements_list, &overloaded_element);
								execute_data.fbc = (zend_function *) emalloc(sizeof(zend_function));
								execute_data.fbc->type = ZEND_OVERLOADED_FUNCTION;
								execute_data.fbc->common.arg_types = NULL;
								execute_data.fbc->overloaded_function.var = {$d['OP1.u.var']};
								goto overloaded_function_call_cont;
							}

							if (!execute_data.object.ptr || execute_data.object.ptr->type != IS_OBJECT) {
								zend_error(E_ERROR, \"Call to a member function on a non-object\");
							}
							if (!execute_data.object.ptr->is_ref && execute_data.object.ptr->refcount > 1) {
								zend_error(E_ERROR, \"Bug: Problem in method call\n\");
							}
							execute_data.object.ptr->is_ref=1;
							execute_data.object.ptr->refcount++; /* For $ this pointer */
							active_function_table = &(execute_data.object.ptr->value.obj.ce->function_table);
						}
					} else { /* function pointer */
						execute_data.object.ptr = NULL;
						active_function_table = EG(function_table);
					}
					if (zend_hash_find(active_function_table, function_name->value.str.val, function_name->value.str.len+1, (void **) &function)==FAILURE) {
						zend_error(E_ERROR, \"Call to undefined function:  %s()\", function_name->value.str.val);
					}
					zval_dtor(&tmp);
					execute_data.fbc = function;
overloaded_function_call_cont:
					FREE_OP(execute_data.Ts, &op2, EG(free_op2));
				}
            ");
    
         
    }
    function op_include_or_eval($d) {
        $string_to_use = $this->_string_buffer[$d['OP1']];
        $args_to_use = $this->_string_buffer_args[$d['OP1']];
        $this->_out2( "    /* NOT SUPPORTED !*/");
       // $this->_out2( "    /*tmpstring = sprintf(\"{$string_to_use}\",". implode(",",$args_to_use).");*/");
        $this->_out2( "    /* zend_include_file(tmpstring);*/");
        $this->_out2( "    /* END NOT SUPPORTED */");
    }
    function op_return($d) {
    				
        //$this->init_opcode['return_reference']
        
              
        $this->_out2( "
                {
                    zval *retval_ptr;
					zval **retval_ptr_ptr;
        ");
        if (( $this->init_opcode['return_reference']  == 1) &&
            ($d['OP1.op_type'] != IS_CONST) && 
			($d['OP1.op_type'] != IS_TMP_VAR)) {
            $this->_make_opcode("OP1",$d);
            $this->_out2("
                    retval_ptr_ptr = get_zval_ptr_ptr(&op1, execute_data.Ts), BP_VAR_W);

                    SEPARATE_ZVAL_TO_MAKE_IS_REF(retval_ptr_ptr);
					(*retval_ptr_ptr)->refcount++;
						
					(*EG(return_value_ptr_ptr)) = (*retval_ptr_ptr);
            ");
        } else {
            $this->_make_opcode("OP1",$d);
            $this->_out2("
               
                    retval_ptr = get_zval_ptr(&op1, execute_data.Ts, &EG(free_op1), BP_VAR_R);
                        
                    if (!EG(free_op1)) { /* Not a temp var */
                        if (PZVAL_IS_REF(retval_ptr) && retval_ptr->refcount > 0) {
                            ALLOC_ZVAL(*(EG(return_value_ptr_ptr)));
                            **EG(return_value_ptr_ptr) = *retval_ptr;
                            (*EG(return_value_ptr_ptr))->is_ref = 0;
                            (*EG(return_value_ptr_ptr))->refcount = 1;
                            zval_copy_ctor(*EG(return_value_ptr_ptr));
                        } else {
                            *EG(return_value_ptr_ptr) = retval_ptr;
                            retval_ptr->refcount++;
                        }
                    } else {
                        ALLOC_ZVAL(*(EG(return_value_ptr_ptr)));
                        **EG(return_value_ptr_ptr) = *retval_ptr;
                        (*EG(return_value_ptr_ptr))->refcount = 1;
                        (*EG(return_value_ptr_ptr))->is_ref = 0;
                    }
            ");
        }
		$this->_out2("
                    free_alloca(execute_data.Ts);
                    EG(in_execution) = execute_data.original_in_execution;
                    return;
                }
				 
        ");
     
    }
    function op_jmp($d) {
        if (($d['JMP']-1) == $d['LINE']) {
            return;
        }
        $this->_gotos[$d['JMP']] = 1;
        $this->_out( "    goto line_".$d['JMP']) ;
    }
    function op_jmpz($d) {
        $this->_make_opcode("OP1",$d);
        $this->_out2("
            if (!i_zend_is_true(get_zval_ptr(op1, execute_data.Ts, &EG(free_op1), BP_VAR_R))) {
						FREE_OP(execute_data.Ts, op1, EG(free_op1));
                        goto line_{$d['OP2.u.opline_num']};
            }
			FREE_OP(execute_data.Ts, op1, EG(free_op1));
        ");
        $this->_gotos[$d['JMP']] = 1;
        
        
    }
    
    function op_jmp_no_ctor($d) {
        // jump past object initialization call if it doesnt exist.
        
        
        list($free,$var) = $this->_get_zval_ptr("OP1", $d);
        
        $this->_out2("
                if (!{$var}->value.obj.ce->handle_function_call && 
                    !zend_hash_exists(&{$var}->value.obj.ce->function_table, {$var}->value.obj.ce->name, 
                            {$var}->value.obj.ce->name_length+1)) {
                    goto line_{$d['OP2.u.opline_num']};
                }
        ");
        return;
        $this->_out2(" 
                {
                    zval *object;
        ");
        
        if ($d['OP1.op_type'] == IS_VAR) {
            $this->_out2("
                    PZVAL_LOCK(*execute_data.Ts[{$d['OP1.u.var']}].var.ptr_ptr);
            ");
        }
        $this->_make_opcode("OP1",$d);
        //print_r($d);
        //exit;
        
		$this->_out2("
                    
					object = get_zval_ptr(&op1, execute_data.Ts, &EG(free_op1), BP_VAR_R);
					if (!object->value.obj.ce->handle_function_call
						&& !zend_hash_exists(&object->value.obj.ce->function_table, object->value.obj.ce->name, object->value.obj.ce->name_length+1)) {
                        goto line_{$d['OP2.u.opline_num']};
					}
				}
        ");
        $this->_gotos[$d['OP2.u.opline_num']] = 1;
        
    
    }
    function op_new($d) {
        
        list($free,$var) = $this->_get_zval_ptr("OP1", $d);
        if ($var{0} == '"') {
            //echo "GOT STRING $var";
            $lvar = strtolower($var);
            
            $this->_out2( "
                {
                    zend_class_entry *ce;
                    zend_hash_find(EG(class_table), $lvar, ".(strlen($var) -1).", (void **) &ce);
                    execute_data.Ts[{$d['RESULT.u.var']}].var.ptr_ptr = &execute_data.Ts[{$d['RESULT.u.var']}].var.ptr;
                    ALLOC_ZVAL(execute_data.Ts[{$d['RESULT.u.var']}].var.ptr);
                    object_init_ex(execute_data.Ts[{$d['RESULT.u.var']}].var.ptr, ce);
                    execute_data.Ts[{$d['RESULT.u.var']}].var.ptr->refcount=1;
                    execute_data.Ts[{$d['RESULT.u.var']}].var.ptr->is_ref=1;
                }
            ");
            return;
            
        }
        
        $this->_make_opcode("OP1", $d);
       
        
        $this->_out2("
                {
                    zval *tmp = get_zval_ptr(&op1, execute_data.Ts, &EG(free_op1), BP_VAR_R);
                    zval class_name;
                    zend_class_entry *ce;
                    
                    class_name = *tmp;
                    zval_copy_ctor(&class_name);
                    convert_to_string(&class_name);
                    zend_str_tolower(class_name.value.str.val, class_name.value.str.len);
                    
                    if (zend_hash_find(EG(class_table), class_name.value.str.val, class_name.value.str.len+1, (void **) &ce)==FAILURE) {
                        zend_error(E_ERROR, \"Cannot instantiate non-existent class:  %s\", class_name.value.str.val);
                    }
                    execute_data.Ts[{$d['RESULT.u.var']}].var.ptr_ptr = &execute_data.Ts[{$d['RESULT.u.var']}].var.ptr;
                    ALLOC_ZVAL(execute_data.Ts[{$d['RESULT.u.var']}].var.ptr);
                    object_init_ex(execute_data.Ts[{$d['RESULT.u.var']}].var.ptr, ce);
                    execute_data.Ts[{$d['RESULT.u.var']}].var.ptr->refcount=1;
                    execute_data.Ts[{$d['RESULT.u.var']}].var.ptr->is_ref=1;
                    zval_dtor(&class_name);
                    FREE_OP(execute_data.Ts, &op1, EG(free_op1));
                }
            ");
		
    
    }
    function op_recv($d) {
        //print_r($d);
        $this->args[$d['OP1.u.constant.val'] ]  =  $d;
        
    }
    function op_recv_init($d) { 
        $this->args[$d['OP1.u.constant.val'] ]  =  unserialize(serialize($d));
        
    }
    
    
    
    
    
    
    function op_send_var($d) {
    
        $this->_make_opcode("OP1",$d);
         
        $this->_out2("
         
        	if (({$d['EXTENDED']} == ZEND_DO_FCALL_BY_NAME) && 
                    ARG_SHOULD_BE_SENT_BY_REF({$d['OP2.u.opline_num']}, execute_data.fbc , execute_data.fbc->common.arg_types)) {
                    zval **varptr_ptr;
                    zval *varptr;
                    varptr_ptr = get_zval_ptr_ptr(&op1,execute_data.Ts, BP_VAR_W);

                    if (!varptr_ptr) {
                        zend_error(E_ERROR, \"Only variables can be passed by reference\");
                    }

                    SEPARATE_ZVAL_TO_MAKE_IS_REF(varptr_ptr);
                    varptr = *varptr_ptr;
                    varptr->refcount++;
                    zend_ptr_stack_push(&EG(argument_stack), varptr);
            } else  {
                                zval *varptr;
                    varptr = get_zval_ptr(&op1, execute_data.Ts, &EG(free_op1), BP_VAR_R);

                    if (varptr == &EG(uninitialized_zval)) {
                        ALLOC_ZVAL(varptr);
                        INIT_ZVAL(*varptr);
                        varptr->refcount = 0;
                    } else if (PZVAL_IS_REF(varptr)) {
                        zval *original_var = varptr;

                        ALLOC_ZVAL(varptr);
                        *varptr = *original_var;
                        varptr->is_ref = 0;
                        varptr->refcount = 0;
                        zval_copy_ctor(varptr);
                    }
                    varptr->refcount++;
                    zend_ptr_stack_push(&EG(argument_stack), varptr);
                    FREE_OP(execute_data.Ts, op1, EG(free_op1));  
            }
        ");
        
    }
    function op_send_val($d) {
        $this->_make_opcode("OP1",$d);
        
       $this->_out2("
          
            if ({$d['EXTENDED']} == ZEND_DO_FCALL_BY_NAME
				&& ARG_SHOULD_BE_SENT_BY_REF({$d['OP2.u.opline_num']}, execute_data.fbc , execute_data.fbc->common.arg_types)) {
						zend_error(E_ERROR, \"Cannot pass parameter %d by reference\", {$d['OP2.u.opline_num']});
				}
				{
					zval *valptr;
					zval *value;

					value = get_zval_ptr(op1, execute_data.Ts,  &EG(free_op1), BP_VAR_R);

					ALLOC_ZVAL(valptr);
					*valptr = *value;
					if (!EG(free_op1)) {
						zval_copy_ctor(valptr);
					}
					INIT_PZVAL(valptr);
					zend_ptr_stack_push(&EG(argument_stack), valptr);
				}
        ");
    
    }
    /* ---------------NON OP CODES ------------*/
    var $_opcodes = array();
    var $_pass = 0;
    
    function _add_op($d) {
        if ($d['LINE'] != -1) {
            $this->_opcodes[$d['LINE']] = $d;
        }
        $this->_do_op($d);
    }
    function _do_op($data) {
        $this->_outComment($data);
        if (@$this->_gotos[$data['LINE']]) {
            $this->_out2("line_{$data['LINE']}:",FALSE);
        }
            
        $op = "op_".strtolower(trim($data['OPCODE_STR']));
        if (method_exists($this, $op)) {
            $this->$op($data);
        } else {
	   if (!$this->_pass) {
	   	$this->_out2("/* --TODO--*/");
	   }
	}
    }
    function _out($s,$do_cr=TRUE) {
        //if ($this->_pass != 0) {
            echo $s; 
	if ($do_cr) {
	    echo "\n";
	 }
        //}
    
    }    
    function _out2($data) {
         if (!$this->_pass) return;
        echo "$data\n";
    }
    function _outComment($data)  {
        $this->_out2("          /*".
            str_pad($data['LINE'],4) . ":" .
            //str_pad($data['OPCODE'],4) .
            str_pad($data['OPCODE_STR'],20) . ":".
            str_pad(@$data['RESULT'] ,15) . "= " . 
            str_pad(@$data['OP1'] ,15) . "," .
            str_pad(@$data['OP2'] ,15) . "*/"
        );
    }    
    function _pass_two() {
        echo "\n\n--------------------------------------------------\n\n";
        $this->_pass=1;
        //print_r($this->_opcodes);

    	global $classname;
        $this->op_opcode_array($this->init_opcode);
    	foreach($this->_opcodes as $data) {
            $this->_do_op($data);
        }
    	$this->_out("}");	
        echo "\n\n--------------------------------------------------\n\n";
    }
    function _set_zendvalue($zend,$real) {
        $this->zend_to_realname[$zend] = $real;
        $this->realname_to_zend[$real] = $zend;
    }
    function _get_zendvalue($zend) {
    	
        return @$this->zend_to_realname[$zend];
    }
    function _init_static_vars() {
        if ($this->_init_static_vars) {
            return;
        }
        $this->_out2("
            /* make sure static variables are initialized */
            if (!EG(active_op_array)->static_variables) {
                    ALLOC_HASHTABLE(EG(active_op_array)->static_variables);
                    zend_hash_init(EG(active_op_array)->static_variables, 2, NULL, ZVAL_PTR_DTOR, 0);
            }
        ");
        $this->_init_static_vars = TRUE;
    
    }
    function _get_zval_ptr($opcode, $d) {
         
        
        switch ($d[$opcode.".op_type"]) {
            case 1: //ZEND_IS_CONST:
                /* probably need to make a zval here */
                //echo "CONST?".$d[$opcode.".u.constant.val"];
                return array(0,"\"".$d[$opcode.".u.constant.val"]."\"");
            case 2: //ZEND_IS_TMP_VAR:
                return array(0,"(&execute_data.Ts[".$d[$opcode.".u.var"]."].tmp_var)"); 
            case 4: //ZEND_IS_VAR:
                //if (@$this->Ts[$d[$opcode.".u.var"]]["var.ptr"]) {
                    /*PZVAL_UNLOCK(Ts[node->u.var].var.ptr); */
                    return  array(0,"(execute_data.Ts[".$d[$opcode.".u.var"]."].var.ptr)");
                //}
               //print_r($d);
                //echo "\nUNHANDLED VAR : see get_zval_ptr\n";
                exit;
            default:
                echo "Unknown type ". $d[$opcode.".op_type"];
                exit;
            
        }
    }
    function _get_zval_ptr_ptr($opcode, $d) {
         
        return "execute_data.Ts[".((int) $d[$opcode.".u.var"]) ."].var.ptr_ptr";
         
    }
    function _fetch_var_address($d,$type) {
        // variable can be string or a 
        $this->_out2('',TRUE);
        list($free_op1, $varname ) = $this->_get_zval_ptr("OP1",$d);
         /* determine where to look for variable */
        //echo "GOT: :".$varname;
        
        switch ($d['OP2.u.fetch_type']) {
            case 1://ZEND_FETCH_LOCAL:
                $target_symbol_table = "EG(active_symbol_table)";
                break;
            case 0://ZEND_FETCH_GLOBAL:
                /* acutal code does some locking?
                if (opline->op1.op_type == IS_VAR) {
                    PZVAL_LOCK(varname);
                }*/
                $target_symbol_table = "EG(symbol_table)";
                break;
            case 2://ZEND_FETCH_STATIC:
                $this->_init_static_vars();
                $target_symbol_table = "EG(active_op_array)->static_variables";
                break;
        }
         
         
        /* 
        if (varname->type != IS_STRING) {
            tmp_varname = *varname;
            zval_copy_ctor(&tmp_varname);
            convert_to_string(&tmp_varname);
            varname = &tmp_varname;
        }
        */
        $varname_str = "{$varname}->value.str.val";
        $varname_len = "{$varname}->value.str.len+1";
        if ($varname{0} == '"') {
            $varname_str = $varname;
            $varname_len = strlen($varname) +1 ;
        }
        
        $this->_out2("        if (zend_hash_find($target_symbol_table, {$varname_str}, {$varname_len},  (void **) &execute_data.Ts[{$d['RESULT.u.var']}].var.ptr_ptr) == FAILURE) {",TRUE);
        
        switch($type) {
            case BP_VAR_R: 
                /*zend_error(E_NOTICE,"Undefined variable:  %s", varname->value.str.val); */
                /* break missing intentionally */
            case BP_VAR_IS:
                $this->_out2("        execute_data.Ts[{$d['RESULT.u.var']}].var.ptr_ptr = &EG(uninitialized_zval_ptr);",TRUE);
                break;
            case BP_VAR_RW:
                /*zend_error(E_NOTICE,"Undefined variable:  %s", varname->value.str.val); */
                /* break missing intentionally */
            case BP_VAR_W: {
                    $this->_out2("             zval *new_zval = &EG(uninitialized_zval);",TRUE);
                    $this->_out2("            new_zval->refcount++;",TRUE);
                    $this->_out2("            zend_hash_update({$target_symbol_table}, {$varname_str}, {$varname_len}, &new_zval, sizeof(zval *), (void **) &execute_data.Ts[{$d['RESULT.u.var']}].var.ptr_ptr);",TRUE);
                }
                break;
            
        }
        $this->_out2( "       }",TRUE);
        
        
        if ($d["OP2.u.fetch_type"] == 1) { //ZEND_FETCH_LOCAL
            //$this->_out( "FREE_OP(Ts, &opline->op1, free_op1); ");
        } else if ($d["OP2.u.fetch_type"] == 2) { //ZEND_FETCH_STATIC
            $this->_out2("       zval_update_constant(execute_data.Ts[{$d['RESULT.u.var']}].var.ptr_ptr, (void *) 1 TSRMLS_CC);");
        }
         
    }
    function _make_opcode($op,$d) {
    /*
        This is the opcode structure!!!
        
        znode.op_type
        // it could be any one of these.... - union is a optional structure thing
        znode.u.constant //zval
        znode.u.var
        znode.u.opline_num
        znode.u.op_array; // op_array ???
        znode.u.EA.var
        znode.u.EA.type
         
        zval.value // zvalue
        zval.type // uchar
        zval.is_ref
        zval.refcount
        
        // union!
        zvalue.lval
        zvalue.dval
        zvalue.str.val
        zvalue.str.len
        zvalue.ht
        zvalue.obj
    */
    
    
        // make some space available! - do this at the beginning...
        
        
        $opu= strtolower($op);
        
        $this->_out2("      {$opu}.op_type = ".$d["{$op}.op_type"].";");
        switch ($d["{$op}.op_type"]) {
                case 1: //ZEND_IS_CONST:
                $this->_out2("      {$opu}.u.constant = {$opu}_zv;");
                $this->_out2("      {$opu}.u.constant.type = ".$d["{$op}.u.constant.type"].";");
                $this->_out2("      {$opu}.u.constant.is_ref = ".$d["{$op}.u.constant.is_ref"].";");
                $this->_out2("      {$opu}.u.constant.refcount = ".$d["{$op}.u.constant.refcount"].";");
                $this->_out2("      {$opu}.u.constant.value = {$opu}_zvalue;");
                switch ($d["{$op}.u.constant.type"]) {
                    case 7://IS_RESOURCE:
                    case 6://IS_BOOL:
                    case 1://IS_LONG:
                        $this->_out2("      {$opu}.u.constant.value.lval = ".$d["{$op}.u.constant.val"].";");
                        break;
                    case 0://IS_NULL:
                        $this->_out2("      {$opu}.u.constant.value.lval = 0;");
                        /* null value, do nothing - although just do this anyway*/
                        break;
                    case 2://IS_DOUBLE:
                        $this->_out2("      {$opu}.u.constant.value.dval = ".$d["{$op}.u.constant.val"].";");
                        break;
                    case 8://IS_CONSTANT:
                    case 3://IS_STRING:
                    case 10://FLAG_IS_BC:
                        $this->_out2("      {$opu}.u.constant.value.str.val = \"".$d["{$op}.u.constant.val"]."\";");
                        $this->_out2("      {$opu}.u.constant.value.str.len = ".strlen($d["{$op}.u.constant.val"]).";");
                        break;
                    case 4://IS_ARRAY:
                    case 9://IS_CONSTANT_ARRAY:
                    case 5://IS_OBJECT:
                    default:
                        echo "UNHANDLED: op_type $op";exit;
                 } 
                 break;
            case 2: //ZEND_IS_TMP_VAR:
            case 4: //ZEND_IS_VAR:
                $this->_out2("      {$opu}.u.var = ".$d["{$op}.u.var"].";");
                break;  
            default:
                print_r($d);
                echo "Unknown type $op ". $d[$opcode.".op_type"];
                exit;
            
        }
    }
    
    function _assign_zval($name,$value) {
        switch(gettype($value)) {
            case "NULL";
                $this->_out2("      {$name}.type = 0;");
                $this->_out2("      {$name}.lval = 0;");
                break;
            case "boolean";
                $this->_out2("      {$name}.type = 6;");
                $this->_out2("      {$name}.lval = $value;");
                break;
            case "integer";
                $this->_out2("      {$name}.type = 1;");
                $this->_out2("      {$name}.lval = $value;");
                break;
            case "double";
                $this->_out2("      {$name}.type = 2;");
                $this->_out2("      {$name}.dval = $value;");
                break;
            case "string";
                $this->_out2("      {$name}.type = 3;");
                $this->_out2("      {$name}.val = \"$value\";");
                $this->_out2("      {$name}.len = ". (strlen($value) +1) .";");
                break;
            default:
                echo 'Unhandled zval type '. gettype($value) ." : $name : $value ";
                exit;
        }
    }
    
    function _do_fcall_common($d) {
        $this->_out2("
                {
                    zval **original_return_value;
					int return_value_used = (!({$d['RESULT.u.EA.type']} & EXT_TYPE_UNUSED));

					zend_ptr_stack_n_push(&EG(argument_stack), 2, (void *) {$d['EXTENDED']}, NULL);

					execute_data.Ts[{$d['RESULT.u.var']}].var.ptr_ptr = &execute_data.Ts[{$d['RESULT.u.var']}].var.ptr;

					if (execute_data.function_state.function->type==ZEND_INTERNAL_FUNCTION) {	
						ALLOC_ZVAL(execute_data.Ts[{$d['RESULT.u.var']}].var.ptr);
						INIT_ZVAL(*(execute_data.Ts[{$d['RESULT.u.var']}.var.ptr));
						((zend_internal_function *) execute_data.function_state.function->handler(
                                {$d['EXTENDED']}, execute_data.Ts[{$d['RESULT.u.var']}].var.ptr, execute_data.object.ptr, 
                                return_value_used TSRMLS_CC);
						if (execute_data.object.ptr) {
							execute_data.object.ptr->refcount--;
						}
						execute_data.Ts[{$d['RESULT.u.var']}].var.ptr->is_ref = 0;
						execute_data.Ts[{$d['RESULT.u.var']}].var.ptr->refcount = 1;
						if (!return_value_used) {
							zval_ptr_dtor(&execute_data.Ts[{$d['RESULT.u.var']}].var.ptr);
						}
					} else if (execute_data.function_state.function->type==ZEND_USER_FUNCTION) {
/* this is probably not needed as it should never call internal user functions! */
						HashTable *calling_symbol_table;

						execute_data.Ts[{$d['RESULT.u.var']}].var.ptr = NULL;
						if (EG(symtable_cache_ptr)>=EG(symtable_cache)) {
							 
							execute_data.function_state.function_symbol_table = *(EG(symtable_cache_ptr)--);
						} else {
							ALLOC_HASHTABLE(execute_data.function_state.function_symbol_table);
							zend_hash_init(execute_data.function_state.function_symbol_table, 0, NULL, ZVAL_PTR_DTOR, 0);
							 
						}
						calling_symbol_table = EG(active_symbol_table);
						EG(active_symbol_table) = execute_data.function_state.function_symbol_table;
						if ({$d['opcode']}==ZEND_DO_FCALL_BY_NAME
							&& execute_data.object.ptr
							&& execute_data.fbc->type!=ZEND_OVERLOADED_FUNCTION) {
							zval **this_ptr;
							zval *null_ptr = NULL;

							zend_hash_update(execute_data.function_state.function_symbol_table, \"this\", sizeof(\"this\"), &null_ptr, sizeof(zval *), (void **) &this_ptr);
							if (!PZVAL_IS_REF(execute_data.object.ptr)) {
								zend_error(E_WARNING, \"Problem with method call - please report this bug\");
                			}
							*this_ptr = execute_data.object.ptr;
							execute_data.object.ptr = NULL;
						}
						original_return_value = EG(return_value_ptr_ptr);
						EG(return_value_ptr_ptr) = execute_data.Ts[{$d['RESULT.u.var']}].var.ptr_ptr;
						EG(active_op_array) = (zend_op_array *) execute_data.function_state.function;

						zend_execute(EG(active_op_array) TSRMLS_CC);

						if (return_value_used && !execute_data.Ts[{$d['RESULT.u.var']}].var.ptr) {
							ALLOC_ZVAL(execute_data.Ts[{$d['RESULT.u.var']}].var.ptr);
							INIT_ZVAL(*execute_data.Ts[{$d['RESULT.u.var']}].var.ptr);
						} else if (!return_value_used && execute_data.Ts[{$d['RESULT.u.var']}].var.ptr) {
							zval_ptr_dtor(&execute_data.Ts[{$d['RESULT.u.var']}].var.ptr);
						}
						/*EG(opline_ptr) = &EX(opline);
						EG(active_op_array) = op_array;
                            */
						EG(return_value_ptr_ptr)=original_return_value;
						if (EG(symtable_cache_ptr)>=EG(symtable_cache_limit)) {
							zend_hash_destroy(execute_data.function_state.function_symbol_table);
							FREE_HASHTABLE(execute_data.function_state.function_symbol_table);
						} else {
							*(++EG(symtable_cache_ptr)) = execute_data.function_state.function_symbol_table;
							zend_hash_clean(*EG(symtable_cache_ptr));
						}
						EG(active_symbol_table) = calling_symbol_table;
					} else { /* ZEND_OVERLOADED_FUNCTION */
						ALLOC_ZVAL(execute_data.Ts[{$d['RESULT.u.var']}].var.ptr);
						INIT_ZVAL(*(execute_data.Ts[{$d['RESULT.u.var']}].var.ptr));
						call_overloaded_function(&execute_data.Ts[execute_data.fbc->overloaded_function.var], {$d['EXTENDED']}, execute_data.Ts[{$d['RESULT.u.var']}].var.ptr TSRMLS_CC);
						efree(execute_data.fbc);
						if (!return_value_used) {
							zval_ptr_dtor(&execute_data.Ts[{$d['RESULT.u.var']}].var.ptr);
						}
					}
					if ({$d['opcode']}  == ZEND_DO_FCALL_BY_NAME) {
						zend_ptr_stack_n_pop(&EG(arg_types_stack), 2, &execute_data.object.ptr, &execute_data.fbc);
					} else {
						execute_data.object.ptr = zend_ptr_stack_pop(&EG(arg_types_stack));
					}
					execute_data.function_state.function = base_function;
					EG(function_state_ptr) = &execute_data.function_state;
					zend_ptr_stack_clear_multiple(TSRMLS_C);
				}
            }
        ");
    
    
    
    
    }
  
          

}





?>
