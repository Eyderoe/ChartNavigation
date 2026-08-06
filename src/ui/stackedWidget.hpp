#ifndef CHARTNAVIGATION_STACKEDWIDGET_HPP
#define CHARTNAVIGATION_STACKEDWIDGET_HPP

#include <QStackedWidget>
#include "services/dataProvider.hpp"

class StackedWidget : public QStackedWidget {
        Q_OBJECT
    public:
        explicit StackedWidget (QWidget *parent = nullptr);
        [[nodiscard]] DataProvider* dataProvider () const;
    private:
        DataProvider *dataProviderPtr{nullptr};
};

#endif //CHARTNAVIGATION_STACKEDWIDGET_HPP
