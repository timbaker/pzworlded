/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "worldgenwindow.h"
#include "ui_worldgenwindow.h"

#include "worldgenview.h"

#include <QFileDialog>

using namespace WorldGen;

WorldGenWindow *WorldGenWindow::mInstance = 0;

WorldGenWindow *WorldGenWindow::instance()
{
    if (!mInstance)
        mInstance = new WorldGenWindow;
    return mInstance;
}

#include "plugins/osmimporter/osmimporter.h"
#include "plugins/osmrenderer/qtilerenderer.h"
#include "plugins/osmrenderer/tile-write.h"
#include "plugins/osmrenderer/types.h"
#include "utils/qthelpers.h"
#include <QDebug>
#include <QSettings>

namespace WorldGen {

#include "plugins/osmrenderer/quadtile.h"

class DrawingRules {
  public:
    struct DrawingRule {
        osm_type_t type_from; //List of types the rule
        osm_type_t type_to;   //applies to
        bool polygon;
        int zoom_from, zoom_to;
        int red[2], green[2], blue[2];
        double width[2];
    };
     DrawingRules(const QString &filename);
    bool get_rule(osm_type_t type, int zoom, int pass,
                  int *r, int *g, int *b, double *width, bool *polygon);
  private:
    static void tokenise(const std::string &input, std::vector<std::string> &output);
    bool load_rules();

     QString filename;
    std::vector<struct DrawingRule> drawing_rules;
};


struct coord {
    unsigned long x, y;
};
class Way {
  public:
    Way() {ncoords=0;}
    bool init(QFile *fp);
    void print();
    bool draw(QGraphicsScene &scene, DrawingRules & rules,
              unsigned long tilex, unsigned long tiley,
              int zoom, int magnification, int pass, int posX, int posY);
    static bool sort_by_type(Way w1, Way w2) {return w1.type<w2.type;}
    static void new_tile() {allcoords.clear();}
    osm_type_t type;

