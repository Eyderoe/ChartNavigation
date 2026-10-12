#include "map.hpp"
#include "ui_note_widget.h"
#include "colorPalette.hpp"
#include "squareGameView.hpp"
#include "ui/theme.hpp"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRandomGenerator>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <numeric>
#include <cstdint>

using GameColors::floodPalette;

namespace {
// Map rules reference: Simon Tatham's Portable Puzzle Collection (MIT).
// Generator and difficulty rules are adapted from map.c in the upstream
// 20260923.616da16 source. See SIMON-TATHAM-LICENSE.txt.
// https://www.chiark.greenend.org.uk/~sgtatham/puzzles/puzzles.tar.gz
enum class MapDifficulty : int { Easy, Normal, Hard, Unreasonable };

struct MapPreset {
    int width, height, regions;
    MapDifficulty difficulty;
};

constexpr std::array<MapPreset, 6> mapPresets{{
    {20, 15, 30, MapDifficulty::Easy},
    {20, 15, 30, MapDifficulty::Normal},
    {20, 15, 30, MapDifficulty::Hard},
    {20, 15, 30, MapDifficulty::Unreasonable},
    {30, 25, 75, MapDifficulty::Normal},
    {30, 25, 75, MapDifficulty::Hard},
}};

struct MapSolverState {
    std::vector<std::uint8_t> possible;
    std::vector<int> colors;
};

bool placeMapColor (const std::vector<std::vector<int>> &adjacent,
                    MapSolverState &state, int region, int color) {
    const auto bit = static_cast<std::uint8_t>(1u << color);
    if (!(state.possible[region] & bit))
        return false;
    state.possible[region] = bit;
    state.colors[region] = color;
    for (int neighbor : adjacent[region])
        state.possible[neighbor] &= static_cast<std::uint8_t>(~bit);
    return true;
}

int solveMapLogic (const std::vector<std::vector<int>> &adjacent,
                   MapSolverState state, int difficulty) {
    const int count = static_cast<int>(state.colors.size());
    while (true) {
        bool changed = false;
        if (difficulty < static_cast<int>(MapDifficulty::Easy))
            break;

        // Easy: repeatedly place a region with only one possible colour.
        for (int region = 0; region < count; ++region) {
            if (state.colors[region] >= 0)
                continue;
            const auto choices = state.possible[region];
            if (!choices)
                return 0;
            if ((choices & (choices - 1)) == 0) {
                const int color = std::countr_zero(static_cast<unsigned>(choices));
                if (!placeMapColor(adjacent, state, region, color))
                    return 0;
                changed = true;
            }
        }
        if (changed)
            continue;

        if (difficulty < static_cast<int>(MapDifficulty::Normal))
            break;

        // Normal: an adjacent pair with the same two colours reserves both
        // colours, so a shared neighbour cannot use either one.
        for (int first = 0; first < count; ++first) {
            if (state.colors[first] >= 0)
                continue;
            for (int second : adjacent[first]) {
                if (second <= first || state.colors[second] >= 0
                    || state.possible[first] != state.possible[second]
                    || std::popcount(static_cast<unsigned>(state.possible[first])) != 2)
                    continue;
                const auto reserved = state.possible[first];
                for (int neighbor : adjacent[first])
                    if (std::binary_search(adjacent[neighbor].begin(), adjacent[neighbor].end(), second)
                        && (state.possible[neighbor] & reserved)) {
                        state.possible[neighbor] &= static_cast<std::uint8_t>(~reserved);
                        changed = true;
                    }
            }
        }
        if (changed)
            continue;

        if (difficulty < static_cast<int>(MapDifficulty::Hard))
            break;

        // Hard: use the same two-colour forcing-chain search as map.c.
        for (int start = 0; start < count; ++start) {
            if (state.colors[start] >= 0
                || std::popcount(static_cast<unsigned>(state.possible[start])) != 2)
                continue;
            for (int color = 0; color < 4; ++color) {
                const auto original = static_cast<std::uint8_t>(1u << color);
                if (!(state.possible[start] & original))
                    continue;
                std::vector<int> forcedColor(count, -1), queue;
                queue.push_back(start);
                forcedColor[start] = state.possible[start] & static_cast<std::uint8_t>(~original);
                for (size_t head = 0; head < queue.size(); ++head) {
                    const int current = queue[head];
                    const auto currentColor = static_cast<std::uint8_t>(forcedColor[current]);
                    for (int neighbor : adjacent[current]) {
                        if (forcedColor[neighbor] < 0 && state.colors[neighbor] < 0
                            && std::popcount(static_cast<unsigned>(state.possible[neighbor])) == 2
                            && (state.possible[neighbor] & currentColor)) {
                            queue.push_back(neighbor);
                            forcedColor[neighbor] = state.possible[neighbor]
                                & static_cast<std::uint8_t>(~currentColor);
                        }
                        if (currentColor == original
                            && std::binary_search(adjacent[neighbor].begin(), adjacent[neighbor].end(), start)
                            && (state.possible[neighbor] & currentColor)) {
                            state.possible[neighbor] &= static_cast<std::uint8_t>(~original);
                            changed = true;
                        }
                    }
                }
            }
        }
        if (!changed)
            break;
    }

    const auto undecided = std::ranges::find(state.colors, -1);
    if (undecided == state.colors.end())
        return 1;
    if (difficulty < static_cast<int>(MapDifficulty::Unreasonable))
        return 2;

    // Unreasonable: recurse on the most constrained region, distinguishing
    // a unique solution from ambiguity exactly as the upstream solver does.
    int selected = -1, fewest = 5;
    for (int region = 0; region < count; ++region)
        if (state.colors[region] < 0) {
            const int choices = std::popcount(static_cast<unsigned>(state.possible[region]));
            if (choices == 0)
                return 0;
            if (choices < fewest) {
                selected = region;
                fewest = choices;
            }
        }
    bool foundOne = false;
    for (int color = 0; color < 4; ++color) {
        if (!(state.possible[selected] & (1u << color)))
            continue;
        auto branch = state;
        if (!placeMapColor(adjacent, branch, selected, color))
            continue;
        const int result = solveMapLogic(adjacent, std::move(branch), difficulty);
        if (result == 2 || (result == 1 && foundOne))
            return 2;
        if (result == 1)
            foundOne = true;
    }
    return foundOne ? 1 : 0;
}

int solveMap (const std::vector<std::vector<int>> &adjacent,
              const std::vector<int> &clues, int difficulty) {
    MapSolverState state{std::vector<std::uint8_t>(clues.size(), 0x0f), clues};
    for (int region = 0; region < static_cast<int>(clues.size()); ++region)
        if (clues[region] >= 0 && !placeMapColor(adjacent, state, region, clues[region]))
            return 0;
    return solveMapLogic(adjacent, std::move(state), difficulty);
}

bool fourColourRecurse (const std::vector<std::vector<int>> &adjacent,
                        std::vector<int> &colors,
                        std::vector<std::array<int, 5>> &scratch) {
    int fewest = 5, tied = 0;
    for (int region = 0; region < static_cast<int>(colors.size()); ++region)
        if (colors[region] < 0 && scratch[region][4] <= fewest) {
            if (fewest > scratch[region][4]) {
                fewest = scratch[region][4];
                tied = 0;
            }
            ++tied;
        }
    if (!tied)
        return true;

    int choice = QRandomGenerator::global()->bounded(tied), selected = -1;
    for (int region = 0; region < static_cast<int>(colors.size()); ++region)
        if (colors[region] < 0 && scratch[region][4] == fewest && choice-- == 0) {
            selected = region;
            break;
        }
    std::array<int, 4> colorOrder{};
    int colorCount = 0;
    for (int color = 0; color < 4; ++color)
        if (!scratch[selected][color])
            colorOrder[colorCount++] = color;
    for (int i = colorCount - 1; i > 0; --i)
        std::swap(colorOrder[i], colorOrder[QRandomGenerator::global()->bounded(i + 1)]);

    while (colorCount > 0) {
        const int color = colorOrder[--colorCount];
        colors[selected] = color;
        for (int neighbor : adjacent[selected]) {
            if (!scratch[neighbor][color])
                --scratch[neighbor][4];
            ++scratch[neighbor][color];
        }
        if (fourColourRecurse(adjacent, colors, scratch))
            return true;
        for (int neighbor : adjacent[selected]) {
            --scratch[neighbor][color];
            if (!scratch[neighbor][color])
                ++scratch[neighbor][4];
        }
        colors[selected] = -1;
    }
    return false;
}

std::vector<int> fourColour (const std::vector<std::vector<int>> &adjacent) {
    std::vector<int> colors(adjacent.size(), -1);
    std::vector<std::array<int, 5>> scratch(adjacent.size());
    for (auto &region : scratch) {
        region.fill(0);
        region[4] = 4;
    }
    Q_ASSERT(fourColourRecurse(adjacent, colors, scratch));
    return colors;
}

template<typename T>
void shuffleMap (std::vector<T> &values) {
    for (int i = static_cast<int>(values.size()) - 1; i > 0; --i)
        std::swap(values[i], values[QRandomGenerator::global()->bounded(i + 1)]);
}

struct MapPuzzle {
    std::vector<int> cells;
    std::vector<MapGeometry::Tile> tiles;
    std::vector<std::vector<int>> adjacent;
    std::vector<int> clues;
};

MapPuzzle generateMap (const MapPreset &preset) {
    int minimumDifficulty = static_cast<int>(preset.difficulty);
    int tries = 50;
    while (true) {
        auto &random = *QRandomGenerator::global();
        auto cells = MapGeometry::generate(preset.width, preset.height, preset.regions, random);
        auto adjacent = MapGeometry::adjacency(cells, preset.width, preset.height, preset.regions);
        const auto solution = fourColour(adjacent);
        auto clues = solution;
        std::array<int, 4> colorCounts{};
        for (int color : solution)
            ++colorCounts[color];
        std::vector<int> order(preset.regions);
        std::iota(order.begin(), order.end(), 0);
        shuffleMap(order);

        // Match map.c: remove one clue at a time only when its own
        // difficulty-limited solver still finds exactly one solution. Keep at
        // least one example of every colour, as required by drag-to-colour.
        for (int region : order) {
            const int color = solution[region];
            if (colorCounts[color] == 1)
                continue;
            clues[region] = -1;
            if (solveMap(adjacent, clues, static_cast<int>(preset.difficulty)) == 1)
                --colorCounts[color];
            else
                clues[region] = color;
        }

        // Require the requested grade: a lower-level solver must not already
        // finish the puzzle. The upstream fallback is retained for parameter
        // sets where the requested grade is structurally unattainable.
        if (solveMap(adjacent, clues, minimumDifficulty - 1) == 1) {
            if (minimumDifficulty > 0
                && (preset.regions < 9 || preset.regions > 2 * preset.width * preset.height / 3)
                && tries-- <= 0)
                minimumDifficulty = static_cast<int>(MapDifficulty::Easy);
            continue;
        }

        MapPuzzle puzzle;
        puzzle.cells = std::move(cells);
        puzzle.tiles = MapGeometry::smooth(puzzle.cells, preset.width, preset.height, random);
        puzzle.adjacent = std::move(adjacent);
        puzzle.clues = std::move(clues);
        return puzzle;
    }
}
}

