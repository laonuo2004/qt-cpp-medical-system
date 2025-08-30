#include <QApplication>
#include "engine.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Engine *engine = Engine::instance();
    engine->startApplicationFlow(); // 调用新的启动函数
    return a.exec();
}
