#include "glwindow.h"
#include "ui_glwindow.h"

#include <QtOpenGL>
#include <gl/GLU.h>

GLWindow::GLWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::GLWindow)
{
    ui->setupUi(this);

}

GLWindow::~GLWindow()
{
    delete ui;
}

/////

/////

#include "mapcomposite.h"
#include "mapmanager.h"
using namespace Tiled;

GLWidget::GLWidget(QWidget *parent) :
    QGLWidget(QGLFormat(QGL::SampleBuffers), parent)
{
//    QLatin1String mapName("C:/Users/Tim/Desktop/ProjectZomboid/Maps/tbx files/lot_house_medium_25_redbrick.tbx");
    QLatin1String mapName("C:/Users/Tim/Desktop/ProjectZomboid/Maps/4_3_Muldraugh_MainRd_02.tmx");
    mapInfo = MapManager::instance()->loadMap(mapName);
    if (mapInfo) {
        mapComposite = new MapComposite(mapInfo);
    }
}

void GLWidget::initializeGL()
{
    loadGLTextures();

    glEnable(GL_TEXTURE_2D);
//    glTexEnvf(GL_TEXTURE_2D,GL_TEXTURE_ENV_MODE,GL_REPLACE);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//    glDisable(GL_ALPHA_TEST);
    glShadeModel(GL_FLAT);
    glClearColor(0.5f, 0.5f, 0.5f, 0.0f);
    glClearDepth(1.0f);
//    glEnable(GL_DEPTH_TEST);
//    glDepthFunc(GL_LEQUAL);
//    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
}

static qreal SCALE = 0.25;

void GLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    glScalef(SCALE, SCALE, 1);
    glTranslatef(center.x(), center.y(), 0.0f);

#if 1
    if (mapInfo) {
#if 0
        // Textured poly!
//        QPolygonF poly = toScene(QRect(0, mapInfo->height() / 2, mapInfo->width()/ 2, mapInfo->height() - mapInfo->height() / 2), 0);
        QPolygonF poly = toScene(mapInfo->bounds(), 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBegin(GL_QUADS);
        glVertex3f(poly[0].x(), poly[0].y(), 0.0f);
        glVertex3f(poly[1].x(), poly[1].y(), 0.0f);
        glVertex3f(poly[2].x(), poly[2].y(), 0.0f);
        glVertex3f(poly[3].x(), poly[3].y(), 0.0f);
        glEnd();
#endif
        foreach (CompositeLayerGroup *lg, mapComposite->sortedLayerGroups()) {
            drawLayerGroup(lg, QRect(0, 0, width() / SCALE, height() / SCALE).translated(-center.x(), -center.y()));
//            break;
        }
    }
#else
    glTranslatef(-1.5f,0.0f,-6.0f);

    glBegin(GL_TRIANGLES);
    glVertex3f( 0.0f, 1.0f, 0.0f);
    glVertex3f(-1.0f,-1.0f, 0.0f);
    glVertex3f( 1.0f,-1.0f, 0.0f);
    glEnd();

    glTranslatef(3.0f,0.0f,0.0f);

    glBindTexture(GL_TEXTURE_2D, textures[QLatin1String("walls_interior_house_01")]);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f, 1.0f, 0.0f); // top-left
    glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f, 1.0f, 0.0f); // top-right
    glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f,-1.0f, 0.0f); // bottom-right
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f,-1.0f, 0.0f); // bottom-left
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);
#endif
}

void GLWidget::resizeGL(int width, int height)
{
    setupViewport(width, height);
}

void GLWidget::mousePressEvent(QMouseEvent *e)
{
    lastPos = e->pos();
}

QSize GLWidget::sizeHint() const
{
    return QSize(400, 400);
}

void GLWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (e->buttons() & Qt::LeftButton) {
        center.rx() += (e->x() - lastPos.x()) / SCALE;
        center.ry() += (lastPos.y() - e->y()) / SCALE;
        qDebug() << center;
        lastPos = e->pos();
        update();
    }
}

