/*
   What this function does:
      Rasterizes a single Field Of View octant on a grid, similar to the way 
      FOV / shadowcasting is implemented in some roguelikes.
      Uses rays to define visible volumes instead of tracing lines from origin to pixels.
      Minimal processing per pixel (each pixel is hit only once most of the time).
      Clips to bitmap
      Symmetrical / Steps on pixel centers
      Optional attenuation
      Optional circle clip
      Optional lit blocking tiles

   To rasterize the entire FOV, call this in a loop with octant in range 0-7
   Inspired by https://docs.microsoft.com/en-us/archive/blogs/ericlippert/shadowcasting-in-c-part-one

   See the result here: 
      https://youtu.be/lIlPfwlcbHo
*/

static inline int Mini( int a, int b ) {
    return a < b ? a : b;
}

static inline int Maxi( int a, int b ) {
    return a > b ? a : b;
}

static inline int Clampi( int v, int min, int max ) {
    return Maxi( min, Mini( v, max ) );
}

typedef union c2_s {
    struct {
        int x, y;
    };
    int a[2];
} c2_t;

static const c2_t c2zero = { .a = { 0, 0 } };
static const c2_t c2one = { .a = { 1, 1 } };

static inline c2_t c2xy( int x, int y ) {
    c2_t c = { { x, y } };
    return c;
}

static inline c2_t c2Neg( c2_t c ) {
    return c2xy( -c.x, -c.y );
}

static inline c2_t c2Add( c2_t a, c2_t b ) {
    return c2xy( a.x + b.x, a.y + b.y );
}

static inline c2_t c2Sub( c2_t a, c2_t b ) {
    return c2xy( a.x - b.x, a.y - b.y );
}

static inline int c2Dot( c2_t a, c2_t b ) {
    return a.x * b.x + a.y * b.y;
}

static inline int c2Cross( c2_t a, c2_t b ) {
    return a.x * b.y - a.y * b.x;
}

static inline c2_t c2Clamp( c2_t c, c2_t min, c2_t max ) {
    return c2xy( Clampi( c.x, min.x, max.x ), Clampi( c.y, min.y, max.y ) );
}

static inline c2_t c2Scale( c2_t a, int s ) {
    return c2xy( a.x * s, a.y * s );
}

