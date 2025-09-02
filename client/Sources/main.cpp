#include <QApplication>
#include "demo_app.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    DemoApp window;
    window.show();
    
    return app.exec();
}