cc=clang
cflags=-g -std=gnu99 -Wall -Werror -Wextra -pedantic
ldflags=-lpthread -lrt
file=proj2
all: $(file).o
	$(cc) $< -o $(file) $(ldflags)

$(file).o: $(file).c
	$(cc) -c $(cflags) $< -o $(file).o

.PHONY clean:
clean:
	rm $(file) $(file).o
