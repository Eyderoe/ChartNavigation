#include "about_dialog.hpp"
#include "ui_about_dialog.h"


about_dialog::about_dialog (QWidget *parent) :
    QDialog(parent), ui(new Ui::about_dialog) {
    ui->setupUi(this);
}

about_dialog::~about_dialog () {
    delete ui;
}