#ifndef CHARTNAVIGATION_GAMES_TENTS_HPP
#define CHARTNAVIGATION_GAMES_TENTS_HPP
#include "game.hpp"
#include "tentsBackend.h"
#include <memory>
#include <vector>
class TentsView;
class TentsGame : public Game {
  public:
    explicit TentsGame(Ui::note_widget *ui, QObject *parent);

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    using State = std::shared_ptr<TentsBackend>;
    using Input = std::unique_ptr<TentsInput, decltype(&tents_input_free)>;
    TentsView *view{};
    TentsConfig config{};
    State state, initial;
    Input input{nullptr, tents_input_free};
    std::vector<State> history, future;
    unsigned generation{};
    bool busy{};
    int dragButton{};
    static State take(TentsBackend *value);
    void selectPreset();
    void applyCustom();
    void fillCustom();
    void reset();
    void retry();
    void undo();
    void redo();
    void adopt(State next);
    void send(int button, int x = -100, int y = -100, bool control = false, bool shift = false);
    void refresh();
};
#endif
