#include "license.hpp"
#include "ui_license.h"


license::license (QWidget *parent) :
    QDialog(parent), ui(new Ui::license) {
    ui->setupUi(this);
}

license::~license () {
    delete ui;
}