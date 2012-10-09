#unix make file - based on edins example


#CHANGE ME!
PHP_BASE=/usr/src/php/php-4.3.2RC1

phpe: phpe.lo
	${PHP_BASE}/libtool --mode=link gcc -pipe -g -O2 -L${PHP_BASE}/TSRM -L${PHP_BASE}/Zend -L${PHP_BASE}/sapi/embed ${PHP_BASE}/libs/libphp4.so phpe.lo -o phpe 

phpe.lo: phpe.c 
	${PHP_BASE}/libtool --mode=compile gcc -pipe -g -O2 -I${PHP_BASE} -I${PHP_BASE}/main -I${PHP_BASE}/Zend -I${PHP_BASE}/TSRM -I${PHP_BASE}/sapi/embed -c phpe.c


clean:
	rm -f *.o *.lo phpe 
