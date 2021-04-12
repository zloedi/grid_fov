// Copyright (c) 2021 Stoiko Todorov
// This work is licensed under the terms of the MIT license.  
// For a copy, see https://opensource.org/licenses/MIT.

#include <SDL2/SDL.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "grid_fov.h"

static SDL_Renderer *x_renderer;
static SDL_Window *x_window;
static int x_mouseX, x_mouseY;

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
