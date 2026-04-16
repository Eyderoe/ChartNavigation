#include "enroute_widget.hpp"
#include "ui_enroute_widget.h"


enroute_widget::enroute_widget(QWidget *parent) :
    QWidget(parent), ui(new Ui::enroute_widget) {
    ui->setupUi(this);
}

enroute_widget::~enroute_widget() {
    delete ui;
}
