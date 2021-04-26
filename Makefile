cc=gcc
cflags=-g -std=gnu99 -Wall -Werror -Wextra -pedantic
ldflags=-lpthread -lrt
file=proj2
test=test.sh
all: $(file).o
	$(cc) $< -o $(file) $(ldflags)

$(file).o: $(file).c
	$(cc) -c $(cflags) $< -o $(file).o

.PHONY clean test:
clean:
	rm $(file) $(file).o
test:
	./$(test)
