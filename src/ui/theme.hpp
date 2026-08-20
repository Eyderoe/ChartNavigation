#ifndef CHARTNAVIGATION_THEMECOLOR_HPP
#define CHARTNAVIGATION_THEMECOLOR_HPP

#include <QApplication>

void setDarkTheme (QApplication *a = nullptr);
void setLightTheme (QApplication *a = nullptr);

void expandComboBox (const QComboBox *comboBox, int extra = 10);

#endif //CHARTNAVIGATION_THEMECOLOR_HPP