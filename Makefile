shaderpaper: src/main.c bin/stb_image.o bin/glad.o bin/parser.o
	gcc -g src/main.c bin/stb_image.o bin/glad.o bin/parser.o -o shaderpaper -lGL -lGLEW -lX11 -lm -lstdc++ -static-libstdc++ -static-libgcc

bin/stb_image.o: src/stb_image.c
	gcc -O3 src/stb_image.c -c -o bin/stb_image.o

bin/glad.o: src/glad.c
	gcc -O3 src/glad.c -c -o bin/glad.o

bin/parser.o: src/parser.cpp
	g++ -O3 -static-libstdc++ -static-libgcc -c src/parser.cpp -o bin/parser.o

clean:
	rm -rf shaderpaper bin/*.o
