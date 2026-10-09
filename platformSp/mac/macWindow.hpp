#ifndef CHARTNAVIGATION_MAC_WINDOW_HPP
#define CHARTNAVIGATION_MAC_WINDOW_HPP

class QWidget;

// Implemented in Objective-C++ and called only by the macOS platform branch.
void setMacWindowStayOnTop (QWidget *window, bool enabled);

#endif // CHARTNAVIGATION_MAC_WINDOW_HPP