  private:
    int coordi, ncoords;
    static std::vector<coord> allcoords; //Coords for all loaded ways.
};
std::vector<coord> Way::allcoords;

//Initialise a way from the database. fp has been seeked to the start of the
//way we wish to read.
bool Way::init(QFile *fp)
{
    unsigned char buf[18];

    qint64 n = fp->read((char*)buf, 11);
    if(n!=11) {
            return false;
    }
    type = (osm_type_t) buf[0];
    int nqtiles = (((int) buf[1]) << 8) | buf[2];

    coord c;
    c.x = buf2l(buf+3);
    c.y = buf2l(buf+7);
    coordi = allcoords.size();
    ncoords = 1;
    allcoords.push_back(c);


    //Add the cordinates
    for(int i=1;i<nqtiles;i++) {
        if(fp->read((char*)buf, 4)!=4) {
            return false;
        }
        long dx = ((buf[0]<<8)  | buf[1]) << 4;
        long dy = ((buf[2]<<8)  | buf[3]) << 4;
        dx -= 1<<19; dy -= 1<<19;
        c.x += (dx);
        c.y += (dy);
        allcoords.push_back(c);
        ncoords++;
    }
    return true;
}

#define TILE_SIZE 256*4
#define TILE_POWER (8+2)

static int g_nways=0, g_ndrawnways=0;
//Draw this way to an img tile.
bool Way::draw(QGraphicsScene &scene, DrawingRules & rules,
              unsigned long tilex, unsigned long tiley,
              int zoom, int magnification, int pass, int posX, int posY)
{
#if 1
    int r, g, b;
    double width;
    bool polygon;
    if(!rules.get_rule(type, zoom, pass, &r, &g, &b, &width, &polygon)) return false;

    QGraphicsPolygonItem *p = polygon ?  new QGraphicsPolygonItem : 0;
    QPainterPath path;
    QGraphicsPathItem *item = polygon ? 0 : new QGraphicsPathItem(path);
    if (width > 0) {
        if (polygon) p->setPen(QPen(QColor(r,g,b), width * magnification));
        else item->setPen(QPen(QColor(r,g,b), width * magnification));
    } else {
        if (polygon) p->setPen(Qt::NoPen);
    }
    if (polygon)
        p->setBrush(QColor(r,g,b));
    QPolygonF pf;

    int oldx=0, oldy=0;

    int j=0;
    for(std::vector<coord>::iterator i=allcoords.begin() + coordi;
        i!=allcoords.begin() + coordi + ncoords; i++){
        int newx, newy;
        if(zoom<10 && i!=allcoords.begin() + coordi)
            i = allcoords.begin() + coordi + ncoords -1;
        //The coords are in the range 0 to 1ULL<<31
        //We want to divide by 2^31 and multiply by 2^zoom and multiply
        //by 2^8 (=256 - the no of pixels in a tile) to get to pixel
        //scale at current zoom. We do this with a shift as it's faster
        //and performance sensitive on profiling. Note we can't do
        //(i->x - tilex) >> (31-8-zoom) as if(tilex>i->x) we'll be shifting
        //a negative number. We shift each part separately, and then cast to a
        //signed number before subtracting.
        newx = (long) ((i->x) >> (31-TILE_POWER-zoom)) - (long) (tilex >> (31-TILE_POWER-zoom));
        newy = (long) ((i->y) >> (31-TILE_POWER-zoom)) - (long) (tiley >> (31-TILE_POWER-zoom));
          newx *= magnification;
          newy *= magnification;
          if(!polygon && (i==allcoords.begin()+coordi))
              path.moveTo(newx, newy);//start pt
        if(!polygon && i!=allcoords.begin() + coordi) {
            path.lineTo(newx, newy);
        }
        if(polygon) {pf += QPointF(newx, newy);}
        oldx=newx; oldy=newy;
    }
    if(polygon) {
        if (pf.size() < 2 || !pf.boundingRect().intersects(QRectF(0,0,TILE_SIZE*magnification,TILE_SIZE*magnification))) {
            delete p;
            return false;
        }
        if (pf.last() != pf.first())
            pf += pf.first();
        p->setPolygon(pf.translated(posX, posY));
        scene.addItem(p);
//        delete [] c;
    } else {
        path.translate(posX, posY);
        item->setPath(path);
        scene.addItem(item);
    }
#else
    g_nways++;
    int r, g, b;
    double width;
    bool polygon;
    if(!rules.get_rule(type, zoom, pass, &r, &g, &b, &width, &polygon)) return false;
    img.SetPen(r, g, b, width * magnification);
    g_ndrawnways++;

    int oldx=0, oldy=0;

    ImgWriter::coord *c=NULL;
    if(polygon) {
        c = new ImgWriter::coord[ncoords];
    }
    int j=0;
    for(std::vector<coord>::iterator i=allcoords.begin() + coordi;
        i!=allcoords.begin() + coordi + ncoords; i++){
        int newx, newy;
        if(zoom<10 && i!=allcoords.begin() + coordi)
            i = allcoords.begin() + coordi + ncoords -1;
        //The coords are in the range 0 to 1ULL<<31
        //We want to divide by 2^31 and multiply by 2^zoom and multiply
        //by 2^8 (=256 - the no of pixels in a tile) to get to pixel
        //scale at current zoom. We do this with a shift as it's faster
        //and performance sensitive on profiling. Note we can't do
        //(i->x - tilex) >> (31-8-zoom) as if(tilex>i->x) we'll be shifting
        //a negative number. We shift each part separately, and then cast to a
        //signed number before subtracting.
        newx = (long) ((i->x) >> (31-8-zoom)) - (long) (tilex >> (31-8-zoom));
        newy = (long) ((i->y) >> (31-8-zoom)) - (long) (tiley >> (31-8-zoom));
          newx *= magnification;
          newy *= magnification;
        if(!polygon && i!=allcoords.begin() + coordi) {
            img.DrawLine(oldx, oldy, newx, newy);
        }
        if(polygon) {c[j].x = newx; c[j++].y = newy;}
        oldx=newx; oldy=newy;
    }
    if(polygon) {
        img.FillPoly(c, ncoords);
        delete [] c;
    }
#endif
    return true;
}

/* DrawingRules - load from rendering.qrr */
DrawingRules::DrawingRules(const QString &filename) :
    filename( filename )
{
    load_rules();
}

bool DrawingRules::get_rule(osm_type_t type, int zoom, int pass,
                  int *r, int *g, int *b, double *width, bool *polygon)
{
    if(pass<0 || pass>1) return false;

    for(std::vector<struct DrawingRule>::iterator rule = drawing_rules.begin();
       rule!=drawing_rules.end(); rule++) {
       if(type >= rule->type_from && type <= rule->type_to &&
           zoom>=rule->zoom_from && zoom<=rule->zoom_to) {
            if(rule->red[pass]==-1) return false;
            *r = rule->red[pass];
            *g = rule->green[pass];
            *b = rule->blue[pass];
            *width = rule->width[pass];
            *polygon = rule->polygon;
            return true;
        }
    }

    return false;
}

bool DrawingRules::load_rules()
{
    static std::map<std::string, osm_type_t> type_table;
    type_table["AREA_PARK"] = AREA_PARK;
    type_table["AREA_CAMPSITE"] = AREA_CAMPSITE;
    type_table["AREA_NATURE"] = AREA_NATURE;
    type_table["AREA_CEMETERY"] = AREA_CEMETERY;
    type_table["AREA_RESIDENTIAL"] = AREA_RESIDENTIAL;
    type_table["AREA_BARRACKS"] = AREA_BARRACKS;
    type_table["AREA_MILITARY"] = AREA_MILITARY;
    type_table["AREA_FIELD"] = AREA_FIELD;
    type_table["AREA_DANGER_AREA"] = AREA_DANGER_AREA;
    type_table["AREA_MEADOW"] = AREA_MEADOW;
    type_table["AREA_COMMON"] = AREA_COMMON;
    type_table["AREA_FOREST"] = AREA_FOREST;
    type_table["AREA_WATER"] = AREA_WATER;
    type_table["AREA_WOOD"] = AREA_WOOD;
    type_table["AREA_RETAIL"] = AREA_RETAIL;
    type_table["AREA_INDUSTRIAL"] = AREA_INDUSTRIAL;
    type_table["AREA_PARKING"] = AREA_PARKING;
    type_table["AREA_BUILDING"] = AREA_BUILDING;
    type_table["HW_PEDESTRIAN"] = HW_PEDESTRIAN;
    type_table["HW_PATH"] = HW_PATH;
    type_table["HW_FOOTWAY"] = HW_FOOTWAY;
    type_table["HW_STEPS"] = HW_STEPS;
    type_table["HW_BRIDLEWAY"] = HW_BRIDLEWAY;
    type_table["HW_CYCLEWAY"] = HW_CYCLEWAY;
    type_table["HW_PRIVATE"] = HW_PRIVATE;
    type_table["HW_UNSURFACED"] = HW_UNSURFACED;
    type_table["HW_UNCLASSIFIED"] = HW_UNCLASSIFIED;
    type_table["HW_RESIDENTIAL"] = HW_RESIDENTIAL;
    type_table["HW_LIVING_STREET"] = HW_LIVING_STREET;
    type_table["HW_SERVICE"] = HW_SERVICE;
    type_table["HW_TERTIARY"] = HW_TERTIARY;
    type_table["HW_SECONDARY"] = HW_SECONDARY;
    type_table["HW_PRIMARY"] = HW_PRIMARY;
    type_table["HW_TRUNK"] = HW_TRUNK;
    type_table["HW_MOTORWAY"] = HW_MOTORWAY;
    type_table["RW_RAIL"] = RW_RAIL;
    type_table["WATERWAY"] = WATERWAY;
    type_table["PLACE_TOWN"] = PLACE_TOWN;

    drawing_rules.clear();
    std::map<std::string, long> colourMap;
    char buf[4096];
     QFile fp(filename);
    if(!fp.open(QIODevice::ReadOnly)) {
          qCritical() << "Can't find rendering rules:" << filename;
        return false;
    }
    while(fp.readLine(buf, 4095) >= 0) {
        if(strlen(buf)<1) return false;
        buf[strlen(buf)-1]=0;
        std::vector<std::string> tokens;
        tokenise(buf, tokens);
        if(tokens.size()<5) continue;
        int i=1;
        if(tokens[0] == "Colour:") {
            if(tokens.size()!=5) continue;
            int red, green, blue;
            i++;
            if(!sscanf(tokens[i++].c_str(), "%i", &red)) continue;
            if(!sscanf(tokens[i++].c_str(), "%i", &green)) continue;
            if(!sscanf(tokens[i++].c_str(), "%i", &blue)) continue;
            printf("%s: %d, %d, %d\n", tokens[1].c_str(), red, green, blue);
            colourMap[tokens[1]] = (red << 16) | (green << 8)  | (blue + 1);
        }
        else if(tokens[0]=="Line:") {
            if(tokens.size()<7) continue;
            DrawingRule r;
            memset(&r, 0, sizeof(DrawingRule));
            r.red[1] = -1; //Disable 2nd pass by default.
            r.type_from = type_table[tokens[i++]];
            r.type_to   = type_table[tokens[i++]];
            r.polygon = false;
            r.zoom_from = atoi(tokens[i++].c_str());
            r.zoom_to = atoi(tokens[i++].c_str());
            std::string tmp = tokens[i++];
            if(colourMap[tmp]) {
                //Colour is stored +1 so we can store black (0) and find it.
                long col = colourMap[tmp]-1;
                r.red[0] = col >> 16;
                r.green[0] = (col >> 8) & 0xFF;
                r.blue[0] = (col & 0xFF);
            } else {
                if(!sscanf(tmp.c_str(), "%i", &r.red[0])) continue;
                if(!sscanf(tokens[i++].c_str(), "%i", &r.green[0])) continue;
                if(!sscanf(tokens[i++].c_str(), "%i", &r.blue[0])) continue;
            }
            r.width[0] = atof(tokens[i++].c_str());
            if(i== (int) tokens.size()) {
                drawing_rules.push_back(r);
                continue;
            }
            tmp = tokens[i++];
            if(colourMap[tmp]) {
                long col = colourMap[tmp]-1;
                r.red[1] = col >> 16;
                r.green[1] = (col >> 8) & 0xFF;
                r.blue[1] = (col & 0xFF);
            } else {
                if(!sscanf(tmp.c_str(), "%i", &r.red[1])) continue;
                if(!sscanf(tokens[i++].c_str(), "%i", &r.green[1])) continue;
                if(!sscanf(tokens[i++].c_str(), "%i", &r.blue[1])) continue;
            }
            r.width[1] = atof(tokens[i++].c_str());
            drawing_rules.push_back(r);
        }
        else if(tokens[0]=="Area:") {
            if(tokens.size()<5) continue;
            DrawingRule r;
            memset(&r, 0, sizeof(DrawingRule));
            r.type_from = type_table[tokens[i++]];
            r.type_to   = r.type_from;
            r.polygon = true;
            r.zoom_from = atoi(tokens[i++].c_str());
            r.zoom_to = atoi(tokens[i++].c_str());
            std::string tmp = tokens[i++];
            if(colourMap[tmp]) {
                //Colour is stored +1 so we can store black (0) and find it.
                long col = colourMap[tmp]-1;
                r.red[0] = col >> 16;
                r.green[0] = (col >> 8) & 0xFF;
                r.blue[0] = (col & 0xFF);
            } else {
                if(!sscanf(tmp.c_str(), "%i", &r.red[0])) continue;
                if(!sscanf(tokens[i++].c_str(), "%i", &r.green[0])) continue;
                if(!sscanf(tokens[i++].c_str(), "%i", &r.blue[0])) continue;
            }
            r.width[0] = -1;
            r.red[1] = -1;
            drawing_rules.push_back(r);
        }
    }
    return true;
}

/* Static helper function */
void DrawingRules::tokenise(const std::string &input, std::vector<std::string> &output)
{
     const char *s = input.c_str();
     char* buffer = new char[strlen( s ) + 1];
     strcpy( buffer, s );
     const char *p = strtok(buffer, " ");
    while(p) {
        output.push_back(p);
        p = strtok(NULL, " ");
    }
     delete buffer;
}

/* A recursive index class into the quadtile way database. */
class qindex {
 public:
   static qindex *load(QFile &fp);
   long get_index(quadtile q, int _level, int *_nways=NULL);

