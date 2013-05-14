#include "luaconsole.h"
#include "ui_luaconsole.h"

#include "mainwindow.h"

#include <QDesktopServices>
#include <QUrl>

#ifdef WORLDED
#else
using namespace Tiled;
using namespace Internal;
#endif

LuaConsole *LuaConsole::mInstance = 0;

LuaConsole *LuaConsole::instance()
{
    if (!mInstance)
        mInstance = new LuaConsole;
    return mInstance;
}

LuaConsole::LuaConsole(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::LuaConsole)
{
    setWindowFlags(windowFlags() | Qt::Tool);

    ui->setupUi(this);

    connect(ui->actionRun_Script, SIGNAL(triggered()), SLOT(runScript()));
    connect(ui->actionRunAgain, SIGNAL(triggered()), SLOT(runAgain()));
    connect(ui->btnRunAgain, SIGNAL(clicked()), SLOT(runAgain()));

    connect(ui->clear, SIGNAL(clicked()), SLOT(clear()));

    connect(ui->actionHelpContents, SIGNAL(triggered()), SLOT(helpContents()));
}

LuaConsole::~LuaConsole()
{
    delete ui;
}


void LuaConsole::runScript()
{
#ifndef WORLDED
    MainWindow::instance()->LuaScript(QString());
#endif
}

void LuaConsole::runAgain()
{
#ifndef WORLDED
    MainWindow::instance()->LuaScript(mFileName);
#endif
}

void LuaConsole::clear()
{
    ui->textEdit->clear();
}

void LuaConsole::writestring(const char *s, int len)
{
    ui->textEdit->moveCursor(QTextCursor::End);
    ui->textEdit->insertPlainText(QString::fromLatin1(s, len));
}

void LuaConsole::writeline()
{
    ui->textEdit->moveCursor(QTextCursor::End);
    ui->textEdit->insertPlainText(QLatin1String("\n"));
}

void LuaConsole::write(const QString &s, QColor color)
{
    if (s.isEmpty()) return;

    ui->textEdit->moveCursor(QTextCursor::End);
    ui->textEdit->setTextColor(color);
    ui->textEdit->insertPlainText(s);

//    ui->textEdit->moveCursor(QTextCursor::End);
    ui->textEdit->setTextColor(Qt::black);
    ui->textEdit->insertPlainText(QLatin1String("\n"));
}

void LuaConsole::helpContents()
{
    QUrl url = QUrl::fromLocalFile(
            QCoreApplication::applicationDirPath() + QLatin1Char('/')
            + QLatin1String("docs/TileZed/LuaScripting.html"));
    QDesktopServices::openUrl(url);
}
