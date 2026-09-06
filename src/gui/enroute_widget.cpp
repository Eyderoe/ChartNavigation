#include "enroute_widget.hpp"
#include "ui_enroute_widget.h"
#include "services/dataProvider.hpp"


enroute_widget::enroute_widget(QWidget *parent) :
    QWidget(parent), ui(new Ui::enroute_widget) {
    ui->setupUi(this);
}

void enroute_widget::setDataProvider (DataProvider *provider) {
    dataProvider = provider;
    ui->graphicsView->setDataProvider(provider);
}

void enroute_widget::setAttachedChart (AttachedChart chart) {
    ui->graphicsView->setAttachedChart(std::move(chart));
}

void enroute_widget::clearAttachedChart () {
    ui->graphicsView->clearAttachedChart();
}

bool enroute_widget::hasAttachedChart () const noexcept {
    return ui->graphicsView->hasAttachedChart();
}

enroute_widget::~enroute_widget() {
    delete ui;
}
