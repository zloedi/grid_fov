demo.exe: demo.c grid_fov.h
	x86_64-w64-mingw32-gcc -o demo.exe demo.c -O3 -Wall -Wextra -Wshadow -mconsole -I/usr/local/x86_64-w64-mingw32/include -Dmain=SDL_main `sdl2-config --static-libs`

demo_dbg.exe: demo.c grid_fov.h
	x86_64-w64-mingw32-gcc -o demo_dbg.exe demo.c -O0 -g -Wall -Wextra -Wshadow -mconsole -I/usr/local/x86_64-w64-mingw32/include -Dmain=SDL_main `sdl2-config --static-libs`

demo_emscripten: demo.c grid_fov.h
	emcc demo.c -s WASM=1 -s USE_SDL=2 -O3 -o index.js