void GLWidget::setupViewport(int width, int height)
{
    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

#if 1
    if (mapInfo) {
        sceneRect = boundingRect(mapInfo->bounds());
        gluOrtho2D(0, width, 0, height);
        qDebug() << sceneRect;
    } else {
        sceneRect = QRectF(0, 0, width, height);
        gluOrtho2D(0, width, 0, height);
    }
#else
    gluPerspective(45.0f, (GLfloat)width/(GLfloat)height, 0.1f, 100.0f);
#endif

//    center.setX((width/2 - sceneRect.center().x()) / 2);
//    center.setY((height/2 - sceneRect.center().y()) / 2);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

#include "progress.h"
#include "tilemetainfomgr.h"
#include "tile.h"
#include "tileset.h"

void GLWidget::loadGLTextures()
{
    if (!mapInfo) return;

//    PROGRESS progress(QLatin1String("Loading Tilesets into OpenGL"));

    QImage t;
    QImage b;

    foreach (Tileset *ts, mapInfo->map()->usedTilesets()) {
        if (b.load(ts->imageSource())) {
#if 1
            QImage fixedImage(b.width(), b.height(), QImage::Format_ARGB32);
            QPainter painter(&fixedImage);
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.fillRect(fixedImage.rect(), Qt::transparent);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.drawImage( 0, 0, b);
            painter.end();
            b = fixedImage;
#elif 0
            QImage mask = b.createMaskFromColor(qRgb(255,255,255), Qt::MaskOutColor);
            b.setAlphaChannel(mask);
#else
            QImage mask = b.createMaskFromColor(Qt::white);
            QPixmap pixmap = QPixmap::fromImage(b);
            pixmap.setMask(QBitmap::fromImage(mask));
            b = pixmap.toImage();
#endif

            t = QGLWidget::convertToGLFormat( b );
            GLuint texture;
            glGenTextures( 1, &texture );
            glBindTexture( GL_TEXTURE_2D, texture );
            glTexImage2D( GL_TEXTURE_2D, 0, 4, t.width(), t.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, t.bits() );
            glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
            glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

            textures[ts->name()] = texture;
        }
    }

    qDebug() << "textures=" << textures;
}

QPointF GLWidget::toScene(qreal x, qreal y, int level)
{
    const int tileWidth = mapInfo->tileWidth();
    const int tileHeight = mapInfo->tileHeight();
    const int mapHeight = mapInfo->height();
    const int tilesPerLevel = 3;
    const int maxLevel = mapComposite->maxLevel();

    const int originX = mapHeight * tileWidth / 2; // top-left corner
    const int originY = tilesPerLevel * (maxLevel - level) * tileHeight;
    return QPointF((x - y) * tileWidth / 2 + originX,
                   (x + y) * tileHeight / 2 + originY);
}

QPolygonF GLWidget::toScene(const QRectF &worldRect, int level)
{
    QPolygonF pf;
    pf << toScene(worldRect.topLeft(), level);
    pf << toScene(worldRect.topRight(), level);
    pf << toScene(worldRect.bottomRight(), level);
    pf << toScene(worldRect.bottomLeft(), level);
    pf += pf.first();
    return pf;
}

QPointF GLWidget::toWorld(qreal x, qreal y, int level)
{
    const int tileWidth = mapInfo->tileWidth();
    const int tileHeight = mapInfo->tileHeight();
    const qreal ratio = (qreal) tileWidth / tileHeight;

    const int mapHeight = mapInfo->height();
    const int tilesPerLevel = 3;
    const int maxLevel = mapComposite->maxLevel();

    x -= mapHeight * tileWidth / 2;
    y -= tilesPerLevel * (maxLevel - level) * tileHeight;
    const qreal mx = y + (x / ratio);
    const qreal my = y - (x / ratio);

    return QPointF(mx / tileHeight,
                   my / tileHeight);
}

QRectF GLWidget::boundingRect(const QRect &tileRect)
{
    return toScene(tileRect, 0).boundingRect();
}

class DrawElements
{
public:
    void clear()
    {
        indices.resize(0);
        vertices.resize(0);
        texcoords.resize(0);
        textureids.resize(0);
    }

    void add(GLuint textureid,
             const QVector2D &uv1, const QVector2D &uv2, const QVector2D &uv3, const QVector2D &uv4,
             const QVector3D &v1, const QVector3D &v2, const QVector3D &v3, const QVector3D &v4)
    {
        indices << indices.size() << indices.size() + 1 << indices.size() + 2 << indices.size() + 3;
        textureids += textureid;
        texcoords << uv1 << uv2 << uv3 << uv4;
        vertices << v1 << v2 << v3 << v4;

        if (indices.size() > 1024 * 2)
            flush();
    }

    void flush()
    {
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, vertices.constData());
        glTexCoordPointer(2, GL_FLOAT, 0, texcoords.constData());
        for (int i = 0; i < textureids.size(); i++) {
            glBindTexture(GL_TEXTURE_2D, textureids[i]);
            glDrawElements(GL_QUADS, 4, GL_UNSIGNED_SHORT, indices.constData() + i * 4);
        }
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        clear();
    }

    QVector<GLushort> indices;
    QVector<QVector3D> vertices;
    QVector<QVector2D> texcoords;
    QVector<GLuint> textureids;

};

