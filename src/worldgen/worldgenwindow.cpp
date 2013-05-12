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
    bool draw(QPainter *painter, DrawingRules & rules,
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

#define TILE_SIZE (256*2)
#define TILE_POWER (8+1)

static int g_nways=0, g_ndrawnways=0;
//Draw this way to an img tile.
bool Way::draw(QPainter *painter, DrawingRules & rules,
              unsigned long tilex, unsigned long tiley,
              int zoom, int magnification, int pass, int posX, int posY)
{
#if 1
//    posX = posY = 0;

    int r, g, b;
    double width;
    bool polygon;
    if(!rules.get_rule(type, zoom, pass, &r, &g, &b, &width, &polygon)) return false;

    if (width > 0) {
        painter->setPen(QPen(QColor(r,g,b,128), width * magnification));
    } else {
//        painter->setPen(QColor(255,0,0,128));
    }
    if (polygon)
        painter->setBrush(QColor(r,g,b,128));
    else
        painter->setBrush(Qt::NoBrush);
    QPolygonF pf;
    QPainterPath path;

    int oldx=0, oldy=0;

    int j=0;
    for(std::vector<coord>::iterator i=allcoords.begin() + coordi;
        i!=allcoords.begin() + coordi + ncoords; i++){
        double newx, newy;
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
#if 1
        newx = (long) ((i->x) >> (31-TILE_POWER-zoom)) - (long) (tilex >> (31-TILE_POWER-zoom));
        newy = (long) ((i->y) >> (31-TILE_POWER-zoom)) - (long) (tiley >> (31-TILE_POWER-zoom));
#else
        newx = (long) ((i->x) >> (31-TILE_POWER-zoom)) - (long) (tilex >> (31-TILE_POWER-zoom));
        newy = (long) ((i->y) >> (31-TILE_POWER-zoom)) - (long) (tiley >> (31-TILE_POWER-zoom));
#endif
        newx *= magnification;
        newy *= magnification;

        if (polygon) {
            if (!pf.size() || pf.last() != QPointF(newx,newy))
                pf += QPointF(newx, newy);
        } else {
            if (i == allcoords.begin() + coordi) {
                path.moveTo(newx, newy);//start pt
                pf += QPointF(newx, newy); // debug
            } else {
                if (oldx != newx || oldy != newy) {
                    path.lineTo(newx, newy);
                    pf += QPointF(newx, newy); // debug
                }
            }

        }
#if 0
        if (posX+newx >= 1500 && posX+newx <= 1520 && posY+newy>=890 && posY+newy <= 910) {
            int i = 0;
        }
#endif
        oldx=newx; oldy=newy;
    }
    if(polygon) {
        if (pf.size() <= 2 /*|| !pf.boundingRect().intersects(QRectF(0,0,TILE_SIZE*magnification,TILE_SIZE*magnification))*/) {
            return true;
        }

        if (pf.last() != pf.first())
            pf += pf.first();
        pf.translate(posX, posY);
#if 0
        bool bogus = false;
        for (int i = 0; i < pf.size() - 1; i += 2) {
            if (QLineF(pf[i],pf[i+1]).length() > 1536) {
                bogus = true;
                break;
            }
        }
//        if (!bogus) return true;
#endif
        painter->drawPolygon(pf, Qt::OddEvenFill);
    } else {
        if (path.elementCount() < 2)
            return true;

        path.translate(posX, posY);
#if 0
        qreal maxLen = 0;
        for (int i = 0; i < pf.size() - 1; i += 2) {
            maxLen = qMax(maxLen, QLineF(pf[i],pf[i+1]).length());
        }
        if (maxLen > 256)
            qDebug() << "max non-poly segment length " << maxLen;
        if (path.boundingRect().width() > 1536) {
            int i = 0;
        }
#endif
        painter->drawPath(path);
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

#include "osmfile.h"
#include "osmlookup.h"
#include "osmrulefile.h"

#include <qmath.h>
#include <QGraphicsSceneHoverEvent>
#include <QMultiMap>

namespace WorldGen {

#define BERLIN 0

class OpenStreetMapItem : public QGraphicsItem
{
public:
    OpenStreetMapItem(QString dir) :
        QGraphicsItem(),
         dr(fileInDirectory(dir, QLatin1String("rendering.qrr"))),
         zoom(13)
    {
        setFlag(ItemUsesExtendedStyleOption);


        QString filename[3];
        db[0].setFileName(fileInDirectory( dir, QLatin1String("ways.all.pqdb") ));
        db[1].setFileName(filename[1] = fileInDirectory( dir, QLatin1String("ways.motorway.pqdb") ));
        db[2].setFileName(filename[2] = fileInDirectory( dir, QLatin1String("places.pqdb") ));
        for(int i=0;i<3;i++) {
            if (db[i].open(QIODevice::ReadOnly)) {
                qidx[i] = qindex::load(db[i]);
            } else {
                qidx[i] = 0;
            }
        }
    }

    // longitude -> east/west (0 at Prime Meridian -> 180 east, -180 west)
    // latitude -> north/south (0 equator -> 90 poles)
    QRectF boundingRect() const
    {
#if BERLIN
        UnsignedCoordinate uc1(575273140, 350191409);
        UnsignedCoordinate uc2(577977756, 353551012); // Berlin
        ProjectedCoordinate topLeft = uc1.ToProjectedCoordinate();
        ProjectedCoordinate botRight = uc2.ToProjectedCoordinate();
#else
        qreal latMax = 49.00000, longMin = -123.31000, latMin = 49.42000, longMax = -122.67000; // Vancouver
        ProjectedCoordinate topLeft(GPSCoordinate(latMin, longMin));
        ProjectedCoordinate botRight(GPSCoordinate(latMax, longMax));
#endif
        double worldInPixels = (1u << zoom) * TILE_SIZE; // size of world in pixels
        return QRectF(0/*topLeft.x * tileFactor*/, 0/*topLeft.y * tileFactor*/,
                      (botRight.x - topLeft.x) * worldInPixels,
                      (botRight.y - topLeft.y) * worldInPixels);
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
    {
        qreal scale = painter->transform().m22(); // vertical scale

        ///// RendererBase::Paint

        int sizeX = painter->device()->width();
        int sizeY = painter->device()->height();

        int m_tileSize = TILE_SIZE;
        double tileFactor = 1u << zoom; // 2^zoom tiles across the whole world
//        double zoomFactor = tileFactor * m_tileSize;

        QTransform inverseTransform = painter->worldTransform().inverted();

        const int xWidth = 1 << zoom;
        const int yWidth = 1 << zoom;

#if BERLIN // Berlin
        UnsignedCoordinate uc1(575273140, 350191409);
        UnsignedCoordinate uc2(577977756, 353551012);
        ProjectedCoordinate topLeft = uc1.ToProjectedCoordinate();
        ProjectedCoordinate botRight = uc2.ToProjectedCoordinate();
#else // Vancouver
        qreal latMax = 49.00000, longMin = -123.31000, latMin = 49.42000, longMax = -122.67000;
        ProjectedCoordinate topLeft(GPSCoordinate(latMin, longMin));
        ProjectedCoordinate botRight(GPSCoordinate(latMax, longMax));
#endif
#if 1
        int tileMinX = option->exposedRect.x() / TILE_SIZE;
        int tileMinY = option->exposedRect.y() / TILE_SIZE;
        int tileMaxX = (option->exposedRect.right() + TILE_SIZE - 1) / TILE_SIZE;
        int tileMaxY = (option->exposedRect.bottom() + TILE_SIZE - 1) / TILE_SIZE;

        qDebug() << tileMinX << "->" << tileMaxX << "," << tileMinY << "->" << tileMaxY;


        int minX = topLeft.x * tileFactor + tileMinX;
        int maxX = minX + (tileMaxX - tileMinX);
        int minY = topLeft.y * tileFactor + tileMinY;
        int maxY = minY + (tileMaxY - tileMinY);

        qDebug() << minX << "->" << maxX << "," << minY << "->" << maxY;
#else
        QRect boundingBox = inverseTransform.mapRect( QRect( 0, 0, sizeX, sizeY ) );

        ProjectedCoordinate pc(GPSCoordinate(latMin + (latMax - latMin) / 2,
                                             longMin + (longMax - longMin) / 2));
        QPointF request_center(pc.x, pc.y); // x && y  0->1.0

        int minX = floor( ( double ) boundingBox.x() / m_tileSize + request_center.x() * tileFactor );
        int maxX = ceil( ( double ) boundingBox.right() / m_tileSize + request_center.x() * tileFactor );
        int minY = floor( ( double ) boundingBox.y() / m_tileSize + request_center.y() * tileFactor );
        int maxY = ceil( ( double ) boundingBox.bottom() / m_tileSize + request_center.y() * tileFactor );
#endif

//        int scaledTileSize = m_tileSize * request.virtualZoom;
        int posX = tileMinX * TILE_SIZE;
        for ( int x = minX; x < maxX; ++x ) {
            int posY = tileMinY * TILE_SIZE;
            for ( int y = minY; y < maxY; ++y ) {
                drawTile(painter, x, y, posX, posY);
                posY += m_tileSize;
            }
            posX += m_tileSize;
        }
    }

    void drawTile(QPainter *painter, int x, int y, int posX, int posY)
    {

        int cur_db = (zoom > 12) ? 0 : 1;
        int magnification=1;

        ///// TileWriter::query_index
        //Work out which quadtile to load.
        long z = 1UL << zoom;
        double xf = ((double)x)/z;
        double yf = ((double)y)/z;
        quadtile qmask, q;
        qmask = 0x3FFFFFFFFFFFFFFFLL >> (((zoom > qidx[cur_db]->max_safe_zoom) ? qidx[cur_db]->max_safe_zoom : zoom)*2);
        //binary_printf(qmask); binary_printf(q);
        q = xy2q(xf, yf);

        //Seek to the relevant point in the db.
        int nways;
        long offset = qidx[cur_db]->get_index(q & ~qmask, zoom, &nways);
        qDebug() << "tile " << x << "," << y << " nways=" << nways;
        if (nways==1) return;/////////// end-of-file with nways==1 in Vancouver2
        if(nways == 0) return;
        if(offset==-1 || !db[cur_db].seek(offset)) {
            return; //error
        }

        ///// TileWriter::draw
        Way::new_tile();
        std::vector<Way> waylist;
        for(int i=0; i<nways;i++) {
            Way s;
            if (!s.init(&db[cur_db])) continue;
            waylist.push_back(s);
        }

        std::sort(waylist.begin(), waylist.end(), Way::sort_by_type);

        //                    painter->translate(posX,posY);
        painter->setClipRect(posX,posY,TILE_SIZE*magnification,TILE_SIZE*magnification);

        Q_ASSERT(x >= 0 && y >= 0);
        unsigned long itilex = ((unsigned long)x) << (31-zoom);
        unsigned long itiley = ((unsigned long)y) << (31-zoom);
        //This is a pain. We have to do both passes of one type before moving on to
        //the next type.
        int current_type = waylist.begin()->type;
        std::vector<Way>::iterator cur_type_start, i, j;
        cur_type_start = waylist.begin();
        for(i=waylist.begin(); i!=waylist.end(); i++) {
            if(need_next_pass(i->type, current_type)) { //Do the second pass.
                for(j=cur_type_start; j!=i; j++)
                    if(!j->draw(painter, dr, itilex, itiley, zoom, magnification, 1, posX, posY)) break;
                current_type = i->type;
                cur_type_start = i;
            }
            i->draw(painter, dr, itilex, itiley, zoom, magnification, 0, posX, posY);
        }
        //Do second pass for the last type.
        for(j=cur_type_start; j!=waylist.end(); j++)
            if(!j->draw(painter, dr, itilex, itiley, zoom, magnification, 1, posX, posY)) break;

        painter->setPen(Qt::black);
        painter->drawRect(posX,posY,TILE_SIZE*magnification,TILE_SIZE*magnification);

        //                    painter->translate(-posX,-posY);
    }

    DrawingRules dr;
    QFile db[3];
    qindex *qidx[3];
    int zoom;
};

class OpenStreetMapItem2 : public QGraphicsItem
{
public:
    OpenStreetMapItem2(QString osmFile) :
        QGraphicsItem(),
        zoom(14),
        magnification(1)
    {
        setFlag(ItemUsesExtendedStyleOption);
        setAcceptHoverEvents(true);

        if (mFile.read(osmFile))
            mLookup.fromFile(mFile);

        if (!mRuleFile.read(QLatin1String("C:/Programming/Tiled/PZWorldEd/PZWorldEd/OSM_Rules.txt")))
            qDebug() << mRuleFile.errorString();

        foreach (OSM::RuleFile::Line line, mRuleFile.mLines) {
            // range from rule1 to rule2
            int s = mRuleFile.mRuleIDs.indexOf(line.rule1);
            int e = mRuleFile.mRuleIDs.indexOf(line.rule2);
            for (int i = s; i <= e; i++)
                mLineByRuleID[mRuleFile.mRuleIDs[i]] = line;
        }
        foreach (OSM::RuleFile::Area area, mRuleFile.mAreas)
            mAreaByRuleID[area.rule_id] = area;

        foreach (OSM::Relation *relation, mFile.relations()) {
            if (relation->type != OSM::Relation::MultiPolygon)
                continue;
            foreach (OSM::Relation::Member m, relation->members) {
                if (m.way) mWayToRelations.insert(m.way, relation);
            }
            foreach (QString tagKey, relation->tags.keys()) {
                QString tagValue = relation->tags[tagKey];
                if (mRuleFile.mWayToRule[tagKey].valueToRule.contains(tagValue)) {
                    QString rule_id = mRuleFile.mWayToRule[tagKey].valueToRule[tagValue];
                    if (mAreaByRuleID.contains(rule_id)) {
                        mRelationToArea[relation] = &mAreaByRuleID[rule_id];
                        break;
                    }
                    if (mLineByRuleID.contains(rule_id)) {
                        mRelationToLine[relation] = &mLineByRuleID[rule_id];
                        break;
                    }
                }
            }
        }

        foreach (OSM::Way *way, mFile.ways() + mFile.coastlines()) {
            // Get tags from relations if any
            QMap<QString,QString> tags = way->tags;
            if (mWayToRelations.contains(way)) {
                bool ignore = false;
                foreach (OSM::Relation *relation, mWayToRelations.values(way)) {
                    if (mRelationToArea.contains(relation) ||
                            mRelationToLine.contains(relation)) {
                        ignore = true;
                        break;
                    }
                    if (ignore) continue;
                }
            }
            foreach (QString tagKey, tags.keys()) {
                if (mRuleFile.mWayToRule.contains(tagKey)) {
                    QString tagValue = tags[tagKey];
                    if (mRuleFile.mWayToRule[tagKey].valueToRule.contains(tagValue)) {
                        QString rule_id = mRuleFile.mWayToRule[tagKey].valueToRule[tagValue];
                        if (mAreaByRuleID.contains(rule_id) && way->isClosed()) {
                            mWayToArea[way] = &mAreaByRuleID[rule_id];
                            way->polygon = true;
                            break;
                        }
                        if (mLineByRuleID.contains(rule_id)) {
                            mWayToLine[way] = &mLineByRuleID[rule_id];
                            break;
                        }
                    }
                }
            }
        }
    }

    QRectF boundingRect() const
    {
        int worldGridX1 = qFloor(mFile.minProjected().x * (1U << zoom));
        int worldGridY1 = qFloor(mFile.minProjected().y * (1U << zoom));
        int worldGridX2 = qCeil(mFile.maxProjected().x * (1U << zoom));
        int worldGridY2 = qCeil(mFile.maxProjected().y * (1U << zoom));

        return QRectF(0, 0,
                      (worldGridX2 - worldGridX1) * TILE_SIZE,
                      (worldGridY2 - worldGridY1) * TILE_SIZE);
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
    {
        int tileMinX = option->exposedRect.x() / TILE_SIZE;
        int tileMinY = option->exposedRect.y() / TILE_SIZE;
        int tileMaxX = (option->exposedRect.right() + TILE_SIZE - 1) / TILE_SIZE;
        int tileMaxY = (option->exposedRect.bottom() + TILE_SIZE - 1) / TILE_SIZE;

        qDebug() << tileMinX << "->" << tileMaxX << "," << tileMinY << "->" << tileMaxY;

        double tileFactor = 1u << zoom; // 2^zoom tiles across the whole world

        GPSCoordinate topLeft(mFile.northMostLatitude(), mFile.westMostLongitude());
        int minX = ProjectedCoordinate(topLeft).x * tileFactor + tileMinX;
        int maxX = minX + (tileMaxX - tileMinX);
        int minY = ProjectedCoordinate(topLeft).y * tileFactor + tileMinY;
        int maxY = minY + (tileMaxY - tileMinY);

        qDebug() << minX << "->" << maxX << "," << minY << "->" << maxY;

        int posX = tileMinX * TILE_SIZE;
        for ( int x = minX; x < maxX; ++x ) {
            int posY = tileMinY * TILE_SIZE;
            for ( int y = minY; y < maxY; ++y ) {
                drawTile(painter, x, y, posX, posY);
                posY += TILE_SIZE;
            }
            posX += TILE_SIZE;
        }
    }

    void drawTile(QPainter *painter, int x, int y, int posX, int posY)
    {
        painter->setClipRect(posX,posY,TILE_SIZE*magnification,TILE_SIZE*magnification);

        double tileFactor = 1u << zoom; // 2^zoom tiles across the whole world

        ProjectedCoordinate min(x / tileFactor, y / tileFactor);
        ProjectedCoordinate max((x+1) / tileFactor, (y+1) / tileFactor);

        QMultiMap<int,OSM::Lookup::Object> drawThis;
        foreach (OSM::Lookup::Object lo, mLookup.objects(min, max)) {
            QString rule_id;
            if (OSM::Way *way = lo.way) {
                if (way->polygon)
                    rule_id = mWayToArea[way]->rule_id;
                else if (mWayToLine.contains(way))
                    rule_id = mWayToLine[way]->rule1;
            }
            if (OSM::Relation *relation = lo.relation) {
                if (relation->type == OSM::Relation::MultiPolygon) {
                    if (mRelationToArea.contains(relation))
                        rule_id = mRelationToArea[relation]->rule_id;
                    else if (mRelationToLine.contains(relation))
                        rule_id = mRelationToLine[relation]->rule1;
                }
            }
            if (!rule_id.isEmpty())
                drawThis.insert(mRuleFile.mRuleIDs.indexOf(rule_id), lo);
            else
                drawThis.insert(1000, lo);
        }
#if 1
        foreach (OSM::Lookup::Object lo, drawThis) {
            if (OSM::Way *way = lo.way) {
                QPainterPath p;
                int i = 0;
                foreach (OSM::Node *node, way->nodes) {
                    if (i++ == 0)
                        p.moveTo(projectedCoordToPixels(node->pc));
                    else
                        p.lineTo(projectedCoordToPixels(node->pc));

                    if (node == mClosestNode) {
                        painter->setPen(Qt::blue);
                        painter->setBrush(Qt::NoBrush);
                        painter->drawEllipse(projectedCoordToPixels(node->pc), 8, 8);
                    }
                }
                if (way->polygon) {
                    painter->setBrush(mWayToArea[way]->color);
                    painter->setPen(Qt::NoPen);
                } else {
                    painter->setBrush(Qt::NoBrush);
                    if (mWayToLine.contains(way)) {
                        if (mWayToLine[way]->color1 == QColor(6,6,6,200)) // hack nature_reserve
                            painter->setPen(Qt::NoPen);
                        else
                            painter->setPen(QPen(mWayToLine[way]->color1, mWayToLine[way]->width1 * 0));
                    } else if (mWayToRelations.contains(way)) {
                        painter->setPen(Qt::red);
                        bool willDrawLater = false;
                        foreach (OSM::Relation *relation, mWayToRelations.values(way)) {
                             if (mRelationToArea.contains(relation)
                                     || mRelationToLine.contains(relation)) {
                                 willDrawLater = true;
                                 break;
                             }
                        }
                        if (willDrawLater) continue;
                    } else
                        painter->setPen(Qt::red);
                }
                painter->drawPath(p);
                if (mHoverObjects.contains(OSM::Lookup::Object(way,0,0)) && way->isClosed())
                    painter->fillPath(p, QColor(128,128,128,128));
            }
            if (OSM::Relation *relation = lo.relation) {
                QPainterPath path;
                foreach (OSM::WayChain *chain, relation->outer) {
                    QPolygonF poly;
                    foreach (OSM::Node *node, chain->toPolygon())
                        poly += projectedCoordToPixels(node->pc);
                    path.addPolygon(poly);
                }
                foreach (OSM::WayChain *chain, relation->inner) {
                    QPolygonF poly;
                    foreach (OSM::Node *node, chain->toPolygon())
                        poly += projectedCoordToPixels(node->pc);
                    path.addPolygon(poly);
                }
                if (mRelationToArea.contains(relation)) {
                    painter->setPen(Qt::NoPen);
                    if (mRelationToArea[relation]->color == QColor(6,6,6,200)) // hack nature_reserve
                        painter->setBrush(Qt::CrossPattern);
                    else
                        painter->setBrush(mRelationToArea[relation]->color);
                } else if (mRelationToLine.contains(relation)) {
                    painter->setBrush(Qt::NoBrush);
                    if (mRelationToLine[relation]->color1 == QColor(6,6,6,200)) // hack nature_reserve
                        continue;
                    painter->setPen(QPen(mRelationToLine[relation]->color1, mRelationToLine[relation]->width1 * 0));
                } else {
                    if (relation->tags.contains(QLatin1String("amenity"))) continue;
                    painter->setBrush(QColor(128,0,0,128));
                    qDebug() << "*** multipolygon with no color" << relation->id << relation->tags;
                    continue;
                }
                painter->drawPath(path);
                if (mHoverObjects.contains(OSM::Lookup::Object(0,0,relation)))
                    painter->fillPath(path, QColor(128,128,128,128));
            }
        }

#else
        foreach (OSM::Way *way, mLookup.ways(min, max)) {
            QPainterPath p;
            int i = 0;
            foreach (OSM::Node *node, way->nodes) {
                if (i++ == 0)
                    p.moveTo(projectedCoordToPixels(node->pc));
                else
                    p.lineTo(projectedCoordToPixels(node->pc));

                if (node == mClosestNode) {
                    painter->setPen(Qt::blue);
                    painter->drawEllipse(projectedCoordToPixels(node->pc), 8, 8);
                }
            }
            if (way->polygon) {
                painter->setBrush(mWayToArea[way]->color);
                painter->setPen(Qt::NoPen);
            } else {
                painter->setBrush(Qt::NoBrush);
                if (mWayToLine.contains(way))
                    painter->setPen(QPen(mWayToLine[way]->color1, 0/*mWayToLine[way]->width1*/));
                else
                    painter->setPen(Qt::red);
            }
            painter->drawPath(p);
            if (mHoverWays.contains(way) && way->isClosed())
                painter->fillPath(p, QColor(128,128,128,128));
        }

        foreach (OSM::Lookup::Object lo, mLookup.objects(min, max)) {
            if (!lo.relation) continue;
            OSM::Relation *relation = lo.relation;
            if (relation->type != OSM::Relation::MultiPolygon) continue;
            QPainterPath path;
            foreach (OSM::WayChain *chain, relation->outer) {
                QPolygonF poly;
                foreach (OSM::Node *node, chain->toPolygon())
                    poly += projectedCoordToPixels(node->pc);
                path.addPolygon(poly);
            }
            foreach (OSM::WayChain *chain, relation->inner) {
                QPolygonF poly;
                foreach (OSM::Node *node, chain->toPolygon())
                    poly += projectedCoordToPixels(node->pc);
                path.addPolygon(poly);
            }
            if (mRelationToArea.contains(relation)) {
                if (mRelationToArea[relation]->color == QColor(6,6,6,200)) // hack nature_reserve
                    painter->setBrush(Qt::CrossPattern);
                else
                    painter->setBrush(mRelationToArea[relation]->color);
            } else if (mRelationToLine.contains(relation)) {
                painter->setBrush(Qt::NoBrush);
                if (mRelationToLine.contains(relation))
                    painter->setPen(QPen(mRelationToLine[relation]->color1, 0));
            } else {
                if (relation->tags.contains(QLatin1String("amenity"))) continue;
                painter->setBrush(QColor(128,0,0,128));
                qDebug() << "*** multipolygon with no color" << relation->id << relation->tags;
            }
            painter->drawPath(path);
        }
#endif
        painter->setPen(Qt::black);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(posX,posY,TILE_SIZE*magnification,TILE_SIZE*magnification);
    }

    QPointF gpsToPixels(const GPSCoordinate &gps)
    {
        ProjectedCoordinate pc(gps);
        return projectedCoordToPixels(pc);
    }

    QPointF projectedCoordToPixels(const ProjectedCoordinate &pc)
    {
        int worldGridX1 = qFloor(mFile.minProjected().x * (1U << zoom)) * TILE_SIZE;
        int worldGridY1 = qFloor(mFile.minProjected().y * (1U << zoom)) * TILE_SIZE;

        double worldInPixels = (1u << zoom) * TILE_SIZE; // size of world in pixels
        return QPointF(pc.x * worldInPixels - worldGridX1,
                       pc.y * worldInPixels - worldGridY1);
    }

    ProjectedCoordinate itemToProjected(const QPointF &itemPos)
    {
        int worldGridX1 = qFloor(mFile.minProjected().x * (1U << zoom)) * TILE_SIZE;
        int worldGridY1 = qFloor(mFile.minProjected().y * (1U << zoom)) * TILE_SIZE;

        double worldInPixels = (1u << zoom) * TILE_SIZE; // size of world in pixels
        return ProjectedCoordinate((itemPos.x() + worldGridX1) / worldInPixels,
                (itemPos.y() + worldGridY1) / worldInPixels);
    }

    void hoverMoveEvent(QGraphicsSceneHoverEvent *event)
    {
        ProjectedCoordinate pc = itemToProjected(event->pos());

        QSet<OSM::Lookup::Object> objects = mLookup.objects(pc).toSet();
        if (objects != mHoverObjects) {
            qDebug() << "hoverMoveEvent #objects=" << objects.size();
            foreach (OSM::Lookup::Object lo, objects) {
                if (OSM::Way *way = lo.way) {
                    if (mWayToArea.contains(way))
                        qDebug() << "way-area" << way->id << way->tags;
                    if (mWayToLine.contains(way))
                        qDebug() << "way-line" << way->id << way->tags;
                    if (!mWayToArea.contains(way) && !mWayToLine.contains(way))
                        qDebug() << "way NO LINE/AREA" << way->id << way->tags;
                }
                if (OSM::Relation *relation = lo.relation) {
                    if (mRelationToArea.contains(relation))
                        qDebug() << "relation-area" << relation->id << relation->tags;
                    if (mRelationToLine.contains(relation))
                        qDebug() << "relation-line" << relation->id << relation->tags;
                    if (!mRelationToArea.contains(relation) && !mRelationToLine.contains(relation))
                        qDebug() << "relation NO LINE/AREA" << relation->id << relation->tags;
                }
            }
            mHoverObjects = objects;
            update();
        }

        qreal minDist = 100000;
        OSM::Node *closest = 0;
        QSet<OSM::Node*> nodes;
        foreach (OSM::Lookup::Object lo, objects) {
            if (lo.way) nodes += lo.way->nodes.toSet();
        }
        foreach (OSM::Node *node, nodes) {
            qreal dist = QLineF(event->pos(), gpsToPixels(node->gps)).length();
            if (dist < minDist) {
                closest = node;
                minDist = dist;
            }
        }
        if (closest != mClosestNode) {
            mClosestNode = closest;
            if (mClosestNode)
                qDebug() << "closest node is" << mClosestNode->id;
            update();
        }
    }

    OSM::File mFile;
    OSM::Lookup mLookup;
    OSM::RuleFile mRuleFile;
    int zoom;
    int magnification;
    QSet<OSM::Lookup::Object> mHoverObjects;
    OSM::Node *mClosestNode;
    QMap<QString,OSM::RuleFile::Area> mAreaByRuleID;
    QMap<OSM::Way*,OSM::RuleFile::Area*> mWayToArea;
    QMap<QString,OSM::RuleFile::Line> mLineByRuleID;
    QMap<OSM::Way*,OSM::RuleFile::Line*> mWayToLine;
    QMap<OSM::Relation*,OSM::RuleFile::Area*> mRelationToArea;
    QMap<OSM::Relation*,OSM::RuleFile::Line*> mRelationToLine;
    QMultiMap<OSM::Way*,OSM::Relation*> mWayToRelations; // can be > 1 relation per way
};

} // namespace WorldGen

void WorldGenWindow::osm()
{
    ui->view->scene()->clear();
#if 0
    OSMImporter imp;
    imp.Preprocess(QLatin1String("C:\\Programming\\OpenStreetMap\\Vancouver2.osm"));
#endif

#if 0
    QtileRenderer ren;
    QSettings settings;
    settings.beginGroup( QLatin1String("Qtile Renderer") );
    settings.setValue(QLatin1String("input"), QLatin1String("C:\\Programming\\OpenStreetMap\\Vancouver.osm"));
    settings.setValue(QLatin1String("rulesFile"), QLatin1String(":/rendering_rules/default.qrr"));
    settings.endGroup();
    ren.LoadSettings(&settings);
    ren.Preprocess(0, QLatin1String("C:\\Programming\\OpenStreetMap\\monav-data\\Vancouver"));
#endif

#if BERLIN
    QString dir = QLatin1String("C:\\Programming\\OpenStreetMap\\monav-data\\Berlin\\rendering_vector_(no_internet_required)");
#else
    QString dir = QLatin1String("C:\\Programming\\OpenStreetMap\\monav-data\\Vancouver");
#endif

    OSM::Node n1; n1.id = 1;
    OSM::Node n2; n2.id = 2;
    OSM::Node n3; n3.id = 3;
    OSM::Node n4; n4.id = 4;
    OSM::Way w1; w1.id = 1; w1.nodes << &n2 << &n3;
    OSM::Way w2; w2.id = 2; w2.nodes << &n1 << &n2;
    OSM::Way w3; w3.id = 3; w3.nodes << &n3 << &n4;
    QList<OSM::WayChain*> chains = OSM::File::makeChains(QList<OSM::Way*>() << &w1 << &w2 << &w3);
    foreach (OSM::WayChain *chain, chains) chain->print(); qDeleteAll(chains);

    w2.nodes.clear(); w2.nodes << &n2 << &n1;
    chains = OSM::File::makeChains(QList<OSM::Way*>() << &w1 << &w2 << &w3);
    foreach (OSM::WayChain *chain, chains) chain->print(); qDeleteAll(chains);
//    return;

    OpenStreetMapItem2 *item = new OpenStreetMapItem2(QLatin1String("C:\\Programming\\OpenStreetMap\\Vancouver2.osm"));
    ui->view->scene()->addItem(item);
    ui->view->scene()->setSceneRect(QRectF());
}