class MapGameView : public SquareGameView {
    public:
        explicit MapGameView (QWidget *parent) : SquareGameView(parent) {
            setObjectName(QStringLiteral("mapView"));
            setFocusPolicy(Qt::StrongFocus);
            setAccessibleName(tr("四色定理地图"));
        }
        QSize sizeHint () const override { return {400, 320}; }
        QSize minimumSizeHint () const override { return {160, 120}; }
        QRectF boardRect () const {
            if (gridWidth <= 0 || gridHeight <= 0)
                return {};
            const auto available = QRectF(0, 0, width(), height()).adjusted(8, 8, -8, -8);
            const qreal scale = std::min(available.width() / gridWidth,
                                         available.height() / gridHeight);
            const QSizeF size(gridWidth * scale, gridHeight * scale);
            return QRectF(available.center().x() - size.width() / 2,
                          available.center().y() - size.height() / 2,
                          size.width(), size.height());
        }
        int regionAt (QPointF point) const {
            const auto rect = boardRect();
            if (gridWidth <= 0 || gridHeight <= 0 || rect.width() <= 0 || rect.height() <= 0
                || !rect.contains(point))
                return -1;
            return MapGeometry::regionAt(tiles, gridWidth, gridHeight,
                {(point.x() - rect.left()) * gridWidth / rect.width(),
                 (point.y() - rect.top()) * gridHeight / rect.height()});
        }
        void toggleNumbers () {
            showNumbers = !showNumbers;
            update();
        }
        void setMap (const std::vector<MapGeometry::Tile> &values, int width, int height, int regions) {
            tiles = values;
            gridWidth = width;
            gridHeight = height;
            paths.assign(regions, {});
            centers.assign(regions, {});
            clearances.assign(regions, 0);
            std::vector<int> counts(regions);
            std::vector<std::vector<QPointF>> candidates(regions);
            for (int cell = 0; cell < gridWidth * gridHeight; ++cell) {
                const int x = cell % gridWidth, y = cell / gridWidth;
                const auto &tile = tiles[cell];
                const bool whole = std::ranges::all_of(tile, [&](int r) { return r == tile[0]; });
                if (whole) {
                    paths[tile[0]].addRect(x, y, 1, 1);
                    candidates[tile[0]].push_back({x + 0.5, y + 0.5});
                }
                for (int edge = 0; edge < 4; ++edge) {
                    const int region = tile[edge];
                    const auto vertices = MapGeometry::triangle(x, y, static_cast<MapGeometry::Edge>(edge));
                    const QPointF center = (vertices[0] + vertices[1] + vertices[2]) / 3;
                    centers[region] += center;
                    ++counts[region];
                    candidates[region].push_back(center);
                    if (!whole) {
                        paths[region].addPolygon(QPolygonF{vertices[0], vertices[1], vertices[2]});
                        paths[region].closeSubpath();
                    }
                }
            }
            // A label must remain inside the drawn region, including a narrow
            // triangle. Prefer the candidate furthest from the actual outline,
            // then the one nearest the area centroid. Grid-cell BFS no longer
            // describes the geometry after upstream diagonal smoothing.
            for (int region = 0; region < regions; ++region) {
                paths[region] = paths[region].simplified();
                const QPointF centroid = centers[region] / counts[region];
                const auto outlines = paths[region].toSubpathPolygons();
                qreal best = -1, bestCentroid = 0;
                for (const auto &point : candidates[region]) {
                    qreal clearance = gridWidth * gridWidth + gridHeight * gridHeight;
                    for (const auto &polygon : outlines)
                        for (qsizetype i = 1; i < polygon.size(); ++i) {
                            const auto a = polygon[i - 1], delta = polygon[i] - a;
                            const qreal length = QPointF::dotProduct(delta, delta);
                            const qreal t = length > 0 ? std::clamp(QPointF::dotProduct(point - a, delta) / length, 0.0, 1.0) : 0;
                            const auto offset = point - a - delta * t;
                            clearance = std::min(clearance, QPointF::dotProduct(offset, offset));
                        }
                    const auto offset = point - centroid;
                    const qreal centroidDistance = QPointF::dotProduct(offset, offset);
                    if (clearance > best + 1e-9 || (std::abs(clearance - best) < 1e-9 && centroidDistance < bestCentroid)) {
                        best = clearance;
                        bestCentroid = centroidDistance;
                        centers[region] = point;
                    }
                }
                clearances[region] = std::sqrt(std::max(0.0, best));
            }
        }
        void setColors (const std::vector<int> &values, const std::vector<int> &pencils,
                        const std::vector<int> &clues, const std::vector<bool> &conflicts, int region) {
            colors = values;
            notes = pencils;
            fixed = clues;
            crossing = conflicts;
            selected = region;
            update();
        }
    protected:
        void paintEvent (QPaintEvent *) override {
            const auto rect = boardRect();
            if (gridWidth <= 0 || gridHeight <= 0 || rect.width() <= 0)
                return;
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.translate(rect.topLeft());
            const qreal scale = rect.width() / gridWidth;
            painter.scale(scale, scale);
            for (size_t region = 0; region < paths.size(); ++region)
                painter.fillPath(paths[region], colors[region] < 0 ? palette().color(QPalette::Base)
                    : QColor(floodPalette[colors[region]]));
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(palette().color(QPalette::Text), 1.2 / scale));
            for (const auto &path : paths)
                painter.drawPath(path);
            painter.setPen(QPen(QColor("#ff3054"), 3 / scale, Qt::DashLine));
            for (size_t region = 0; region < paths.size(); ++region)
                if (crossing[region])
                    painter.drawPath(paths[region]);
            if (selected >= 0) {
                painter.setPen(QPen(Qt::white, 4 / scale));
                painter.drawPath(paths[selected]);
                painter.setPen(QPen(Qt::black, 1.5 / scale, Qt::DashLine));
                painter.drawPath(paths[selected]);
            }
            // Annotation sizes follow the available interior; tiny regions
            // must not spill their lock or number into a neighbour.
            painter.resetTransform();
            const qreal preferredSize = std::clamp(rect.width() * 0.025, 8.0, 14.0);
            for (int region = 0; region < static_cast<int>(paths.size()); ++region) {
                const auto center = rect.topLeft() + centers[region] * scale;
                const qreal room = clearances[region] * scale;
                const qreal labelSize = std::max(1.0, std::min(preferredSize, room));
                const auto ink = colors[region] < 0 ? palette().color(QPalette::Text)
                    : colors[region] == 2 ? QColor("#30291b") : QColor(Qt::white);
                auto labelFont = font();
                labelFont.setPixelSize(std::max(1, static_cast<int>(labelSize)));
                labelFont.setBold(true);
                painter.setFont(labelFont);
                painter.setPen(ink);
                if (showNumbers && labelSize >= 5)
                    painter.drawText(QRectF(center.x() - labelSize, center.y() - labelSize / 2, labelSize * 2, labelSize),
                                     Qt::AlignCenter, QString::number(region + 1));
                if (fixed[region] >= 0) {
                    const qreal unit = std::min(1.0, room / (showNumbers ? 15.0 : 7.0));
                    const auto lockCenter = center + QPointF(0, showNumbers ? -labelSize * 0.8 : 0);
                    painter.setBrush(Qt::NoBrush);
                    painter.setPen(QPen(ink, std::max(0.5, 1.2 * unit)));
                    painter.drawArc(QRectF(lockCenter.x() - 3 * unit, lockCenter.y() - 5 * unit, 6 * unit, 7 * unit), 0, 180 * 16);
                    painter.setBrush(ink);
                    painter.drawRoundedRect(QRectF(lockCenter.x() - 4 * unit, lockCenter.y() - 2 * unit, 8 * unit, 6 * unit), unit, unit);
                } else if (colors[region] < 0 && notes[region] != 0) {
                    const qreal unit = std::min(1.0, room / 13.0);
                    for (int color = 0; color < 4; ++color)
                        if (notes[region] & (1 << color)) {
                            painter.setBrush(QColor(floodPalette[color]));
                            painter.setPen(QPen(palette().color(QPalette::Text), 0.5));
                            painter.drawEllipse(center + QPointF((color - 1.5) * 6 * unit, showNumbers ? labelSize * 0.7 : 0), 2.5 * unit, 2.5 * unit);
                        }
                }
            }
        }
    private:
        int gridWidth{}, gridHeight{};
        std::vector<MapGeometry::Tile> tiles;
        std::vector<QPainterPath> paths;
        std::vector<QPointF> centers;
        std::vector<qreal> clearances;
        bool showNumbers{};
        std::vector<int> colors, notes, fixed;
        std::vector<bool> crossing;
        int selected{-1};
};

