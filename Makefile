all: myexec.c 
	wget -qO- http://github.com/pymumu/tinylog/archive/v1.3.tar.gz|tar -xzf -
	gcc -O2 -fPIC -c myexec.c -Itinylog-1.3 -o myexec_verbose.o -DVERBOSE
	gcc -O2 -fPIC -c myexec.c -Itinylog-1.3 -o myexec.o
	gcc -O2 -fPIC -c tinylog-1.3/tlog.c -o tlog.o
	gcc -fPIC -shared myexec.o tlog.o -lpcre2-8 -ldl -lpthread -o myexec.so
	gcc -fPIC -shared myexec_verbose.o tlog.o -lpcre2-8 -ldl -lpthread -o myexec_verbose.so
clean:
	rm -rf *.so tinylog-1.3 *.o
	
