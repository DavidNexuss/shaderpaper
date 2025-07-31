shaderpaper: src/main.c bin/stb_image.o bin/glad.o
	gcc -g src/main.c bin/stb_image.o bin/glad.o -o shaderpaper -lGL -lGLEW -lX11 -lm

bin/stb_image.o: src/stb_image.c
	gcc -O3 src/stb_image.c -c -o bin/stb_image.o

bin/glad.o: src/glad.c
	gcc -O3 src/glad.c -c -o bin/glad.o

clean:
	rm -rf shaderpaper bin/*.o