   int max_safe_zoom;
 private:
   qindex(){level = -1;}

   qindex *child[4];
   long long q, qmask;
   long offset;
   int nways;
   int level;
};

#define DB_VERSION "org.hollo.quadtile.pqdb.03"

//Static function to load the index from the start of the DB
qindex *qindex::load(QFile &fp)
{
//    Log(LOG_DEBUG, "Load index\n");
    char tmp[100];
    if(!fp.readLine(tmp, 100)) return NULL;
    if(strncmp(tmp, DB_VERSION, strlen(DB_VERSION))) {
//        Log(LOG_ERROR, "Not a DB file, or wrong version\n");
        return NULL;
    }
    char *s=strstr(tmp, "depth=");
    int max_safe_zoom;
    if(s) max_safe_zoom = atoi(s+6);
    else {
//        Log(LOG_ERROR, "Can't read maximum safe zoom\n");
        return NULL;
    }

    fp.seek(strlen(tmp));////////////


    unsigned char buf[8];
    memset(buf, 0, 8);
    if(fp.read((char*)buf+5, 3)!=3) {
//        Log(LOG_ERROR, "Failed to read index file\n");
        return NULL;
    }
    int nidx = (int) buf2ll(buf);
//    Log(LOG_DEBUG, "Load %d index items\n", nidx);

    qindex *result;
    result = new qindex[nidx];
    result->max_safe_zoom = max_safe_zoom;
    result[0].level = 0;
    result[0].qmask = 0x3FFFFFFFFFFFFFFFLL;

    for(int i=0; i<nidx;i++) {
        result[i].max_safe_zoom = max_safe_zoom;
        if(result[i].level==-1) {
//            Log(LOG_ERROR, "Inconsistent index file - orphaned child\n");
            delete result;
            return NULL;
        }
        unsigned char buf[28];
        if(fp.read((char*)buf, 28)!=28) {
//            Log(LOG_ERROR, "Failed read of index file\n");
            delete result;
            return NULL;
        }
        result[i].q = buf2ll(buf);
        result[i].offset = (buf[8] << 24) |
                           (buf[9] << 16) |
                           (buf[10]<< 8) |
                            buf[11];
        result[i].nways  = (buf[12] << 24) |
                           (buf[13] << 16) |
                           (buf[14]<< 8) |
                            buf[15];

        //nways = buf[12];
        for(int j=0;j<4;j++) {
            long child_offset = (buf[16+j*3] << 16) |
                                (buf[17+j*3] << 8) |
                                 buf[18+j*3];
            if(child_offset>=0xFFFFFE) result[i].child[j]=NULL;
            else {
                result[i].child[j] = result + child_offset;
                if(child_offset > nidx) {
//                    Log(LOG_ERROR, "Inconsistent index file - child too big %x %d\n", (int)child_offset, i);
                    delete result;
                    return NULL;
                }
                result[i].child[j]->level = result[i].level+1;
                result[i].child[j]->qmask = result[i].qmask >> 2;
            }
        }
    }
    return result;
}

//Get the offset into the DB file where we can read the first way from
//quadtile _q (masked with level - ie. the first way in the same level
//_level map tile as _q
long qindex::get_index(quadtile _q, int _level, int *_nways)
{
    if(_level > max_safe_zoom) _level = max_safe_zoom;
    if((_q & (~qmask))!=q) return(-1); //We don't contain _q
    //We contain _q.
    if(_level==level) {
        //If we are the same level as the request, we have the answer.
        *_nways = nways;
        return offset;
    }
    //See whether our children have the answer
    long result=-1;
    for(int i=0;i<4;i++) {
        if(child[i]) result = child[i]->get_index(_q, _level, _nways);
        if(result!=-1) return result;
    }
    //If execution reaches here, _q/_level refers to a tile within us
    //which none of our children claim - ie. an empty section of this tile.
    *_nways = 0;
    return 0;
}

} // namespace WorldGen

