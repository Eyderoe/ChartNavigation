#ifndef CHARTNAVIGATION_GAMES_SOLO_HPP
#define CHARTNAVIGATION_GAMES_SOLO_HPP
#include "game.hpp"
#include "soloBackend.h"
#include <memory>
#include <vector>
class SoloView;
class SoloGame : public Game {
  public:
    explicit SoloGame(Ui::note_widget *ui, QObject *parent);

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    using State = std::shared_ptr<SoloBackend>;
    using Input = std::unique_ptr<SoloInput, decltype(&solo_input_free)>;
    SoloView *view{};
    SoloConfig config{};
    State state, initial;
    Input input{nullptr, solo_input_free};
    std::vector<State> history, future;
    unsigned generation{};
    bool busy{};
    int buttonsOrder{};
    static State take(SoloBackend *value);
    void selectPreset();
    void applyCustom();
    void fillCustom();
    void reset();
    void retry();
    void undo();
    void redo();
    void adopt(State next);
    void send(int button, int x = -100, int y = -100);
    void refresh();
    void buildButtons();
};
#endif