void RasterizeFOVOctant( int originX, int originY,
                         int radius, 
                         int bitmapWidth, int bitmapHeight,
                         int octant,
                         int skipAttenuation,
                         int skipClampToRadius,
                         int darkWalls,
                         const unsigned char *inBitmap, 
                         unsigned char *outBitmap ) {
#define READ_PIXEL(c) inBitmap[(c).x+(c).y*bitmapWidth]
#define WRITE_PIXEL(c,color) outBitmap[(c).x+(c).y*bitmapWidth]=(color)
#define MAX_RAYS 64
#define ADD_RAY(c) {nextRays->rays[Mini(nextRays->numRays,MAX_RAYS-1)] = (c);nextRays->numRays++;}
#define IS_ON_MAP(c) ((c).x >= 0 && (c).x < bitmapWidth && (c).y >= 0 && (c).y < bitmapHeight)
    typedef struct {
        int numRays;
        c2_t rays[MAX_RAYS];
    } raysList_t;
    // keep these coupled like this
    static const c2_t bases[] = {
        { { 1, 0  } }, { { 0, 1  } },
        { { 1, 0  } }, { { 0, -1 } },
        { { -1, 0 } }, { { 0, -1 } },
        { { -1, 0 } }, { { 0, 1  } },
        { { 0, 1  } }, { { -1, 0 } },
        { { 0, 1  } }, { { 1, 0  } },
        { { 0, -1 } }, { { 1, 0  } },
        { { 0, -1 } }, { { -1, 0 } },
    }; 
    c2_t e0 = bases[( octant * 2 + 0 ) & 15];
    c2_t e1 = bases[( octant * 2 + 1 ) & 15];
    raysList_t rayLists[2] = { {
        .numRays = 2,
        .rays = {
            c2xy( 1, 0 ),
            c2xy( 1, 1 ),
        }, 
    } };

    // quit if origin is on a solid pixel
    c2_t bitmapSize = c2xy( bitmapWidth, bitmapHeight );
    c2_t bitmapMax = c2Sub( bitmapSize, c2one );
    c2_t origin = c2Clamp( c2xy( originX, originY ), c2zero, bitmapMax );
    if ( READ_PIXEL( origin ) ) {
        WRITE_PIXEL( origin, 255 );
        return;
    }

    // clamp to map size
    c2_t dmin = c2Neg( origin );
    c2_t dmax = c2Sub( bitmapMax, origin );
    int dmin0 = c2Dot( dmin, e0 );
    int dmax0 = c2Dot( dmax, e0 );
    int limitX = Mini( radius, dmin0 > 0 ? dmin0 : dmax0 );
    limitX = Maxi( limitX, 0 );
    int dmin1 = c2Dot( dmin, e1 );
    int dmax1 = c2Dot( dmax, e1 );
    int limitY = Mini( radius, dmin1 > 0 ? dmin1 : dmax1 );
    limitY = Maxi( limitY, 0 );

    {
        // go over all 'columns'
        int column;
        c2_t ci;
        for ( column = 0, ci = origin; column <= limitX; 
                                            column++, ci = c2Add( ci, e0 ) ) {
            // work in half-pixel units
            int i2 = column << 1;

            // the rays list is recycled between the columns
            // we recreate the rays list on each column
            raysList_t *currRays = &rayLists[( column + 0 ) & 1];
            raysList_t *nextRays = &rayLists[( column + 1 ) & 1];
            nextRays->numRays = 0;

            // for all ray pairs...
            for ( int r = 0; r < currRays->numRays - 1; r += 2 ) {
                // a pair of rays defining a frustum
                c2_t ray0 = currRays->rays[r + 0];
                c2_t ray1 = currRays->rays[r + 1];

                // the Y coordinate of the intersection of the TOP ray with the PREVIOUS column
                int inyr0 = ( i2 - 1 ) * ray0.y / ray0.x;
                // the Y coordinate of the intersection of the TOP ray with the CURRENT column
                int outyr0 = ( i2 + 1 ) * ray0.y / ray0.x;

                // the Y coordinate of the intersection of the BOTTOM ray with the PREVIOUS column
                int inyr1 = ( i2 - 1 ) * ray1.y / ray1.x;
                // the Y coordinate of the intersection of the BOTTOM ray with the CURRENT column
                int outyr1 = ( i2 + 1 ) * ray1.y / ray1.x;

                // == light up a chunk of pixels inside the frustum ==

                {
                    int starty = ( outyr0 + 1 ) >> 1;
                    int endy = ( ( outyr1 + 1 ) >> 1 ) - 1;

                    int y;
                    c2_t p;
                    int miny = starty;
                    int maxy = Mini( endy, limitY ); 
                    c2_t start = c2Add( ci, c2Scale( e1, starty ) );
                    for ( y = miny, p = start; y <= maxy; 
                                                    y++, p = c2Add( p, e1 ) ) {
                        WRITE_PIXEL( p, 255 );
                    }
                }

                // == push the rays closer to each other if they hit solid pixels ==

                c2_t newRay0;
                c2_t newRay1;

                {
                    // if nothing is blocking the top ray -- keep it (no pinning to pixel corners)
                    int iny = Mini( ( inyr0 + 1 ) >> 1, limitY );
                    int outy = Mini( ( outyr0 + 1 ) >> 1, limitY );
                    c2_t in = c2Add( ci, c2Scale( e1, iny ) );
                    c2_t out = c2Add( ci, c2Scale( e1, outy ) );
                    if ( ! READ_PIXEL( in ) && ! READ_PIXEL( out ) ) {
                        newRay0 = ray0;
                    } 
                    // if blocked, walk along the solid pixels toward the 
                    // bottom ray until we find a hole, then pin the ray there
                    else {
                        int top = outy;
                        int bottom = Mini( ( inyr1 + 1 ) >> 1, limitY );
                        int y;
                        c2_t p = c2Add( ci, c2Scale( e1, top ) );
                        for ( y = top * 2; y <= bottom * 2; 
                                                y += 2, p = c2Add( p, e1 ) ) {
                            if ( ! READ_PIXEL( p ) ) {
                                break;
                            }
                            // pixels that push rays closer are lit too
                            WRITE_PIXEL( p, 255 );
                        }
                        newRay0 = c2xy( i2 - 1, y - 1 );
                        outyr0 = ( i2 + 1 ) * newRay0.y / newRay0.x;
                        // the bounding rays form a zero-area frustum
                        // stop processing this ray pair
                        if ( c2Cross( newRay0, ray1 ) <= 0 ) {
                            continue;
                        }
                    }
                }

                {
                    // if nothing is blocking the bottom ray -- keep it (no pinning to pixel corners)
                    int iny = Mini( ( inyr1 + 1 ) >> 1, limitY );
                    int outy = Mini( ( outyr1 + 1 ) >> 1, limitY );
                    c2_t in = c2Add( ci, c2Scale( e1, iny ) );
                    c2_t out = c2Add( ci, c2Scale( e1, outy ) );
                    if ( ! READ_PIXEL( in ) && ! READ_PIXEL( out ) ) {
                        newRay1 = ray1;
                    } 
                    // if blocked, walk along the solid pixels toward the 
                    // bottom ray until we find a hole, then pin the ray there
                    else {
                        int top = Mini( ( outyr0 + 1 ) >> 1, limitY );
                        int bottom = Mini( ( inyr1 + 1 ) >> 1, limitY );
                        int y;
                        c2_t p = c2Add( ci, c2Scale( e1, bottom ) );
                        for ( y = bottom * 2; y >= top * 2; 
                                                y -= 2, p = c2Sub( p, e1 ) ) {
                            if ( ! READ_PIXEL( p ) ) {
                                break;
                            }
                            // pixels that push rays closer are lit too
                            WRITE_PIXEL( p, 255 );
                        }
                        newRay1 = c2xy( i2 + 1, y + 1 );
                        inyr1 = ( i2 - 1 ) * newRay1.y / newRay1.x;
                        // the bounding rays form a zero-area frustum
                        // stop processing this ray pair
                        if ( c2Cross( newRay0, newRay1 ) <= 0 ) {
                            continue;
                        }
                    }
                }

                // == collect any valid rays for the next column ==

                {
                    // push top ray
                    ADD_RAY( newRay0 );

                    int top = ( outyr0 + 1 ) / 2;
                    int bottom = Mini( ( inyr1 + 1 ) / 2, limitY );
                    c2_t p = c2Add( ci, c2Scale( e1, top ) );
                    int prevPixel = READ_PIXEL( p );
                    for ( int y = top * 2; y <= bottom * 2; y += 2, p = c2Add( p, e1 ) ) {
                        int pixel = READ_PIXEL( p );
                        if ( prevPixel != pixel ) {
                            c2_t ray;
                            if ( pixel ) {
                                ray = c2xy( i2 + 1, y - 1 );
                            } else {
                                ray = c2xy( i2 - 1, y - 1 );
                            }

                            // create / push any new rays inbetween
                            ADD_RAY( ray );
                        }
                        prevPixel = pixel;
                    }

                    // push bottom ray
                    ADD_RAY( newRay1 );
                }
            }
        }
    }

    if ( ! skipAttenuation ) {
        c2_t ci = origin;
        int rsq = radius * radius;
        for ( int i = 0; i <= limitX; i++ ) {
            c2_t p = ci;
            for ( int j = 0; j <= limitY; j++ ) {
                c2_t d = c2Sub( p, origin );
                int dsq = c2Dot( d, d );
                int mod = 255 - Mini( dsq * 255 / rsq, 255 );
                int lit = !! outBitmap[p.x + p.y * bitmapWidth];
                WRITE_PIXEL( p, mod * lit );
                p = c2Add( p, e1 );
            }
            ci = c2Add( ci, e0 );
        }
    } else if ( ! skipClampToRadius ) {
        c2_t ci = origin;
        int rsq = radius * radius;
        for ( int i = 0; i <= limitX; i++ ) {
            c2_t p = ci;
            for ( int j = 0; j <= limitY; j++ ) {
                c2_t d = c2Sub( p, origin );
                if ( c2Dot( d, d ) > rsq ) { 
                    WRITE_PIXEL( p, 0 );
                }
                p = c2Add( p, e1 );
            }
            ci = c2Add( ci, e0 );
        }
    }

    if ( darkWalls ) {
        c2_t ci = origin;
        for ( int i = 0; i <= limitX; i++ ) {
            c2_t p = ci;
            for ( int j = 0; j <= limitY; j++ ) {
                if ( READ_PIXEL( p ) ) { 
                    WRITE_PIXEL( p, 0 );
                }
                p = c2Add( p, e1 );
            }
            ci = c2Add( ci, e0 );
        }
    } 
}
