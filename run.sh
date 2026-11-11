clang main.c $(pkg-config sdl3 --cflags --libs) -lm -o 0pong
./0pong
rm 0pong

