#ifndef CHARTNAVIGATION_WEATHERREPORTPARSER_HPP
#define CHARTNAVIGATION_WEATHERREPORTPARSER_HPP

#include <QList>
#include <QString>
#include <QStringList>

namespace WeatherReport {
enum class Type { Unknown, Metar, Speci, Taf };

struct Field {
    QString name;
    QString code;
    QString description;
};

struct Report {
    Type type = Type::Unknown;
    QList<Field> fields;
    QStringList warnings;
    QString error;
};

// Detect and decode one METAR/SPECI/TAF; unsupported groups remain visible.
[[nodiscard]] Report parse(const QString &text);
}

#endif
