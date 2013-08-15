all: genplot

test: dotplot
	gcc -o plottest lib/dotplot.o lib/list/src/iterator.o lib/list/src/list.o lib/list/src/node.o test.c -lgd -Llib/list/build/liblist.a

genplot: dotplot generate_dotplot.c
	gcc -o genplot lib/dotplot.o lib/list/src/iterator.o lib/list/src/list.o lib/list/src/node.o generate_dotplot.c -lgd -Llib/list/build/liblist.a

dotplot: list lib/dotplot.h lib/dotplot.c
	cd lib; gcc -c dotplot.c -lgd -Llist/build/liblist.a

list: lib/list/src/list.h lib/list/src/list.c lib/list/src/iterator.c lib/list/src/node.c
	cd lib/list; make
	
clean:
	find . -name *.o -print | xargs rm; rm genplot* plottest*
