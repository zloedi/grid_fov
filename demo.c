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

static SDL_Renderer *x_renderer;
static SDL_Window *x_window;
static int x_mouseX, x_mouseY;
static unsigned char x_maze[MAZE_W * MAZE_H];
static SDL_Texture *x_mazeTex;

//static inline int Mini( int a, int b ) {
//    return a < b ? a : b;
//}
//
//static inline int Maxi( int a, int b ) {
//    return a > b ? a : b;
//}

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
        //int code = event.key.keysym.sym;
        switch( event.type ) {
			case SDL_MOUSEMOTION:
				x_mouseX = event.motion.x;
                x_mouseY = event.motion.y;
				break;

            case SDL_MOUSEBUTTONDOWN:
                //ZH_UI_OnMouseButton( 1 );
                break;

            case SDL_MOUSEBUTTONUP:
                //ZH_UI_OnMouseButton( 0 );
                break;

            case SDL_QUIT:
                *quit = 1;
                break;
            default:
                break;
        }
    }
    SDL_SetRenderDrawColor( x_renderer, 40, 45, 50, 255 );
    SDL_RenderClear( x_renderer );
    //ZH_UI_Begin( x_mouseX, x_mouseY );
    //if ( ZH_UI_ClickRect( 10, 10, 50, 20 ) == UIBR_RELEASED ) {
    //    QON_Print( "Clicked\n" );
    //}
    //ZH_UI_End();

    // == draw the maze ==

    SDL_SetTextureAlphaMod( x_mazeTex, 0xff );
    SDL_SetTextureBlendMode( x_mazeTex, SDL_BLENDMODE_BLEND );
    SDL_SetTextureColorMod( x_mazeTex, 0x00, 0x90, 0x00 );
    SDL_Rect dst = { 0, 0, MAZE_W * 5, MAZE_H * 5 };
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
    SDL_CreateWindowAndRenderer( 640, 768, flags, &x_window, &x_renderer );
#else
    flags = SDL_WINDOW_RESIZABLE;
    SDL_CreateWindowAndRenderer( 1024, 768, flags, &x_window, &x_renderer );
#endif
    
    // == generate maze ==

    {
        int sz = MAZE_W * MAZE_H;
        //memset( x_maze, 0, sz );
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
            RasterizeRectangle( rx, ry, rw, rh, 255, MAZE_W, MAZE_H, x_maze );
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
        static unsigned char bytes[MAZE_W * MAZE_H * 4];
        for ( int i = 0; i < MAZE_W * MAZE_H; i++ ) {
            bytes[i * 4 + 0] = 0xff;
            bytes[i * 4 + 1] = 0xff;
            bytes[i * 4 + 2] = 0xff;
            bytes[i * 4 + 3] = x_maze[i];
        }
        SDL_UpdateTexture( x_mazeTex, NULL, bytes, MAZE_W * 4 );
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
