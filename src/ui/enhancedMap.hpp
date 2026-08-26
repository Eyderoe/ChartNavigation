#ifndef CHARTNAVIGATION_ENHANCEDMAP_HPP
#define CHARTNAVIGATION_ENHANCEDMAP_HPP

#include <QGraphicsView>
#include <QGraphicsScene>

class MapView : public QGraphicsView {
        Q_OBJECT
    public:
        explicit MapView(QWidget *parent = nullptr);
    private:
        QGraphicsScene *scene;
};

#endif //CHARTNAVIGATION_ENHANCEDMAP_HPP
