#ifndef LUACONSOLE_H
#define LUACONSOLE_H

#include <QMainWindow>

namespace Ui {
class LuaConsole;
}

class LuaConsole : public QMainWindow
{
    Q_OBJECT
    
public:
    static LuaConsole *instance();
    static void deleteInstance();

    void setFile(const QString &fileName)
    { mFileName = fileName; }

    void write(const QString &s, QColor color = Qt::black);

    // These are the luai_write* implementations!
    void writestring(const char *s, int len);
    void writeline();

private slots:
    void runScript();
    void runAgain();
    void clear();
    void helpContents();

private:
    Q_DISABLE_COPY(LuaConsole)
    static LuaConsole *mInstance;
    explicit LuaConsole(QWidget *parent = 0);
    ~LuaConsole();

    Ui::LuaConsole *ui;
    QString mFileName;
};

#endif // LUACONSOLE_H
