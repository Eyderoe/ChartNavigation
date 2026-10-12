#ifndef CHARTNAVIGATION_GAMES_UNDEAD_HPP
#define CHARTNAVIGATION_GAMES_UNDEAD_HPP
#include "game.hpp"
#include "undeadBackend.h"
#include <memory>
#include <vector>
class UndeadView;
class UndeadGame : public Game {
  public:
    explicit UndeadGame(Ui::note_widget *ui, QObject *parent);

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    using State = std::shared_ptr<UndeadBackend>;
    using Input = std::unique_ptr<UndeadInput, decltype(&undead_input_free)>;
    UndeadView *view{};
    UndeadConfig config{};
    State state, initial;
    Input input{nullptr, undead_input_free};
    std::vector<State> history, future;
    unsigned generation{};
    bool busy{};
    static State take(UndeadBackend *value);
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
    void applyPreferences();
};
#endif
