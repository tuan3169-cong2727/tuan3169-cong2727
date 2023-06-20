include ../Makefile.kernel
myprogram.o: myprogram.c
	$(CC) $(CFLAGS) -c myprogram.c -o myprogram.o
myprogram: myprogram.o
	$(LD) $(LDFLAGS) myprogram.o $(LDLIBS) -o myprogram
clean:
	rm -f myprogram.o myprogram

