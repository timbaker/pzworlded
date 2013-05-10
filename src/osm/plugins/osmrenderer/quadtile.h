#ifndef QUADTILE_H
#define QUADTILE_H

typedef signed long long quadtile;

// Unseralize a quadtile from 8 bytes
quadtile buf2ll(const unsigned char *s)
{
    quadtile result=0;
    for(int i=0; i<8;i++)
        result = (result << 8LL) | s[i];
    return result;
}

// Unserialize an unsigned long from 4 bytes
unsigned long buf2l(const unsigned char *s)
{
    unsigned long result=0;
    for(int i=0; i<4;i++)
        result = (result << 8UL) | s[i];
    return result;
}

// Take two (<=4bytes) numbers, and alternate their binary digits.
quadtile mux(quadtile x, quadtile y)
{
    long long t=0;

    for(int i=0;i<32;i++) {
        t <<= 1;
        if(x & (1ULL<<31)) t |= 1;
        t <<= 1;
        if(y & (1ULL<<31)) t |= 1;
        x <<= 1; y <<= 1;
    }
    return t;
}

void demux(quadtile tile, quadtile *x, quadtile *y)
{
    *x = 0; *y = 0;
    for(int i=0;i<31;i++) {
    //binary_printf(tile); binary_printf(*x); binary_printf(*y);printf("\n");
        (*y) <<= 1; (*x) <<= 1;
        if(tile & (1ULL<<61)) (*x)|=1;
        tile <<= 1;
        if(tile & (1ULL<<61)) (*y)|=1;
        tile <<= 1;
    }
    //binary_printf(tile); binary_printf(*x); binary_printf(*y);printf("\n");
}

/* Store a pair of doubles in the range (0.0->1.0) as integers with alternating
   binary bits */
quadtile xy2q(double fx, double fy)
{
    return mux((long long) (fx * (1ULL<<31)), (long long) (fy * (1ULL<<31)));
}

#ifdef NEED_QTILE_WRITE
// Below here is only needed in the preprocessor.
#include <math.h>
/* latlon to projected x/y coords */
void ll2pxy(double lat, double lon, unsigned long *x, unsigned long *y)
{
    double fx,fy;
    fx = (lon+180.0) / 360.0;
    fy = (1.0 - log(tan(lat*M_PI/180.0) +
          (1.0/cos(lat*M_PI/180.0)))/M_PI)/2.0;

    *x = (unsigned long) (fx * (1UL<<31));
    *y = (unsigned long) (fy * (1UL<<31));
}

/* Serialise a quadtile to 4 unsigned char bytes, MSB first */
unsigned char *ll2buf(quadtile q)
{
    static unsigned char result[8];
    for(int i=0; i<8;i++) {
        result[7-i] = q & 255;
        q >>= 8;
    }
    return result;
}

unsigned char *l2buf(unsigned long l)
{
    static unsigned char result[4];
    for(int i=0; i<4;i++) {
        result[3-i] = l & 255;
        l >>= 8;
    }
    return result;
}

/* Horrible name! Assume a line drawn between q1 and q2.
    if (q1 & qmask) != (q2 & qmask) then return the quadtile q corresponding
    to the first point along the line q1->q2 where (q & qmask != q1 & qmask) */
quadtile line_edge_intersect(quadtile q1, quadtile q2, quadtile qmask)
{
    quadtile qmin, qmax, xmin, xmax, ymin, ymax, x1, y1, x2, y2;
    if((q1 & qmask) == (q2 & qmask)) {
        printf("Error - q1 and q2 are in the same qtile\n");
        exit(-1);
    }

    //Get the bounds of q1's quadtile.
    qmin = q1 & qmask;
    qmax = q1 | (~qmask);
    demux(qmin, &xmin, &ymin);
    demux(qmax, &xmax, &ymax);

    demux(q1, &x1, &y1);
    demux(q2, &x2, &y2);

    //y = ax + b       x = (y-b)/a
    //First a special case for x1==x2 (a==infinity. Doesn't work.)
    if(x1==x2) {
        if(y2>y1) return mux(x1, ymax+1);
        else return mux(x1, ymin-1);
    }

    double a = ((double)(y2-y1)) / ((double)(x2-x1));
    double b = y1 - a *x1;

    //Find where the line crosses the bounds of this quadtile.
    //FIXME this will fail at the international date line - do roads cross this?
    if(x2 > x1) {
        quadtile y = roundl(a*(xmax) + b);
        if(y>=ymin && y<=ymax) return mux(xmax+1, roundl(a*(xmax+1) + b));
    } else {
        quadtile y = roundl(a*(xmin) + b);
        if(y>=ymin && y<=ymax) return mux(xmin-1, roundl(a*(xmin-1) + b));
    }
    if(y2 > y1) {
        quadtile x = roundl((ymax-b)/a);
        if(x>=xmin && x<=xmax) return mux(roundl((ymax+1-b)/a), ymax+1);
    } else {
        quadtile x = roundl((ymin-b)/a);
        if(x>=xmin && x<=xmax) return mux(roundl((ymin-b-1)/a), ymin-1);
    }

    printf("Error - shouldn't get here in intersect calcs\n");
    //Debugging output.
    printf("q1=%llx q2=%llx qmask=%llx\na=%f, b=%f\n1=%lld,%lld 2=%lld,%lld\n",
            q1, q2, qmask, a, b, x1, y1, x2, y2);
    printf("xmin = %lld xmax = %lld, ymin = %lld, ymax=%lld\n",
            xmin, xmax, ymin, ymax);
    printf("Maxy = %f, Miny=%f\n", a*(xmax+1)+b, a*(ymin-1)+b);
    printf("Maxx = %f, Minx=%f\n", (ymax+1-b)/a, (ymin-1-b)/a);
    exit(-1);
    return 0;
}
#endif /*NEED_QTILE_WRITE*/

#endif /*QUADTILE_H*/
