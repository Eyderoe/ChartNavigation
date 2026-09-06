#ifndef CHARTNAVIGATION_ATTACHEDCHART_HPP
#define CHARTNAVIGATION_ATTACHEDCHART_HPP

#include <QImage>

#include <array>

#include "utils/geographic.hpp"


/**
 * @brief 一张固定附加到航路页的地理配准航图。
 *
 * geographicCorners 按左上、右上、右下、左下顺序保存，与 image 的四角一一对应。
 * image 始终保存 PDF 的原始渲染结果；暗色主题副本由航路页按需生成。
 */
struct AttachedChart {
    QImage image;
    std::array<Point2D, 4> geographicCorners;
};

#endif //CHARTNAVIGATION_ATTACHEDCHART_HPP
