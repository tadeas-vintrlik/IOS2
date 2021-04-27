cc=gcc
cflags=-std=gnu99 -Wall -Werror -Wextra -pedantic
ldflags=-lpthread -lrt
file=proj2
test=test.sh
all: $(file).o
	$(cc) $< -o $(file) $(ldflags)

$(file).o: $(file).c
	$(cc) -c $(cflags) $< -o $(file).o

.PHONY clean test zip:
clean:
	rm $(file) $(file).o
test:
	./$(test)

zip: $(file).c Makefile
	zip -r $(file).zip $(file).c Makefile