MapGame::MapGame (Ui::note_widget *ui, QObject *parent) : Game(ui, parent) {
    initMap();
}

void MapGame::initMap () {
    expandComboBox(ui->mapPreset);
    mapView = new MapGameView(ui->mapBoard);
    ui->mapBoardLayout->addWidget(mapView);
    mapView->installEventFilter(this);
    for (int color = 0; color < 4; ++color) {
        auto *button = new QToolButton(ui->pageMap);
        mapColorButtons[color] = button;
        button->setObjectName(QStringLiteral("mapColor%1").arg(color + 1));
        button->setText(QString::number(color + 1));
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setCheckable(true);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setAccessibleName(tr("颜色 %1").arg(color + 1));
        button->setToolTip(tr("选择颜色 %1（数字键 %1）").arg(color + 1));
        QPixmap swatch(24, 24);
        swatch.fill(QColor(floodPalette[color]));
        button->setIcon(QIcon(swatch));
        ui->mapColorsLayout->addWidget(button);
        connect(button, &QToolButton::clicked, this, [this, color] {
            mapSelectedColor = color;
            updateMap();
            mapView->setFocus(Qt::OtherFocusReason);
        });
    }
    ui->mapEraseButton->setCheckable(true);
    connect(ui->mapEraseButton, &QToolButton::clicked, this, [this] {
        mapSelectedColor = -1;
        updateMap();
        mapView->setFocus(Qt::OtherFocusReason);
    });
    connect(ui->mapPreset, &QComboBox::currentIndexChanged, this, &MapGame::resetMap);
    connect(ui->mapNewButton, &QToolButton::clicked, this, &MapGame::resetMap);
    connect(ui->mapRetryButton, &QToolButton::clicked, this, [this] {
        mapColors = mapInitialColors;
        std::ranges::fill(mapNotes, 0);
        mapHistory.clear();
        mapDragging = false;
        updateMap();
    });
    connect(ui->mapUndoButton, &QToolButton::clicked, this, [this] {
        if (mapHistory.empty())
            return;
        auto previous = std::move(mapHistory.back());
        mapHistory.pop_back();
        mapColors = std::move(previous.colors);
        mapNotes = std::move(previous.notes);
        updateMap();
    });
    resetMap();
}

