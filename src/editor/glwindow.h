#ifndef GLWINDOW_H
#define GLWINDOW_H

#include <QMainWindow>
#include <QGLWidget>

namespace Ui {
class GLWindow;
}

#if 0
#include <QObject>
#include <QColor>

class Patch;
struct Geometry;

//! [0]
class QtLogo : public QObject
{
public:
    QtLogo(QObject *parent, int d = 64, qreal s = 1.0);
    ~QtLogo();
    void setColor(QColor c);
    void draw() const;
private:
    void buildGeometry(int d, qreal s);

    QList<Patch *> parts;
    Geometry *geom;
};
//! [0]

#include <QBrush>
#include <QColor>
#include <QPointF>
#include <QRect>
#include <QRectF>

class Bubble
{
public:
    Bubble(const QPointF &position, qreal radius, const QPointF &velocity);

    void drawBubble(QPainter *painter);
    void updateBrush();
    void move(const QRect &bbox);
    QRectF rect();

private:
    QColor randomColor();

    QBrush brush;
    QPointF position;
    QPointF vel;
    qreal radius;
    QColor innerColor;
    QColor outerColor;
};

#include <QBrush>
#include <QFont>
#include <QImage>
#include <QPen>
#include <QGLWidget>
#include <QTimer>

class GLWidget : public QGLWidget
{
    Q_OBJECT

public:
    GLWidget(QWidget *parent = 0);
    ~GLWidget();
//! [0]

    QSize sizeHint() const;
    int xRotation() const { return xRot; }
    int yRotation() const { return yRot; }
    int zRotation() const { return zRot; }

public slots:
    void setXRotation(int angle);
    void setYRotation(int angle);
    void setZRotation(int angle);

//! [1]
protected:
    void initializeGL();
    void paintEvent(QPaintEvent *event);
    void resizeGL(int width, int height);
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void showEvent(QShowEvent *event);

private slots:
    void animate();

private:
    void createBubbles(int number);
    void drawInstructions(QPainter *painter);
//! [1]
    void setupViewport(int width, int height);

    QColor qtGreen;
    QColor qtPurple;

    GLuint object;
    int xRot;
    int yRot;
    int zRot;
    QPoint lastPos;
//! [4]
    QtLogo *logo;
    QList<Bubble*> bubbles;
    QTimer animationTimer;
//! [4]
};
#endif

class CompositeLayerGroup;
class MapComposite;
class MapInfo;

class GLWidget : public QGLWidget
{
    Q_OBJECT

public:
    GLWidget(QWidget *parent = 0);
//    ~GLWidget();

    QSize sizeHint() const;

protected:
    void initializeGL();
    void paintGL();
    void resizeGL(int width, int height);

    void mousePressEvent(QMouseEvent *e);
    void mouseMoveEvent(QMouseEvent *e);

private:
    void setupViewport(int width, int height);
    void loadGLTextures();
    QPointF toScene(qreal x, qreal y, int level);
    QPointF toScene(const QPointF &p, int level) { return toScene(p.x(), p.y(), level); }
    QPolygonF toScene(const QRectF &worldRect, int level);
    QPointF toWorld(qreal x, qreal y, int level);
    QRectF boundingRect(const QRect &tileRect);
    void drawLayerGroup(CompositeLayerGroup *layerGroup, const QRectF &exposed);

    QMap<QString,GLuint> textures;
    MapInfo *mapInfo;
    MapComposite *mapComposite;
    QRectF sceneRect;
    QPoint center;
    QPoint lastPos;
};

class GLWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit GLWindow(QWidget *parent = 0);
    ~GLWindow();
    
private:
    Ui::GLWindow *ui;
};

#endif // GLWINDOW_H
