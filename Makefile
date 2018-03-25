all : a

a : Ass3b_42.c
	gcc -std=c99 -g -o a Ass3b_42.c -pthread

clean :
	rm -f a ./*~
