#ifndef WORLDGENWINDOW_H
#define WORLDGENWINDOW_H

#include <QMainWindow>

namespace Ui {
class WorldGenWindow;
}

namespace WorldGen {



class WorldGenWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    static WorldGenWindow *instance();
    static void deleteInstance();
    
private:
    Q_DISABLE_COPY(WorldGenWindow)
    explicit WorldGenWindow(QWidget *parent = 0);
    ~WorldGenWindow();

    static WorldGenWindow *mInstance;

    Ui::WorldGenWindow *ui;
};

} // namespace WorldGen

#endif // WORLDGENWINDOW_H
