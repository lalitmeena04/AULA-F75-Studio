#include <QGuiApplication>

#include "MainControllerClass.h"

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    MainControllerClass controller(app);
    return controller.run();
}