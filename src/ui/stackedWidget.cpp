#include "stackedWidget.hpp"
#include <json.hpp>


StackedWidget::StackedWidget (QWidget *parent) : QStackedWidget(parent) {
    readTurbuCate();
}

void StackedWidget::readTurbuCate () {
    QFile mappingFile(":/doc/resources/documents/wtc.json");
    mappingFile.open(QIODevice::ReadOnly);
    QTextStream stream(&mappingFile);
    const auto database = nlohmann::json::parse(stream.readAll().toUtf8().constData());
    for (const auto &item : database.items()) {
        auto value = item.value().get<std::string>();
        turbuCate[item.key()] = value[0];
    }
}