void GLWidget::drawLayerGroup(CompositeLayerGroup *layerGroup, const QRectF &exposed)
{
    int level = layerGroup->level();

    const int tileWidth = mapInfo->tileWidth();
    const int tileHeight = mapInfo->tileHeight();

    QRect rect = exposed.toAlignedRect();
    if (rect.isNull()) {
        Q_ASSERT(false);
        return;
    }

    QMargins drawMargins(0, 128 - 32, 0, 0); // ???

    rect.adjust(-drawMargins.right(),
                -drawMargins.bottom(),
                drawMargins.left(),
                drawMargins.top());

    // Determine the tile and pixel coordinates to start at
    QPointF tilePos = toWorld(rect.x(), rect.y(), level);
    QPoint rowItr = QPoint(qFloor(tilePos.x()), qFloor(tilePos.y()));
    QPointF startPos = toScene(rowItr.x(), rowItr.y(), level);
    startPos.rx() -= tileWidth / 2;
    startPos.ry() += tileHeight;

    /* Determine in which half of the tile the top-left corner of the area we
     * need to draw is. If we're in the upper half, we need to start one row
     * up due to those tiles being visible as well. How we go up one row
     * depends on whether we're in the left or right half of the tile.
     */
    const bool inUpperHalf = startPos.y() - rect.y() > tileHeight / 2;
    const bool inLeftHalf = rect.x() - startPos.x() < tileWidth / 2;

    if (inUpperHalf) {
        if (inLeftHalf) {
            --rowItr.rx();
            startPos.rx() -= tileWidth / 2;
        } else {
            --rowItr.ry();
            startPos.rx() += tileWidth / 2;
        }
        startPos.ry() -= tileHeight / 2;
    }

    // Determine whether the current row is shifted half a tile to the right
    bool shifted = inUpperHalf ^ inLeftHalf;

//    qreal opacity = painter->opacity();

    static DrawElements de; de.clear();

    static QVector<const Cell*> cells(40);
    static QVector<qreal> opacities(40);

    for (int y = startPos.y(); y - tileHeight < rect.bottom();
         y += tileHeight / 2) {
        QPoint columnItr = rowItr;

        for (int x = startPos.x(); x < rect.right(); x += tileWidth) {
            cells.resize(0);
            opacities.resize(0);
            if (mapInfo->bounds().contains(columnItr) && layerGroup->orderedCellsAt(columnItr, cells, opacities)) {
                for (int i = 0; i < cells.size(); i++) {
                    const Cell *cell = cells[i];
                    if (!cell->isEmpty()) {
                        Tileset *tileset = cell->tile->tileset();
                        if (textures.contains(tileset->name())) {
                            GLfloat tileX = (cell->tile->id() % tileset->columnCount()) * 64;
                            GLfloat tileY = (cell->tile->id() / tileset->columnCount()) * 128;

                            int texW = tileset->imageWidth(), texH = tileset->imageHeight();
                            tileY = texH - tileY; // invert Y-axis for OpenGL

//                            GLfloat gx = x; // left of tile image pos
                            GLfloat gy = sceneRect.bottom() - y; // bottom of tile image pos

#if 1
                            de.add(textures[tileset->name()],
                                    QVector2D(tileX/texW,         tileY/texH),
                                    QVector2D((tileX+64.0f)/texW, tileY/texH),
                                    QVector2D((tileX+64.0f)/texW, (tileY-128.0f)/texH),
                                    QVector2D(tileX/texW,         (tileY-128.0f)/texH),
                                    QVector3D(x,      gy + 128, 0.0f),
                                    QVector3D(x + 64, gy + 128, 0.0f),
                                    QVector3D(x + 64, gy, 0.0f),
                                    QVector3D(x,      gy, 0.0f));
#elif 0
                            textureids += textures[tileset->name()];
                            indices << vertices.size() << vertices.size() + 1 << vertices.size() + 2 << vertices.size() + 3;
                            texcoords += QVector2D(tileX/texW,         tileY/texH); vertices += QVector3D(x,      gy + 128, 0.0f); // top-left
                            texcoords += QVector2D((tileX+64.0f)/texW, tileY/texH); vertices += QVector3D(x + 64, gy + 128, 0.0f); // top-right
                            texcoords += QVector2D((tileX+64.0f)/texW, (tileY-128.0f)/texH); vertices += QVector3D(x + 64, gy, 0.0f); // bottom-right
                            texcoords += QVector2D(tileX/texW,         (tileY-128.0f)/texH); vertices += QVector3D(x,      gy, 0.0f); // bottom-left
#else
                            glBindTexture(GL_TEXTURE_2D, textures[tileset->name()]);
                            glBegin(GL_QUADS);
                            glTexCoord2f(tileX/texW,         tileY/texH);          glVertex3f(x,      gy + 128, 0.0f); // top-left
                            glTexCoord2f((tileX+64.0f)/texW, tileY/texH);          glVertex3f(x + 64, gy + 128, 0.0f); // top-right
                            glTexCoord2f((tileX+64.0f)/texW, (tileY-128.0f)/texH); glVertex3f(x + 64, gy, 0.0f); // bottom-right
                            glTexCoord2f(tileX/texW,         (tileY-128.0f)/texH); glVertex3f(x,      gy, 0.0f); // bottom-left
                            glEnd();
#endif
                        }
                    }
                }
            }

            // Advance to the next column
            ++columnItr.rx();
            --columnItr.ry();
        }

        // Advance to the next row
        if (!shifted) {
            ++rowItr.rx();
            startPos.rx() += tileWidth / 2;
            shifted = true;
        } else {
            ++rowItr.ry();
            startPos.rx() -= tileWidth / 2;
            shifted = false;
        }
    }

//    glBindTexture(GL_TEXTURE_2D, textures.values().first());

#if 1
    de.flush();
#else
    qDebug() << textureids.size() << " textures to draw";
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    for (int offset = 0; offset < textureids.size(); offset += 10000) {
        glVertexPointer(3, GL_FLOAT, 0, vertices.constData() + offset);
        glTexCoordPointer(2, GL_FLOAT, 0, texcoords.constData() + offset);
        for (int i = offset; i < textureids.size(); i++) {
            glBindTexture(GL_TEXTURE_2D, textureids[i]);
            glDrawElements(GL_QUADS, 4, GL_UNSIGNED_SHORT, indices.constData() + (i - offset) * 4);
        }
    }
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
#endif

}