void MapGame::resetMap () {
    const auto preset = mapPresets[std::clamp(ui->mapPreset->currentIndex(), 0,
                                              static_cast<int>(mapPresets.size()) - 1)];
    mapWidth = preset.width;
    mapHeight = preset.height;
    auto puzzle = generateMap(preset);
    mapTiles = std::move(puzzle.tiles);
    mapAdjacent = std::move(puzzle.adjacent);
    mapInitialColors = std::move(puzzle.clues);
    mapColors = mapInitialColors;
    mapNotes.assign(mapColors.size(), 0);
    mapHistory.clear();
    mapSelectedColor = 0;
    mapSelectedRegion = -1;
    mapDragging = false;
    mapView->setMap(mapTiles, mapWidth, mapHeight, preset.regions);
    updateMap();
}

void MapGame::colorMapRegion (int region, int color, bool pencil) {
    if (region < 0 || region >= static_cast<int>(mapColors.size()) || color < -1 || color > 3)
        return;
    mapSelectedRegion = region;
    if (mapInitialColors[region] >= 0) {
        updateMap();
        ui->mapFeedback->setText(tr("区域 %1 是预填线索，颜色不能修改。").arg(region + 1));
        return;
    }
    if (pencil && (color < 0 || mapColors[region] >= 0)) {
        updateMap();
        return;
    }
    if (!pencil && mapColors[region] == color && mapNotes[region] == 0) {
        updateMap();
        return;
    }
    mapHistory.push_back({mapColors, mapNotes});
    if (pencil)
        mapNotes[region] ^= 1 << color;
    else {
        mapColors[region] = color;
        mapNotes[region] = 0;
    }
    updateMap();
}

