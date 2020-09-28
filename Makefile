all: compile
compile:
	gcc sary.c -o sary
execute:
	./sary
clean:
	rm sary