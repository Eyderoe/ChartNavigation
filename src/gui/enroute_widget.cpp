#include "enroute_widget.hpp"
#include "ui_enroute_widget.h"
#include "services/dataProvider.hpp"


enroute_widget::enroute_widget(QWidget *parent) :
    QWidget(parent), ui(new Ui::enroute_widget) {
    ui->setupUi(this);
}

void enroute_widget::setDataProvider (DataProvider *provider) {
    dataProvider = provider;
}

enroute_widget::~enroute_widget() {
    delete ui;
}
