#ifndef CHARTNAVIGATION_ABOUT_DIALOG_HPP
#define CHARTNAVIGATION_ABOUT_DIALOG_HPP

#include <QDialog>


QT_BEGIN_NAMESPACE

namespace Ui
{
class about_dialog;
}

QT_END_NAMESPACE

class about_dialog : public QDialog {
    Q_OBJECT
    public:
        explicit about_dialog (QWidget *parent = nullptr);
        ~about_dialog () override;
    private:
        Ui::about_dialog *ui;
};


#endif //CHARTNAVIGATION_ABOUT_DIALOG_HPP