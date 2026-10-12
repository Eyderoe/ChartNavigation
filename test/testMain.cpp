#define DOCTEST_CONFIG_IMPLEMENT

#include "doctest.h"
#include <QCoreApplication>


int main (int argc, char **argv) {
    QCoreApplication application(argc, argv);
    doctest::Context context(argc, argv);
    return context.run();
}