void MapGame::updateMap () {
    std::vector<bool> conflicts(mapColors.size());
    int pairs = 0, filled = 0, clues = 0;
    for (int region = 0; region < static_cast<int>(mapColors.size()); ++region) {
        filled += mapColors[region] >= 0;
        clues += mapInitialColors[region] >= 0;
        for (int neighbor : mapAdjacent[region])
            if (neighbor > region && mapColors[region] >= 0 && mapColors[region] == mapColors[neighbor]) {
                ++pairs;
                conflicts[region] = conflicts[neighbor] = true;
            }
    }
    const bool solved = filled == static_cast<int>(mapColors.size()) && pairs == 0;
    mapView->setColors(mapColors, mapNotes, mapInitialColors, conflicts, mapSelectedRegion);
    ui->mapStatus->setText(tr("已着色：%1 / %2　线索：%3　冲突：%4")
        .arg(filled).arg(mapColors.size()).arg(clues).arg(pairs));
    ui->mapUndoButton->setEnabled(!mapHistory.empty());
    for (int color = 0; color < 4; ++color)
        mapColorButtons[color]->setChecked(mapSelectedColor == color);
    ui->mapEraseButton->setChecked(mapSelectedColor == -1);
    ui->mapFeedback->setText(solved ? tr("地图完成！所有相邻区域都不同色。点击新一局继续。")
        : pairs > 0 ? tr("红色虚线标出了相邻同色的冲突区域。")
        : mapSelectedRegion >= 0 ? tr("当前区域：%1%2").arg(mapSelectedRegion + 1)
            .arg(mapInitialColors[mapSelectedRegion] >= 0 ? tr("（预填线索）") : QString{}) : QString{});
    mapView->setAccessibleDescription(ui->mapStatus->text() + ui->mapFeedback->text());
}

