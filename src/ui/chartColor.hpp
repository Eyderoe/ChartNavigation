#ifndef CHARTNAVIGATION_CHARTCOLOR_HPP
#define CHARTNAVIGATION_CHARTCOLOR_HPP

#include <QImage>

inline void applyDarkChartTheme (QImage &image, const int style = 0) {
    // 样式1: (255-B, 255-G, 255-R)；样式2: (255-R, 255-G, 255-B)。保留 alpha。
    image.invertPixels(QImage::InvertRgb);
    if (style != 1)
        image.rgbSwap();
}

#endif // CHARTNAVIGATION_CHARTCOLOR_HPP