/////














#if 0
#include <QtOpenGL>
#include <stdlib.h>
#include <math.h>

#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE  0x809D
#endif

//! [0]
GLWidget::GLWidget(QWidget *parent)
    : QGLWidget(QGLFormat(QGL::SampleBuffers), parent)
{
    QTime midnight(0, 0, 0);
    qsrand(midnight.secsTo(QTime::currentTime()));

    logo = 0;
    xRot = 0;
    yRot = 0;
    zRot = 0;

    qtGreen = QColor::fromCmykF(0.40, 0.0, 1.0, 0.0);
    qtPurple = QColor::fromCmykF(0.39, 0.39, 0.0, 0.0);

    animationTimer.setSingleShot(false);
    connect(&animationTimer, SIGNAL(timeout()), this, SLOT(animate()));
    animationTimer.start(25);

    setAutoFillBackground(false);
    setMinimumSize(200, 200);
    setWindowTitle(tr("Overpainting a Scene"));
}
//! [0]

//! [1]
GLWidget::~GLWidget()
{
}
//! [1]

static void qNormalizeAngle(int &angle)
{
    while (angle < 0)
        angle += 360 * 16;
    while (angle > 360 * 16)
        angle -= 360 * 16;
}

void GLWidget::setXRotation(int angle)
{
    qNormalizeAngle(angle);
    if (angle != xRot)
        xRot = angle;
}

void GLWidget::setYRotation(int angle)
{
    qNormalizeAngle(angle);
    if (angle != yRot)
        yRot = angle;
}

void GLWidget::setZRotation(int angle)
{
    qNormalizeAngle(angle);
    if (angle != zRot)
        zRot = angle;
}

//! [2]
void GLWidget::initializeGL()
{
    glEnable(GL_MULTISAMPLE);

    logo = new QtLogo(this);
    logo->setColor(qtGreen.dark());
}
//! [2]

void GLWidget::mousePressEvent(QMouseEvent *event)
{
    lastPos = event->pos();
}

