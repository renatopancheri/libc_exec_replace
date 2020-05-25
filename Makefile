all: myexec.c 
	wget -qO- http://github.com/pymumu/tinylog/archive/v1.3.tar.gz|tar -xzf -
	gcc -shared -ldl -lpthread -fPIC myexec.c tinylog-1.3/tlog.c -Itinylog-1.3 -o myexec_verbose.so -DVERBOSE
	gcc -shared -ldl -lpthread -fPIC myexec.c tinylog-1.3/tlog.c -Itinylog-1.3 -o myexec.so
clean:
	rm -rf *.so tinylog-1.3
	
