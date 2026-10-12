#ifndef CHARTNAVIGATION_GAMES_SQUAREGAMEVIEW_HPP
#define CHARTNAVIGATION_GAMES_SQUAREGAMEVIEW_HPP

#include <QResizeEvent>
#include <QWidget>

// Keep the board square at the available width. Short viewports scroll the
// surrounding page instead of squeezing the board vertically.
class SquareGameView : public QWidget {
    public:
        explicit SquareGameView (QWidget *parent) : QWidget(parent) {
            auto policy = QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            policy.setHeightForWidth(true);
            setSizePolicy(policy);
        }
        int heightForWidth (int width) const override { return width; }
    protected:
        void resizeEvent (QResizeEvent *event) override {
            QWidget::resizeEvent(event);
            if (minimumHeight() != width() || maximumHeight() != width())
                setFixedHeight(width());
        }
};

#endif // CHARTNAVIGATION_GAMES_SQUAREGAMEVIEW_HPP