void GLWidget::mouseMoveEvent(QMouseEvent *event)
{
    int dx = event->x() - lastPos.x();
    int dy = event->y() - lastPos.y();

    if (event->buttons() & Qt::LeftButton) {
        setXRotation(xRot + 8 * dy);
        setYRotation(yRot + 8 * dx);
    } else if (event->buttons() & Qt::RightButton) {
        setXRotation(xRot + 8 * dy);
        setZRotation(zRot + 8 * dx);
    }
    lastPos = event->pos();
}

void GLWidget::paintEvent(QPaintEvent *event)
{
    makeCurrent();
//! [4]
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
//! [4]

//! [6]
    qglClearColor(qtPurple.dark());
    glShadeModel(GL_SMOOTH);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_MULTISAMPLE);
    static GLfloat lightPosition[4] = { 0.5, 5.0, 7.0, 1.0 };
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);

    setupViewport(width(), height());
//! [6]

//! [7]
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glTranslatef(0.0, 0.0, -10.0);
    glRotatef(xRot / 16.0, 1.0, 0.0, 0.0);
    glRotatef(yRot / 16.0, 0.0, 1.0, 0.0);
    glRotatef(zRot / 16.0, 0.0, 0.0, 1.0);

    logo->draw();
//! [7]

//! [8]
    glShadeModel(GL_FLAT);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
//! [8]

//! [10]
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    foreach (Bubble *bubble, bubbles) {
        if (bubble->rect().intersects(event->rect()))
            bubble->drawBubble(&painter);
    }
    drawInstructions(&painter);
    painter.end();
}
//! [10]

//! [11]
void GLWidget::resizeGL(int width, int height)
{
    setupViewport(width, height);
}
//! [11]

//! [12]
void GLWidget::showEvent(QShowEvent *event)
{
    Q_UNUSED(event);
    createBubbles(20 - bubbles.count());
}
//! [12]

QSize GLWidget::sizeHint() const
{
    return QSize(400, 400);
}

void GLWidget::createBubbles(int number)
{
    for (int i = 0; i < number; ++i) {
        QPointF position(width()*(0.1 + (0.8*qrand()/(RAND_MAX+1.0))),
                        height()*(0.1 + (0.8*qrand()/(RAND_MAX+1.0))));
        qreal radius = qMin(width(), height())*(0.0125 + 0.0875*qrand()/(RAND_MAX+1.0));
        QPointF velocity(width()*0.0125*(-0.5 + qrand()/(RAND_MAX+1.0)),
                        height()*0.0125*(-0.5 + qrand()/(RAND_MAX+1.0)));

        bubbles.append(new Bubble(position, radius, velocity));
    }
}

//! [13]
void GLWidget::animate()
{
    QMutableListIterator<Bubble*> iter(bubbles);

    while (iter.hasNext()) {
        Bubble *bubble = iter.next();
        bubble->move(rect());
    }
    update();
}
//! [13]

//! [14]
void GLWidget::setupViewport(int width, int height)
{
    int side = qMin(width, height);
    glViewport((width - side) / 2, (height - side) / 2, side, side);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
#ifdef QT_OPENGL_ES
    glOrthof(-0.5, +0.5, -0.5, 0.5, 4.0, 15.0);
#else
    glOrtho(-0.5, +0.5, -0.5, 0.5, 4.0, 15.0);
#endif
    glMatrixMode(GL_MODELVIEW);
}
//! [14]

//! [15]
void GLWidget::drawInstructions(QPainter *painter)
{
    QString text = tr("Click and drag with the left mouse button "
                      "to rotate the Qt logo.");
    QFontMetrics metrics = QFontMetrics(font());
    int border = qMax(4, metrics.leading());

    QRect rect = metrics.boundingRect(0, 0, width() - 2*border, int(height()*0.125),
                                      Qt::AlignCenter | Qt::TextWordWrap, text);
    painter->setRenderHint(QPainter::TextAntialiasing);
    painter->fillRect(QRect(0, 0, width(), rect.height() + 2*border),
                     QColor(0, 0, 0, 127));
    painter->setPen(Qt::white);
    painter->fillRect(QRect(0, 0, width(), rect.height() + 2*border),
                      QColor(0, 0, 0, 127));
    painter->drawText((width() - rect.width())/2, border,
                      rect.width(), rect.height(),
                      Qt::AlignCenter | Qt::TextWordWrap, text);
}
//! [15]

/////

Bubble::Bubble(const QPointF &position, qreal radius, const QPointF &velocity)
    : position(position), vel(velocity), radius(radius)
{
    innerColor = randomColor();
    outerColor = randomColor();
    updateBrush();
}