/*Static function. Way types that are drawn similarly (eg.
HIGHWAY_RESIDENTIAL / HIGHWAY_LIVING_STREET) should have their first passes
drawn together then second passes to make the pics look right. */
static bool /*TileWriter::*/need_next_pass(int type1, int type2)
{
    if(type1==type2) return false;
    if(type1 > HW_UNSURFACED && type1<HW_SECONDARY &&
       type2 > HW_UNSURFACED && type2<HW_SECONDARY) return false;
    return true;
}

WorldGenWindow::WorldGenWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::WorldGenWindow)
{
    ui->setupUi(this);

    connect(ui->actionOpen, SIGNAL(triggered()), SLOT(open()));

    connect(ui->actionIncreaseDepth, SIGNAL(triggered()),
            ui->view->scene(), SLOT(depthIncr()));
    connect(ui->actionDecreaseDepth, SIGNAL(triggered()),
            ui->view->scene(), SLOT(depthDecr()));

    ui->actionShowGrids->setChecked(true);
    connect(ui->actionShowGrids, SIGNAL(toggled(bool)),
            ui->view->scene(), SLOT(showGrids(bool)));

    connect(ui->actionOsm, SIGNAL(triggered()), SLOT(osm()));


}

WorldGenWindow::~WorldGenWindow()
{
    delete ui;
}

