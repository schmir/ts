CC = gcc -O2 -Wall

bindir = /usr/local/bin

ts: ts.c
	$(CC) ts.c -o ts 

install: ts
	install -m 755 -d $(bindir)
	install -m 755 -p -s ts $(bindir)

clean::
	rm -f ts *~