void Bubble::updateBrush()
{
    QRadialGradient gradient(QPointF(radius, radius), radius,
                             QPointF(radius*0.5, radius*0.5));

    gradient.setColorAt(0, QColor(255, 255, 255, 255));
    gradient.setColorAt(0.25, innerColor);
    gradient.setColorAt(1, outerColor);
    brush = QBrush(gradient);
}

void Bubble::drawBubble(QPainter *painter)
{
    painter->save();
    painter->translate(position.x() - radius, position.y() - radius);
    painter->setBrush(brush);
    painter->drawEllipse(0, 0, int(2*radius), int(2*radius));
    painter->restore();
}

QColor Bubble::randomColor()
{
    int red = int(205 + 50.0*qrand()/(RAND_MAX+1.0));
    int green = int(205 + 50.0*qrand()/(RAND_MAX+1.0));
    int blue = int(205 + 50.0*qrand()/(RAND_MAX+1.0));
    int alpha = int(91 + 100.0*qrand()/(RAND_MAX+1.0));

    return QColor(red, green, blue, alpha);
}

void Bubble::move(const QRect &bbox)
{
    position += vel;
    qreal leftOverflow = position.x() - radius - bbox.left();
    qreal rightOverflow = position.x() + radius - bbox.right();
    qreal topOverflow = position.y() - radius - bbox.top();
    qreal bottomOverflow = position.y() + radius - bbox.bottom();

    if (leftOverflow < 0.0) {
        position.setX(position.x() - 2 * leftOverflow);
        vel.setX(-vel.x());
    } else if (rightOverflow > 0.0) {
        position.setX(position.x() - 2 * rightOverflow);
        vel.setX(-vel.x());
    }

    if (topOverflow < 0.0) {
        position.setY(position.y() - 2 * topOverflow);
        vel.setY(-vel.y());
    } else if (bottomOverflow > 0.0) {
        position.setY(position.y() - 2 * bottomOverflow);
        vel.setY(-vel.y());
    }
}

QRectF Bubble::rect()
{
    return QRectF(position.x() - radius, position.y() - radius,
                  2 * radius, 2 * radius);
}

/////

#include <QMatrix4x4>
#include <QVector3D>

#include <qmath.h>

static const qreal tee_height = 0.311126;
static const qreal cross_width = 0.25;
static const qreal bar_thickness = 0.113137;
static const qreal inside_diam = 0.20;
static const qreal outside_diam = 0.30;
static const qreal logo_depth = 0.10;
static const int num_divisions = 32;

//! [0]
struct Geometry
{
    QVector<GLushort> faces;
    QVector<QVector3D> vertices;
    QVector<QVector3D> normals;
    void appendSmooth(const QVector3D &a, const QVector3D &n, int from);
    void appendFaceted(const QVector3D &a, const QVector3D &n);
    void finalize();
    void loadArrays() const;
};
//! [0]

//! [1]
class Patch
{
public:
    enum Smoothing { Faceted, Smooth };
    Patch(Geometry *);
    void setSmoothing(Smoothing s) { sm = s; }
    void translate(const QVector3D &t);
    void rotate(qreal deg, QVector3D axis);
    void draw() const;
    void addTri(const QVector3D &a, const QVector3D &b, const QVector3D &c, const QVector3D &n);
    void addQuad(const QVector3D &a, const QVector3D &b,  const QVector3D &c, const QVector3D &d);

    GLushort start;
    GLushort count;
    GLushort initv;

    GLfloat faceColor[4];
    QMatrix4x4 mat;
    Smoothing sm;
    Geometry *geom;
};
//! [1]

static inline void qSetColor(float colorVec[], QColor c)
{
    colorVec[0] = c.redF();
    colorVec[1] = c.greenF();
    colorVec[2] = c.blueF();
    colorVec[3] = c.alphaF();
}

void Geometry::loadArrays() const
{
    glVertexPointer(3, GL_FLOAT, 0, vertices.constData());
    glNormalPointer(GL_FLOAT, 0, normals.constData());
}

void Geometry::finalize()
{
    // TODO: add vertex buffer uploading here

    // Finish smoothing normals by ensuring accumulated normals are returned
    // to length 1.0.
    for (int i = 0; i < normals.count(); ++i)
        normals[i].normalize();
}

