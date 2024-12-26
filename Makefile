all: 
	gcc -g parser.c main.c shell.c -o shell

clean:
	rm -f *.o
	rm -f shell
	rm -f *.txt

