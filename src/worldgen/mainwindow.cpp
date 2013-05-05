#include "mainwindow.h"
#include "ui_mainwindow.h"

using namespace WorldGen;

WorldGenWindow *WorldGenWindow::mInstance = 0;

WorldGenWindow *WorldGenWindow::instance()
{
    if (!mInstance)
        mInstance = new WorldGenWindow;
    return mInstance;
}

WorldGenWindow::WorldGenWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::WorldGenWindow)
{
    ui->setupUi(this);
}

WorldGenWindow::~WorldGenWindow()
{
    delete ui;
}