void Geometry::appendSmooth(const QVector3D &a, const QVector3D &n, int from)
{
    // Smooth normals are acheived by averaging the normals for faces meeting
    // at a point.  First find the point in geometry already generated
    // (working backwards, since most often the points shared are between faces
    // recently added).
    int v = vertices.count() - 1;
    for ( ; v >= from; --v)
        if (qFuzzyCompare(vertices[v], a))
            break;
    if (v < from)
    {
        // The vert was not found so add it as a new one, and initialize
        // its corresponding normal
        v = vertices.count();
        vertices.append(a);
        normals.append(n);
    }
    else
    {
        // Vert found, accumulate normals into corresponding normal slot.
        // Must call finalize once finished accumulating normals
        normals[v] += n;
    }
    // In both cases (found or not) reference the vert via its index
    faces.append(v);
}

void Geometry::appendFaceted(const QVector3D &a, const QVector3D &n)
{
    // Faceted normals are achieved by duplicating the vert for every
    // normal, so that faces meeting at a vert get a sharp edge.
    int v = vertices.count();
    vertices.append(a);
    normals.append(n);
    faces.append(v);
}

Patch::Patch(Geometry *g)
   : start(g->faces.count())
   , count(0)
   , initv(g->vertices.count())
   , sm(Patch::Smooth)
   , geom(g)
{
    qSetColor(faceColor, QColor(Qt::darkGray));
}

void Patch::rotate(qreal deg, QVector3D axis)
{
    mat.rotate(deg, axis);
}

void Patch::translate(const QVector3D &t)
{
    mat.translate(t);
}

static inline void qMultMatrix(const QMatrix4x4 &mat)
{
    if (sizeof(qreal) == sizeof(GLfloat))
        glMultMatrixf((GLfloat*)mat.constData());
#ifndef QT_OPENGL_ES
    else if (sizeof(qreal) == sizeof(GLdouble))
        glMultMatrixd((GLdouble*)mat.constData());
#endif
    else
    {
        GLfloat fmat[16];
        qreal const *r = mat.constData();
        for (int i = 0; i < 16; ++i)
            fmat[i] = r[i];
        glMultMatrixf(fmat);
    }
}

//! [2]
void Patch::draw() const
{
    glPushMatrix();
    qMultMatrix(mat);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, faceColor);

    const GLushort *indices = geom->faces.constData();
    glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, indices + start);
    glPopMatrix();
}
//! [2]

void Patch::addTri(const QVector3D &a, const QVector3D &b, const QVector3D &c, const QVector3D &n)
{
    QVector3D norm = n.isNull() ? QVector3D::normal(a, b, c) : n;
    if (sm == Smooth)
    {
        geom->appendSmooth(a, norm, initv);
        geom->appendSmooth(b, norm, initv);
        geom->appendSmooth(c, norm, initv);
    }
    else
    {
        geom->appendFaceted(a, norm);
        geom->appendFaceted(b, norm);
        geom->appendFaceted(c, norm);
    }
    count += 3;
}

void Patch::addQuad(const QVector3D &a, const QVector3D &b,  const QVector3D &c, const QVector3D &d)
{
    QVector3D norm = QVector3D::normal(a, b, c);
    if (sm == Smooth)
    {
        addTri(a, b, c, norm);
        addTri(a, c, d, norm);
    }
    else
    {
        // If faceted share the two common verts
        addTri(a, b, c, norm);
        int k = geom->vertices.count();
        geom->appendSmooth(a, norm, k);
        geom->appendSmooth(c, norm, k);
        geom->appendFaceted(d, norm);
        count += 3;
    }
}

static inline QVector<QVector3D> extrude(const QVector<QVector3D> &verts, qreal depth)
{
    QVector<QVector3D> extr = verts;
    for (int v = 0; v < extr.count(); ++v)
        extr[v].setZ(extr[v].z() - depth);
    return extr;
}

class Rectoid
{
public:
    void translate(const QVector3D &t)
    {
        for (int i = 0; i < parts.count(); ++i)
            parts[i]->translate(t);
    }
    void rotate(qreal deg, QVector3D axis)
    {
        for (int i = 0; i < parts.count(); ++i)
            parts[i]->rotate(deg, axis);
    }

    // No special Rectoid destructor - the parts are fetched out of this member
    // variable, and destroyed by the new owner
    QList<Patch*> parts;
};

class RectPrism : public Rectoid
{
public:
    RectPrism(Geometry *g, qreal width, qreal height, qreal depth);
};

