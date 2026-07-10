#include "stackedWidget.hpp"
#include <json.hpp>


StackedWidget::StackedWidget (QWidget *parent) : QStackedWidget(parent) {
    readTurbuCate();
}

void StackedWidget::readTurbuCate () {
    QFile mappingFile(":/doc/resources/documents/wtc.json");
    mappingFile.open(QIODevice::ReadOnly);
    QTextStream stream(&mappingFile);
    auto database=nlohmann::json{};
    try {
        database = nlohmann::json::parse(stream.readAll().toUtf8().constData());
    } catch (nlohmann::json::parse_error& ex) {
        qDebug() << mappingFile.fileName() << " 解析失败";
        return;
    }
    for (const auto &item : database.items()) {
        auto value = item.value().get<std::string>();
        turbuCate[item.key()] = value[0];
    }
}
