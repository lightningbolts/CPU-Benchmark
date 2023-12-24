# Makefile
prime: prime.c
	g++ -o prime prime.c $(pkg-config --cflags --libs libmongoc-1.0)

clean:
	rm -f prime
