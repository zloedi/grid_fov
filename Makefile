demo.exe: demo.c grid_fov.h
	x86_64-w64-mingw32-gcc -o demo.exe demo.c -O0 -g -Wall -Wextra -Wshadow -mconsole -I/usr/local/x86_64-w64-mingw32/include -Dmain=SDL_main `sdl2-config --static-libs`
