#ifndef CHARTNAVIGATION_GAMES_PALISADE_HPP
#define CHARTNAVIGATION_GAMES_PALISADE_HPP
#include "game.hpp"
#include "palisadeBackend.h"
#include <memory>
#include <vector>
class PalisadeView;
class PalisadeGame : public Game {
  public:
    explicit PalisadeGame(Ui::note_widget *ui, QObject *parent);

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    using State = std::shared_ptr<PalisadeBackend>;
    using Input = std::unique_ptr<PalisadeInput, decltype(&palisade_input_free)>;
    PalisadeView *view{};
    PalisadeConfig config{};
    State state, initial;
    Input input{nullptr, palisade_input_free};
    std::vector<State> history, future;
    unsigned generation{};
    bool busy{};
    static State take(PalisadeBackend *value);
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