RectPrism::RectPrism(Geometry *g, qreal width, qreal height, qreal depth)
{
    enum { bl, br, tr, tl };
    Patch *fb = new Patch(g);
    fb->setSmoothing(Patch::Faceted);

    // front face
    QVector<QVector3D> r(4);
    r[br].setX(width);
    r[tr].setX(width);
    r[tr].setY(height);
    r[tl].setY(height);
    QVector3D adjToCenter(-width / 2.0, -height / 2.0, depth / 2.0);
    for (int i = 0; i < 4; ++i)
        r[i] += adjToCenter;
    fb->addQuad(r[bl], r[br], r[tr], r[tl]);

    // back face
    QVector<QVector3D> s = extrude(r, depth);
    fb->addQuad(s[tl], s[tr], s[br], s[bl]);

    // side faces
    Patch *sides = new Patch(g);
    sides->setSmoothing(Patch::Faceted);
    sides->addQuad(s[bl], s[br], r[br], r[bl]);
    sides->addQuad(s[br], s[tr], r[tr], r[br]);
    sides->addQuad(s[tr], s[tl], r[tl], r[tr]);
    sides->addQuad(s[tl], s[bl], r[bl], r[tl]);

    parts << fb << sides;
}

class RectTorus : public Rectoid
{
public:
    RectTorus(Geometry *g, qreal iRad, qreal oRad, qreal depth, int numSectors);
};

RectTorus::RectTorus(Geometry *g, qreal iRad, qreal oRad, qreal depth, int k)
{
    QVector<QVector3D> inside;
    QVector<QVector3D> outside;
    for (int i = 0; i < k; ++i) {
        qreal angle = (i * 2 * M_PI) / k;
        inside << QVector3D(iRad * qSin(angle), iRad * qCos(angle), depth / 2.0);
        outside << QVector3D(oRad * qSin(angle), oRad * qCos(angle), depth / 2.0);
    }
    inside << QVector3D(0.0, iRad, 0.0);
    outside << QVector3D(0.0, oRad, 0.0);
    QVector<QVector3D> in_back = extrude(inside, depth);
    QVector<QVector3D> out_back = extrude(outside, depth);

    // Create front, back and sides as separate patches so that smooth normals
    // are generated for the curving sides, but a faceted edge is created between
    // sides and front/back
    Patch *front = new Patch(g);
    for (int i = 0; i < k; ++i)
        front->addQuad(outside[i], inside[i],
                       inside[(i + 1) % k], outside[(i + 1) % k]);
    Patch *back = new Patch(g);
    for (int i = 0; i < k; ++i)
        back->addQuad(in_back[i], out_back[i],
                      out_back[(i + 1) % k], in_back[(i + 1) % k]);
    Patch *is = new Patch(g);
    for (int i = 0; i < k; ++i)
        is->addQuad(in_back[i], in_back[(i + 1) % k],
                    inside[(i + 1) % k], inside[i]);
    Patch *os = new Patch(g);
    for (int i = 0; i < k; ++i)
        os->addQuad(out_back[(i + 1) % k], out_back[i],
                    outside[i], outside[(i + 1) % k]);
    parts << front << back << is << os;
}

QtLogo::QtLogo(QObject *parent, int divisions, qreal scale)
    : QObject(parent)
    , geom(new Geometry())
{
    buildGeometry(divisions, scale);
}

QtLogo::~QtLogo()
{
    qDeleteAll(parts);
    delete geom;
}

void QtLogo::setColor(QColor c)
{
    for (int i = 0; i < parts.count(); ++i)
        qSetColor(parts[i]->faceColor, c);
}

//! [3]
void QtLogo::buildGeometry(int divisions, qreal scale)
{
    qreal cw = cross_width * scale;
    qreal bt = bar_thickness * scale;
    qreal ld = logo_depth * scale;
    qreal th = tee_height *scale;

    RectPrism cross(geom, cw, bt, ld);
    RectPrism stem(geom, bt, th, ld);

    QVector3D z(0.0, 0.0, 1.0);
    cross.rotate(45.0, z);
    stem.rotate(45.0, z);

    qreal stem_downshift = (th + bt) / 2.0;
    stem.translate(QVector3D(0.0, -stem_downshift, 0.0));

    RectTorus body(geom, 0.20, 0.30, 0.1, divisions);

    parts << stem.parts << cross.parts << body.parts;

    geom->finalize();
}
//! [3]

//! [4]
void QtLogo::draw() const
{
    geom->loadArrays();

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);

    for (int i = 0; i < parts.count(); ++i)
        parts[i]->draw();

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
}
//! [4]
#endif


