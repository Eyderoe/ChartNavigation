#ifndef CHARTNAVIGATION_MAIN_WINDOW_HPP
#define CHARTNAVIGATION_MAIN_WINDOW_HPP

#include <QMainWindow>

#include "enroute_widget.hpp"
#include "main_widget.hpp"
#include "ui_enroute_widget.h"

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
        static void setTheme (Qt::ColorScheme colorScheme);
    protected:
        void closeEvent (QCloseEvent *event) override;
    private:
        Ui::main_window *ui;
        main_widget *pdfBrowser;
        enroute_widget *enroute;
        QStackedWidget *stackedWidget;
        QActionGroup *sourceGroup{nullptr}, *tcasGroup{nullptr}, *altGroup{nullptr};

        void setDataSourceGroup (int val) const;
        void setTcasRangeGroup (int val) const;
        void setAltModeGroup (int val) const;
        void initConnect ();
        void initActionGroup ();
        void menu2toolBar();
    private Q_SLOTS:
        static void on_action_dark_triggered (bool checked);
        void openFile ();
        void openFolder ();
};


#endif //CHARTNAVIGATION_MAIN_WINDOW_HPP
