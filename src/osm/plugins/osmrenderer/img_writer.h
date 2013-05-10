#ifndef IMG_WRITER_H
#define IMG_WRITER_H

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "agg2/agg_pixfmt_rgb.h"
#include "agg2/agg_renderer_scanline.h"
#include "agg2/agg_rasterizer_scanline_aa.h"
#include "agg2/agg_scanline_u.h"
#include "agg2/agg_path_storage.h"
#include "agg2/agg_conv_stroke.h"
#include "agg2/agg_conv_close_polygon.h"
#include "agg2/agg_trans_affine.h"


class ImgWriter {
  public:
    typedef agg::rendering_buffer                         renbuf_type;
    typedef agg::pixfmt_rgb24                             pixfmt_type;
    typedef agg::renderer_base<pixfmt_type>               renbase_type;
    typedef agg::renderer_scanline_aa_solid<renbase_type> renderer_type;
    typedef agg::path_storage                             path_type;
    typedef agg::conv_transform<path_type>                trans_path_type;
    typedef agg::conv_stroke<trans_path_type>             stroke_type;
    typedef agg::rgba8                                    color_type;

    struct coord {int x; int y;};

    ImgWriter() {
        buf=NULL;
        rbuf=NULL;
        pixf=NULL;
        rbase=NULL;
        renderer=NULL;
        drawing = NOTHING;
        _r = _g = _b = -1;
    };
    ~ImgWriter() {
        if(buf) delete [] buf;
        if(rbuf) delete rbuf;
        if(pixf) delete pixf;
        if(rbase) delete rbase;
        if(renderer) delete renderer;
    };
    void NewImage(int x, int y, std::string _name) {
        frame_width = x; frame_height = y;
        rasterizer.clip_box(0, 0, frame_width, frame_height);
        name = _name;
        if(buf) delete [] buf;
        if(rbuf) delete rbuf;
        if(pixf) delete pixf;
        if(rbase) delete rbase;
        if(renderer) delete renderer;

        buf = new unsigned char[x * y * 3];

        rbuf = new renbuf_type(buf, frame_width, frame_height, frame_width * 3);
        pixf = new pixfmt_type(*rbuf);
        rbase = new renbase_type(*pixf);
        renderer = new renderer_type(*rbase);
        drawing = NOTHING;
    };
    void SetBG(int r, int g, int b) {
        rbase->clear(agg::rgba8(r, g, b));
        //prim->fill_color(agg::rgba8(r, g, b));
        //prim->solid_rectangle(0, 0, frame_width, frame_height);
    };
    void SetPen(int r, int g, int b, double w=1) {
        if(r==_r && g==_g && b==_b && w==pen_width) return;

        if(drawing==LINES) draw_lines();

        _r = r; _g = g; _b = b;
        line_color = agg::rgba8(r, g, b);
        pen_width = w;
    };
    
