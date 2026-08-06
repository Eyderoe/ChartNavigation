#include "stackedWidget.hpp"


StackedWidget::StackedWidget (QWidget *parent) : QStackedWidget(parent) {
    dataProviderPtr = new DataProvider(this);
}

DataProvider* StackedWidget::dataProvider () const {
    return dataProviderPtr;
}
