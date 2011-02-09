CFLAGS=-Wall -g

bin/vcslog-wrapper: bin/xquotestr.o wrapper.c
	gcc $(CFLAGS) $^ -o $@

bin/xquotestr.o: xquotestr.c xquotestr.h
	gcc $(CFLAGS) -c $< -o $@

bin/test-xquotestr: bin/xquotestr.o test-xquotestr.c
	gcc $(CFLAGS) $^ -o $@

test: bin/test-xquotestr test.py
	./test.py

clean:
	rm -r bin/*
