#include "games.hpp"
#include "ui_note_widget.h"
#include "twentyFour.hpp"
#include "guessNumber.hpp"
#include "countdown.hpp"
#include "pyramid.hpp"
#include "lightsOut.hpp"
#include "game2048.hpp"
#include "untangle.hpp"
#include "flood.hpp"
#include "map.hpp"
#include "cube.hpp"
#include "pegs.hpp"
#include "net.hpp"
#include "fifteen.hpp"
#include "mines.hpp"
#include "signpost.hpp"
#include "solo.hpp"
#include "palisade.hpp"
#include "tents.hpp"
#include "undead.hpp"
#include "ui/theme.hpp"
#include <QScrollArea>

Games::Games (Ui::note_widget *ui, QObject *parent) : QObject(parent), ui(ui) {
    expandComboBox(ui->miniGamesComboBox);
    connect(ui->miniGamesComboBox, &QComboBox::currentIndexChanged,
            ui->miniGamesStackWidget, &QStackedWidget::setCurrentIndex);
    new TwentyFourGame(ui, this);
    new GuessNumberGame(ui, this);
    new CountdownGame(ui, this);
    new PyramidGame(ui, this);
    new LightsOutGame(ui, this);
    new Game2048(ui, this);
    new UntangleGame(ui, this);
    new FloodGame(ui, this);
    new MapGame(ui, this);
    new CubeGame(ui, this);
    new PegsGame(ui, this);
    new NetGame(ui, this);
    new FifteenGame(ui, this);
    new MinesGame(ui, this);
    new SignpostGame(ui, this);
    new SoloGame(ui, this);
    new PalisadeGame(ui, this);
    new TentsGame(ui, this);
    new UndeadGame(ui, this);
    // Each game has its own scroll content, so hidden games cannot inflate the
    // current game's content height or propagate minimum sizes to the window.
    for (int i = 0; i < ui->miniGamesStackWidget->count(); ++i) {
        auto *page = ui->miniGamesStackWidget->widget(i);
        ui->miniGamesStackWidget->removeWidget(page);
        auto *scroll = new QScrollArea(ui->miniGamesStackWidget);
        scroll->setObjectName(page->objectName() + QStringLiteral("Scroll"));
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setStyleSheet(QStringLiteral("QScrollArea { border: 0; padding: 0; background: transparent; }"));
        scroll->setWidgetResizable(true);
        scroll->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        page->layout()->setSizeConstraint(QLayout::SetMinAndMaxSize);
        scroll->setWidget(page);
        // setWidget enables a palette background after the initial theme polish.
        // Keep pages transparent from their first display, as after theme changes.
        page->setAutoFillBackground(false);
        ui->miniGamesStackWidget->insertWidget(i, scroll);
    }
    for (auto *scroll : ui->miniGamesStackWidget->findChildren<QAbstractScrollArea*>()) {
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
    ui->miniGamesStackWidget->setCurrentIndex(ui->miniGamesComboBox->currentIndex());
}
