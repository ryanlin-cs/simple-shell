default: build

check: clean build run

run: myshell
	./myshell

gdb: myshell
	gdb --args myshell

build: myshell 

myshell: shell.c
	gcc -g -O0 -o myshell shell.c

clean:
	rm -f myshell a.out *~

dist:
	dir=`basename $$PWD`; cd ..; tar cvf $$dir.tar ./$$dir; gzip $$dir.tar
	dir=`basename $$PWD`; ls -l ../$$dir.tar.gz
