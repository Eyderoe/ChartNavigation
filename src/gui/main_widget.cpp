#include "main_widget.hpp"
#include "ui_main_widget.h"


main_widget::main_widget (QWidget *parent) :
    QWidget(parent), ui(new Ui::main_widget) {
    ui->setupUi(this);
    xp.addPlaneInfo();
}

main_widget::~main_widget () {
    delete ui;
}
