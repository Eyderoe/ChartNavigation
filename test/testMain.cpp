#define DOCTEST_CONFIG_IMPLEMENT

#include "doctest.h"
#include <QApplication>


int main (int argc, char **argv) {
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication application(argc, argv);
    doctest::Context context(argc, argv);
    return context.run();
}
