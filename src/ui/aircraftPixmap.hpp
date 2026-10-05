#ifndef CHARTNAVIGATION_AIRCRAFTPIXMAP_HPP
#define CHARTNAVIGATION_AIRCRAFTPIXMAP_HPP

#include <QIcon>
#include <QPixmap>

struct AircraftPixmaps {
    QPixmap own;
    QPixmap traffic;
};

inline AircraftPixmaps loadAircraftPixmaps (const int style) {
    AircraftPixmaps pixmaps{
        QPixmap(QStringLiteral(":/map/resources/planes/plane_small.png")),
        QPixmap(QStringLiteral(":/map/resources/planes/plane_small_2.png"))
    };
    if (style == 1) {
        // 直接以卡通图片的宽度渲染 SVG，避免先栅格化小图再放大。
        const auto loadPlain = [](QPixmap &cartoon, const QString &path) {
            if (cartoon.isNull())
                return;
            const QPixmap plain = QIcon(path).pixmap(QSize(cartoon.width(), cartoon.width()), 1.0);
            if (!plain.isNull())
                cartoon = plain;
        };
        loadPlain(pixmaps.own, QStringLiteral(":/map/resources/planes/plane_self_plain.svg"));
        loadPlain(pixmaps.traffic, QStringLiteral(":/map/resources/planes/plane_other_plain.svg"));
    }
    return pixmaps;
}

#endif // CHARTNAVIGATION_AIRCRAFTPIXMAP_HPP
