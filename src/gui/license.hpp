#ifndef CHARTNAVIGATION_LICENSE_HPP
#define CHARTNAVIGATION_LICENSE_HPP

#include <QDialog>


QT_BEGIN_NAMESPACE
namespace Ui
{
class license;
}
QT_END_NAMESPACE

class license final : public QDialog {
        Q_OBJECT
    public:
        explicit license (QWidget *parent = nullptr);
        ~license () override;
    private:
        Ui::license *ui;
};


#endif //CHARTNAVIGATION_LICENSE_HPP