bool MapGame::handleMapEvent (QEvent *event) {
    if (event->type() == QEvent::Hide || event->type() == QEvent::UngrabMouse) {
        mapDragging = false;
        return false;
    }
    if (event->type() == QEvent::MouseButtonPress) {
        const auto *mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() != Qt::LeftButton && mouse->button() != Qt::RightButton)
            return false;
        const int region = mapView->regionAt(mouse->position());
        mapDragging = true;
        mapDragStart = mouse->position();
        mapDragColor = region < 0 ? -1 : mapColors[region] >= 0 ? mapColors[region] : mapSelectedColor;
        mapSelectedRegion = region;
        mapView->setFocus(Qt::MouseFocusReason);
        updateMap();
        return true;
    }
    if (event->type() == QEvent::MouseButtonRelease && mapDragging) {
        const auto *mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() != Qt::LeftButton && mouse->button() != Qt::RightButton)
            return false;
        mapDragging = false;
        const int region = mapView->regionAt(mouse->position());
        const bool dragged = QLineF(mapDragStart, mouse->position()).length() > 4;
        colorMapRegion(region, dragged ? mapDragColor : mapSelectedColor, mouse->button() == Qt::RightButton);
        return true;
    }
    if (event->type() != QEvent::KeyPress)
        return false;
    const auto *key = static_cast<QKeyEvent*>(event);
    if (key->key() == Qt::Key_Tab || key->key() == Qt::Key_Backtab) {
        if ((key->modifiers() & ~(Qt::ShiftModifier | Qt::KeypadModifier)) != Qt::NoModifier)
            return false;
        const int count = static_cast<int>(mapColors.size());
        const bool backwards = key->key() == Qt::Key_Backtab || (key->modifiers() & Qt::ShiftModifier);
        mapSelectedRegion = mapSelectedRegion < 0 ? (backwards ? count - 1 : 0)
            : (mapSelectedRegion + (backwards ? count - 1 : 1)) % count;
        updateMap();
        return true;
    }
    if ((key->modifiers() & ~Qt::KeypadModifier) != Qt::NoModifier)
        return false;
    if (key->key() == Qt::Key_L) {
        mapView->toggleNumbers();
        return true;
    }
    if (key->key() >= Qt::Key_1 && key->key() <= Qt::Key_4) {
        mapSelectedColor = key->key() - Qt::Key_1;
        colorMapRegion(mapSelectedRegion, mapSelectedColor);
        updateMap();
        return true;
    }
    if (key->key() == Qt::Key_Delete || key->key() == Qt::Key_Backspace) {
        colorMapRegion(mapSelectedRegion, -1);
        return true;
    }
    if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter || key->key() == Qt::Key_Space) {
        colorMapRegion(mapSelectedRegion, mapSelectedColor);
        return true;
    }
    return false;
}

bool MapGame::eventFilter (QObject *watched, QEvent *event) {
    if (watched == mapView && handleMapEvent(event))
        return true;
    return QObject::eventFilter(watched, event);
}
