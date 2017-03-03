CC=c++
#CFLAGS=-I. -g -std=c++11 -Wall
CFLAGS=-I. -g -Wall
DEPS = config.h
OBJ = main.o

%.o: %.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

nginx-server-names: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f *.o nginx-server-names

install:
	install ./nginx-server-names /usr/local/bin/