    bool clip_line_to_img(int &x1, int &y1, int &x2, int &y2) {
        //Simple clipping function for speed. Check profiling
        //to see how much is important here. Return false if line
        //doesn't cross image at all. Otherwise move line ends to
        //close to the edge of the image (ie. for performance not
        //memory protection).

        bool need_adjust_start=true, need_adjust_end=true;
        //Are the ends in the image. This is cheap to test and true for
        //most lines. 
        if((x1>=-5 && x1<=frame_width+5 && y1>=-5 && y1<=frame_height+5)) {
            if(x2>=-5 && x2<=frame_width+5 && y2>=-5 && y2<=frame_height+5) 
                return true; //Line already contained by image. All done.
            need_adjust_start=false;
        }
        else if(x2>=-5 && x2<=frame_width+5 && y2>=-5 && y2<=frame_height+5) 
            need_adjust_end=false;
        else {
            //Quick check for lines which are way off.
            //This is worthwile on profile
            if(x1<0 && x2<0) return false;
            if(x1>frame_width && x2>frame_width) return false;
            if(y1<0 && y2<0) return false;
            if(y1>frame_height && y2>frame_height) return false;
        }
        // Now calculate where the line intersects the edges of the image.
        // First special case for vertical lines:
        if(x1==x2) {
            //These two checks have already been done in "Quick checks" above.
            //if(x1 <-5 || x1 > frame_width+5) return false;
            //if((y1<-5  && y2<-5) || (y1>frame_width+5 &&y2>frame_width+5)
            //    return false;
            if(need_adjust_start) y1 = y1>y2 ? frame_height+5 : -5;
            if(need_adjust_end) y2 = y2>y1 ? frame_height+5 : -5;
            return true;
        }
        // Now calculate a and b where y = ax + b
        double a, b;
        a = ((double)(y2-y1)) / ((double)(x2-x1));
        b = y1 - a * x1;
        int ibottom, ileft, iright, itop;
        ileft = -5*a + b; iright = a*(frame_width+5) + b;
        ibottom = (-5-b)/a; itop = (frame_height+5-b)/a;
        //If we intersect an edge of the tile we need drawing.
        bool intersect = false;
        if(ibottom > -5 && ibottom < frame_width +5) {
            if(y2>y1 && need_adjust_start) {x1=ibottom; y1=-5;
                                            need_adjust_start=false;}
            if(y1>y2 && need_adjust_end) {x2=ibottom; y2=-5;
                                          need_adjust_end=false;}
            intersect = true;
        }
        if(itop    > -5 && itop    < frame_width +5) {
            if(y2>y1 && need_adjust_end) {x2=itop; y2=frame_height+5;
                                          need_adjust_end=false;}
            if(y1>y2 && need_adjust_start) {x1=itop; y1=frame_height+5;
                                            need_adjust_start=false;}
            intersect = true;
        }
        if(ileft   > -5 && ileft   < frame_height+5) {
            if(x2>x1 && need_adjust_start) {x1=-5; y1=ileft;
                                            need_adjust_start=false;}
            if(x1>x2 && need_adjust_end) {x2=-5; y2=ileft;
                                          need_adjust_end=false;}
            intersect = true;
        }
        if(iright  > -5 && iright  < frame_height+5) {
            if(x2>x1 && need_adjust_end) {x2=frame_width+5; y2=iright; 
                                          need_adjust_end=false;}
            if(x1>x2 && need_adjust_end) {x1=frame_width+5; y1=iright;
                                          need_adjust_start=false;}
            intersect = true;
        }
        //otherwise we don't
        return intersect;
    };
    bool polygon_crosses_tile(struct coord *points, int npoints) {
        //As above. Could be made more accurate, not sure it's worth it.
        int maxx=-10000, maxy=-10000, minx=10000, miny=10000;
        for(int i=0;i<npoints;i++) {
            if(points[i].x>maxx) maxx= points[i].x;
            if(points[i].x<minx) minx= points[i].x;
            if(points[i].y>maxy) maxy= points[i].y;
            if(points[i].y<miny) miny= points[i].y;
        }
        if(minx > frame_width || miny > frame_height ||
           maxx < 0 || maxy < 0) return false;
        return true;
    };
    void FillPoly(struct coord *points, int npoints) {
        if(npoints<2) return;
        if(!polygon_crosses_tile(points, npoints)) return;
        if(drawing==LINES) draw_lines();

        path.move_to(points[0].x, points[0].y);
        for(int i=1;i<npoints;i++)
            path.line_to(points[i].x, points[i].y);
        path.close_polygon();

        //FIXME Cargo Cult. Is this all really neaded?
        agg::trans_affine mtx;
        trans_path_type trans(path, mtx);
        //stroke_type stroke(trans);
        agg::conv_close_polygon<trans_path_type> poly(trans);

        renderer->color(line_color);
        rasterizer.add_path(poly);
        agg::render_scanlines(rasterizer, scanline, *renderer);
        rasterizer.reset();
        path.free_all();
    };
    void draw_lines() {
        drawing = NOTHING;
        //FIXME Cargo Cult. Is this all really neaded?
        agg::trans_affine mtx;
        trans_path_type trans(path, mtx);
        stroke_type stroke(trans);

        stroke.width(pen_width);
        stroke.line_cap(agg::round_cap);
        renderer->color(line_color);
        rasterizer.add_path(stroke);
        agg::render_scanlines(rasterizer, scanline, *renderer);
        rasterizer.reset();
        path.free_all();
    };
    void DrawLine(int x1, int y1, int x2, int y2) {
        if(!clip_line_to_img(x1, y1, x2, y2)) return;
        path.move_to(x1, y1);
        path.line_to(x2, y2);
        drawing = LINES;
    };
    void Save() {
        if(drawing==LINES) draw_lines();
        FILE* fd = fopen("tmp.ppm", "wb");
        if(fd)
        {
            fprintf(fd, "P6 %d %d 255 ", frame_width, frame_height);
            fwrite(buf, 1, frame_width * frame_height * 3, fd);
            fclose(fd);
          
        }
        std::string s("convert tmp.ppm ");
        s+= name;
#ifdef _CRT_SYSTEM_DEFINED // not always defined, e.g. windows mobile
		  if(std::system(s.c_str())!=0) printf("Failed convert\n");
#else
		  printf("System not available, Failed!\n" );
#endif
    };
    const unsigned char * get_img_data() {
        if(drawing==LINES) draw_lines();
        return buf;
    };

  private:
    agg::scanline_u8 scanline;
    agg::rasterizer_scanline_aa<> rasterizer;
    std::string name;
    int frame_width, frame_height;
    unsigned char *buf;
    agg::rendering_buffer *rbuf;
    pixfmt_type *pixf;
    renbase_type *rbase;
    renderer_type *renderer;
    color_type line_color;
    path_type path;
    double pen_width;
    enum {NOTHING, LINES, POLYGON} drawing;
    int _r, _g, _b;
};

//typedef ImgWriter ImgWriter;
#endif

