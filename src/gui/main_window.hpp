#ifndef CHARTNAVIGATION_MAIN_WINDOW_HPP
#define CHARTNAVIGATION_MAIN_WINDOW_HPP

#include <QMainWindow>

QT_BEGIN_NAMESPACE

namespace Ui
{
class main_window;
}

QT_END_NAMESPACE

class main_window : public QMainWindow {
        Q_OBJECT
    public:
        explicit main_window (QWidget *parent = nullptr);
        ~main_window () override;
    private:
        Ui::main_window *ui;
};


#endif //CHARTNAVIGATION_MAIN_WINDOW_HPP
