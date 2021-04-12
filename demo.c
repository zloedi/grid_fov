// Copyright (c) 2021 Stoiko Todorov
// This work is licensed under the terms of the MIT license.  
// For a copy, see https://opensource.org/licenses/MIT.

#include <SDL2/SDL.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "grid_fov.h"

#define MAZE_W 200
#define MAZE_H 150
#define TEX_SCALE 5

static SDL_Renderer *x_renderer;
static SDL_Window *x_window;
static int x_mouseX, x_mouseY;
static unsigned char x_maze[MAZE_W * MAZE_H];
static unsigned char x_fov[MAZE_W * MAZE_H];
static SDL_Texture *x_mazeTex;

static void RasterizeRectangle( int rx, int ry, int rw, int rh, int color,
                                                        int mazeW, int mazeH, 
                                                        unsigned char *maze ) {
    int minX = Clampi( rx, 0, mazeW - 1 );
    int maxX = Clampi( rx + rw, 0, mazeW - 1 );
    int minY = Clampi( ry, 0, mazeH - 1 );
    int maxY = Clampi( ry + rh, 0, mazeH - 1 );
    for ( int y = minY; y <= maxY; y++ ) {
        for ( int x = minX; x <= maxX; x++ ) {
            maze[x + y * mazeW] = color;
        }
    }
}

void MainLoop( void *arg ) {
    int *quit = arg;
    SDL_Event event;
    while ( SDL_PollEvent( &event ) ) {
        switch( event.type ) {
			case SDL_MOUSEMOTION:
				x_mouseX = event.motion.x;
                x_mouseY = event.motion.y;
				break;

            case SDL_QUIT:
                *quit = 1;
                break;

            default:
                break;
        }
    }
    SDL_SetRenderDrawColor( x_renderer, 40, 40, 40, 255 );
    SDL_RenderClear( x_renderer );

    // == rasterize Field of View ==

    int curX = Clampi( x_mouseX / TEX_SCALE, 0, MAZE_W - 1 );
    int curY = Clampi( x_mouseY / TEX_SCALE, 0, MAZE_H - 1 );
    memset( x_fov, 0, sizeof( x_fov ) );
    for ( int i = 0; i < 8; i++ ) {
        RasterizeFOVOctant( curX, curY, MAZE_W / 4, MAZE_W, MAZE_H, i, 0, 0, 0,
                                                             x_maze, x_fov );
    }

    // == update the texture ==

    void *p;
    int pitch;
    SDL_LockTexture( x_mazeTex, NULL, &p, &pitch );
    unsigned char *pixels = p;
    for ( int i = 0; i < MAZE_W * MAZE_H; i++ ) {
        int mz = x_maze[i] >> 1;
        int fov = x_fov[i] >> 1;
        pixels[i * 4 + 0] = fov;
        pixels[i * 4 + 1] = fov + mz;
        pixels[i * 4 + 2] = fov;
        pixels[i * 4 + 3] = 0xff;
    }
    pixels[curX * 4 + 0 + curY * pitch] = 0xff;
    pixels[curX * 4 + 1 + curY * pitch] = 0xff;
    pixels[curX * 4 + 2 + curY * pitch] = 0xff;
    pixels[curX * 4 + 3 + curY * pitch] = 0xff;
    SDL_UnlockTexture( x_mazeTex );

    // == draw the texture ==

    SDL_SetTextureAlphaMod( x_mazeTex, 0xff );
    SDL_SetTextureBlendMode( x_mazeTex, SDL_BLENDMODE_BLEND );
    SDL_SetTextureColorMod( x_mazeTex, 0xff, 0xff, 0xff );
    SDL_Rect dst = { 0, 0, MAZE_W * TEX_SCALE, MAZE_H * TEX_SCALE };
    SDL_RenderCopy( x_renderer, x_mazeTex, NULL, &dst );

    SDL_RenderPresent( x_renderer );
    SDL_Delay( 33 );
}

int main( int argc, char *argv[] ) {
    // shut up gcc
    (void)argc;
    (void)argv;

    SDL_Init( SDL_INIT_VIDEO );
    int flags;
#ifdef __EMSCRIPTEN__
    flags = 0;
    SDL_CreateWindowAndRenderer( TEX_SCALE * MAZE_W, TEX_SCALE * MAZE_H, flags, &x_window, &x_renderer );
#else
    flags = SDL_WINDOW_RESIZABLE;
    SDL_CreateWindowAndRenderer( TEX_SCALE * MAZE_W, TEX_SCALE * MAZE_H, flags, &x_window, &x_renderer );
#endif
    
    // == generate maze ==

    {
        int sz = MAZE_W * MAZE_H;
        int numPixels = sz / 100;
        int numRects = sz / 50;
        int minRectSide = Maxi( Mini( MAZE_W, MAZE_H ) / 64, 1 );
        int maxRectSide = minRectSide * 8;
        for ( int i = 0; i < numPixels; i++ ) {
            int x = rand() % MAZE_W;
            int y = rand() % MAZE_H;
            x_maze[x + y * MAZE_W] = 0xff;
        }
        for ( int i = 0; i < numRects; i++ ) {
            int rx = rand() % MAZE_W;
            int ry = rand() % MAZE_H;
            int rw = minRectSide + rand() % ( maxRectSide - minRectSide );
            int rh = minRectSide + rand() % ( maxRectSide - minRectSide );
            RasterizeRectangle( rx, ry, rw, rh, 0xff, MAZE_W, MAZE_H, x_maze );
        }
        for ( int i = 0; i < numRects / 2; i++ ) {
            int rx = rand() % MAZE_W;
            int ry = rand() % MAZE_H;
            int rw = minRectSide + rand() % ( maxRectSide - minRectSide );
            int rh = minRectSide + rand() % ( maxRectSide - minRectSide );
            RasterizeRectangle( rx, ry, rw, rh, 0, MAZE_W, MAZE_H, x_maze );
        }
        SDL_SetHint( SDL_HINT_RENDER_SCALE_QUALITY, "0" );
        x_mazeTex = SDL_CreateTexture( x_renderer, SDL_PIXELFORMAT_ABGR8888, 
                                SDL_TEXTUREACCESS_STREAMING, MAZE_W, MAZE_H );
    }

    int quit = 0;
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg( MainLoop, &quit, -1, 1 );
#else
    SDL_SetWindowTitle( x_window, "Grid Field of View" );
    while ( 1 ) {
        MainLoop( &quit );
        if ( quit ) {
            break;
        }
    }
#endif
    SDL_DestroyRenderer( x_renderer );
    SDL_DestroyWindow( x_window );
    SDL_Quit();
    return 0;
}
