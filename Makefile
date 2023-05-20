CC = gcc
FLAGS = -Wall -Wextra -Wpedantic -std=c11 -D_GNU_SOURCE -g

ex13: main.o param_info.o helper.o
	$(CC) $^ -o $@

main.o: main.c
	$(CC) $(FLAGS) -c $^ -o $@

param_info.o: param_info.c
	$(CC) $(FLAGS) -c $^ -o $@

helper.o: helper.c
	$(CC) $(FLAGS) -c $^ -o $@

clean:
	rm -f ex13 *.o 