void WorldGenWindow::open()
{
    QString f = QFileDialog::getOpenFileName(this, tr("Open L-System"),
                                             QLatin1String("C:/Programming/WorldGen/Lse/Examples"));
    if (f.isEmpty())
        return;
    ui->view->LoadFile(f);
}

void WorldGenWindow::osm()
{
    ui->view->scene()->clear();
#if 0
    OSMImporter imp;
    imp.Preprocess(QLatin1String("C:\\Programming\\OpenStreetMap\\Vancouver2.osm"));

    QtileRenderer ren;
    QSettings settings;
    settings.beginGroup( QLatin1String("Qtile Renderer") );
    settings.setValue(QLatin1String("input"), QLatin1String("C:\\Programming\\OpenStreetMap\\Vancouver2.osm"));
    settings.setValue(QLatin1String("rulesFile"), QLatin1String(":/rendering_rules/default.qrr"));
    settings.endGroup();
    ren.LoadSettings(&settings);
    ren.Preprocess(&imp, QLatin1String("C:\\Programming\\OpenStreetMap\\monav-data\\Vancouver2"));
#endif
    DrawingRules dr(fileInDirectory( QLatin1String("C:\\Programming\\OpenStreetMap\\monav-data\\Vancouver"),
                                     QLatin1String("rendering.qrr") ));
    QFile db(QLatin1String("C:\\Programming\\OpenStreetMap\\monav-data\\Vancouver\\ways.all.pqdb"));
    if (db.open(QIODevice::ReadOnly)) {
        qindex *qidx = qindex::load(db); // FIXME: free it

        ///// RendererBase::Paint
        int zoom = 13, magnification=1;

        int sizeX = ui->view->viewport()->width();
        int sizeY = ui->view->viewport()->height();

        int m_tileSize = TILE_SIZE;
        double tileFactor = 1u << zoom;
        double zoomFactor = tileFactor * m_tileSize;

        QTransform inverseTransform = ui->view->viewportTransform().inverted();

        const int xWidth = 1 << zoom;
        const int yWidth = 1 << zoom;

        QRect boundingBox = inverseTransform.mapRect( QRect( 0, 0, sizeX, sizeY ) );

        ProjectedCoordinate pc(GPSCoordinate(49.25000,-123.25));
        QPointF request_center(pc.x, pc.y);

        int minX = floor( ( double ) boundingBox.x() / m_tileSize + request_center.x() * tileFactor );
        int maxX = ceil( ( double ) boundingBox.right() / m_tileSize + request_center.x() * tileFactor );
        int minY = floor( ( double ) boundingBox.y() / m_tileSize + request_center.y() * tileFactor );
        int maxY = ceil( ( double ) boundingBox.bottom() / m_tileSize + request_center.y() * tileFactor );

//        int scaledTileSize = m_tileSize * request.virtualZoom;
        int posX = ( minX - request_center.x() * tileFactor ) * m_tileSize * magnification;
        for ( int x = minX; x < maxX; ++x ) {
            int posY = ( minY - request_center.y() * tileFactor ) * m_tileSize * magnification;
            for ( int y = minY; y < maxY; ++y ) {
                if ( x >= 0 && x < xWidth && y >= 0 && y < yWidth ) {
                    ///// TileWriter::query_index
                    //Work out which quadtile to load.
                    long z = 1UL << zoom;
                    double xf = ((double)x)/z;
                    double yf = ((double)y)/z;
                    quadtile qmask, q;
                    qmask = 0x3FFFFFFFFFFFFFFFLL >> ((zoom>qidx->max_safe_zoom ? qidx->max_safe_zoom : zoom)*2);
                    //binary_printf(qmask); binary_printf(q);
                    q = xy2q(xf, yf);

                    //Seek to the relevant point in the db.
                    int nways;
                    long offset = qidx->get_index(q & ~qmask, zoom, &nways);
                    qDebug() << "tile " << x << "," << y << " nways=" << nways;
                    if (nways==1) continue;/////////// end-of-file with nways==1 in Vancouver2
                    if(nways == 0) continue;
                    if(offset==-1 || !db.seek(offset)) {
                        continue; //error
                    }

                    ///// TileWriter::draw
                    Way::new_tile();
                    std::vector<Way> waylist;
                    for(int i=0; i<nways;i++) {
                        Way s;
                        if(!s.init(&db)) continue;
                        waylist.push_back(s);
                    }

                    std::sort(waylist.begin(), waylist.end(), Way::sort_by_type);

                    unsigned long itilex = x << (31-zoom), itiley = y << (31-zoom);
                    //This is a pain. We have to do both passes of one type before moving on to
                    //the next type.
                    int current_type = waylist.begin()->type;
                    std::vector<Way>::iterator cur_type_start, i, j;
                    cur_type_start = waylist.begin();
                    for(i=waylist.begin(); i!=waylist.end(); i++) {
                        if(need_next_pass(i->type, current_type)) { //Do the second pass.
                            for(j=cur_type_start; j!=i; j++)
                                if(!j->draw(*ui->view->scene(), dr, itilex, itiley, zoom, magnification, 1, posX, posY)) break;
                            current_type = i->type;
                            cur_type_start = i;
                        }
                        i->draw(*ui->view->scene(), dr, itilex, itiley, zoom, magnification, 0, posX, posY);
                    }
                    //Do second pass for the last type.
                    for(j=cur_type_start; j!=waylist.end(); j++)
                        if(!j->draw(*ui->view->scene(), dr, itilex, itiley, zoom, magnification, 1, posX, posY)) break;

                    ui->view->scene()->addRect(posX,posY,m_tileSize*magnification,m_tileSize*magnification);
                }
                posY += m_tileSize;
            }
            posX += m_tileSize;
        }
    }
    ui->view->scene()->setSceneRect(QRectF());
}
