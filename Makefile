CC=gcc
CFLAGS=-I/usr/include/GL -D_GNU_SOURCE -DPTHREADS -g
LFLAGS=-lGL -lGLEW -lGLU -lGL -lm -lX11 -lXext -g

glxgears: glxgears.o
	$(CC) -o $@ $^ $(CFLAGS) $(LFLAGS)

glxgears.o: glxgears.c
	$(CC) -c -o $@ $< $(CFLAGS) -MT $< -MD -MP -MF glxgears.Tpo

clean:
	rm *.o
	rm glxgears
