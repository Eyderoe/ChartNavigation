#include "weatherReportParser.hpp"

#include <QHash>
#include <QRegularExpression>

// Field grouping reference: https://github.com/sameer/weather-reports
// Code meanings/units: https://aviationweather.gov/help/data/
// International TAF groups: https://www.hko.gov.hk/sc/aviat/decode_taf.htm
// This is an independent Qt decoder; RMK and unsupported regional groups are preserved.
namespace {
QString weather(QString code) {
    static const QHash<QString, QString> descriptors{
        {"MI", "浅"}, {"PR", "部分"}, {"BC", "碎片状"}, {"DR", "低吹"},
        {"BL", "高吹"}, {"SH", "阵性"}, {"TS", "雷暴"}, {"FZ", "冻结"}
    };
    static const QHash<QString, QString> phenomena{
        {"DZ", "毛毛雨"}, {"RA", "雨"}, {"SN", "雪"}, {"SG", "米雪"},
        {"IC", "冰晶"}, {"PL", "冰粒"}, {"GR", "冰雹"}, {"GS", "小冰雹／霰"},
        {"UP", "未知降水"}, {"BR", "轻雾"}, {"FG", "雾"}, {"FU", "烟"},
        {"VA", "火山灰"}, {"DU", "浮尘"}, {"SA", "沙"}, {"HZ", "霾"},
        {"PY", "水沫"}, {"PO", "尘／沙旋风"}, {"SQ", "飑"},
        {"FC", "漏斗云／龙卷风／水龙卷"}, {"SS", "沙暴"}, {"DS", "尘暴"}
    };
    QString prefix;
    if (code.startsWith('-') || code.startsWith('+')) {
        prefix = code.startsWith('-') ? "轻度 " : "强 ";
        code.remove(0, 1);
    }
    if (code.startsWith("VC")) {
        prefix += "机场附近 ";
        code.remove(0, 2);
    }
    QString descriptor;
    if (descriptors.contains(code.left(2))) {
        descriptor = descriptors.value(code.left(2));
        code.remove(0, 2);
    }
    QStringList parts;
    while (!code.isEmpty()) {
        if (code.size() < 2 || !phenomena.contains(code.left(2)))
            return {};
        parts << phenomena.value(code.left(2));
        code.remove(0, 2);
    }
    if (parts.isEmpty() && descriptor != "雷暴")
        return {};
    return prefix + descriptor + parts.join("、");
}

QString temperature(const QString &code) {
    if (code.isEmpty() || code == "//")
        return "未报告";
    return QString::number(code.startsWith('M') ? -code.mid(1).toInt() : code.toInt()) + " °C";
}

QString boundValue(QString code, const QString &unit) {
    QString bound;
    if (code.startsWith('M') || code.startsWith('P')) {
        bound = code.startsWith('M') ? "小于 " : "大于 ";
        code.remove(0, 1);
    }
    return bound + QString::number(code.toInt()) + " " + unit;
}

bool validTime(const QString &time) {
    return time.left(2).toInt() < 24 && time.mid(2, 2).toInt() < 60;
}

QString timeText(const QString &time) {
    return time.left(2) + ":" + time.mid(2, 2) + " UTC";
}

bool validDay(const QString &day) {
    return day.toInt() >= 1 && day.toInt() <= 31;
}

bool validDayHour(const QString &value, bool allowMidnightEnd = false) {
    return validDay(value.left(2)) && value.mid(2).toInt() <= (allowMidnightEnd ? 24 : 23);
}

QString dayHourText(const QString &value) {
    return QString("%1 日 %2:00 UTC").arg(value.left(2), value.mid(2));
}

bool validPeriod(const QString &code) {
    static const QRegularExpression pattern("^\\d{4}/\\d{4}$");
    if (!pattern.match(code).hasMatch() || !validDayHour(code.left(4)) || !validDayHour(code.mid(5), true))
        return false;
    // A smaller end day denotes a month boundary; the report does not supply a month/year.
    const int startDay = code.left(2).toInt();
    const int endDay = code.mid(5, 2).toInt();
    if (startDay == endDay)
        return code.mid(7, 2).toInt() > code.mid(2, 2).toInt();
    return endDay > startDay || (startDay >= 28 && endDay <= 2);
}

QString periodText(const QString &code) {
    QString description = dayHourText(code.left(4)) + " 至 " + dayHourText(code.mid(5));
    if (code.mid(5, 2).toInt() < code.left(2).toInt())
        description += "（跨月）";
    return description;
}
}

WeatherReport::Report WeatherReport::parse(const QString &text) {
    Report result;
    QString normalized = text.trimmed().toUpper();
    if (normalized.endsWith('='))
        normalized.chop(1);
    if (normalized.contains('=')) {
        result.error = "请一次粘贴一份 METAR、SPECI 或 TAF 报文。";
        return result;
    }
    const QStringList tokens = normalized.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (tokens.isEmpty())
        return result;
    qsizetype index = 0;
    auto add = [&result](const QString &name, const QString &code, const QString &description) {
        result.fields.append({name, code, description});
    };
    if (tokens[index] == "METAR" || tokens[index] == "SPECI" || tokens[index] == "TAF") {
        result.type = tokens[index] == "TAF" ? Type::Taf : tokens[index] == "SPECI" ? Type::Speci : Type::Metar;
        ++index;
    }
    QStringList flags;
    while (index < tokens.size() && (tokens[index] == "COR" || tokens[index] == "AMD"))
        flags << tokens[index++];
    const qsizetype stationIndex = index;
    static const QRegularExpression periodPattern("^\\d{4}/\\d{4}$");
    if (result.type == Type::Unknown) {
        // Only the header validity period identifies a prefix-free TAF. METAR also has TEMPO/BECMG.
        const bool hasPeriod = stationIndex + 2 < tokens.size() && periodPattern.match(tokens[stationIndex + 2]).hasMatch();
        result.type = flags.contains("AMD") || hasPeriod ? Type::Taf : Type::Metar;
    }
    const bool isTaf = result.type == Type::Taf;
    add("报文类型", isTaf ? "TAF" : result.type == Type::Speci ? "SPECI" : "METAR",
        isTaf ? "机场天气预报" : result.type == Type::Speci ? "特殊天气报告" : "例行天气报告");
    for (const auto &flag : flags)
        add("报告标志", flag, flag == "AMD" ? "修订预报" : "更正报告");
    static const QRegularExpression station("^[A-Z][A-Z0-9]{3}$");
    if (index >= tokens.size() || !station.match(tokens[index]).hasMatch()
        || tokens[index] == "AUTO" || tokens[index] == "TAF" || tokens[index] == "NIL") {
        result.error = "未找到四位机场代码，例如 ZSPD、KSEA。";
        return result;
    }
    add("机场", tokens[index], tokens[index]);
    ++index;
    static const QRegularExpression observation("^(\\d{2})(\\d{4})Z$");
    if (index < tokens.size() && tokens[index] == "NIL") {
        add("报告状态", tokens[index++], "无可用天气报告");
        if (index < tokens.size())
            result.warnings << "NIL 后仍有内容：" + tokens.mid(index).join(' ');
        return result;
    }
    const auto obs = index < tokens.size() ? observation.match(tokens[index]) : QRegularExpressionMatch{};
    if (!obs.hasMatch() || obs.captured(1).toInt() < 1 || obs.captured(1).toInt() > 31
        || !validTime(obs.captured(2))) {
        result.error = (isTaf ? QString("发布时间") : QString("观测时间"))
            + "应为 DDHHMMZ，例如 071230Z；日期与时间必须有效。";
        return result;
    }
    add(isTaf ? "发布时间" : "观测时间", tokens[index++], QString("每月 %1 日 %2（报文未包含年月）")
        .arg(obs.captured(1)).arg(timeText(obs.captured(2))));
    if (isTaf) {
        if (index < tokens.size() && tokens[index] != "NIL") {
            if (!validPeriod(tokens[index])) {
                result.error = "TAF 有效期应为 DDHH/DDHH，例如 0712/0818；结束时刻可使用 24:00。";
                return result;
            }
            add("预报有效期", tokens[index], periodText(tokens[index]));
            ++index;
        } else if (index >= tokens.size()) {
            result.error = "TAF 缺少预报有效期。";
            return result;
        }
        if (index < tokens.size() && (tokens[index] == "NIL" || tokens[index] == "CNL")) {
            add("预报状态", tokens[index], tokens[index] == "NIL" ? "无可用机场天气预报" : "取消该时段的机场天气预报");
            if (++index < tokens.size())
                result.warnings << "预报状态后仍有内容：" + tokens.mid(index).join(' ');
            return result;
        }
    }

    static const QRegularExpression wind("^(VRB|\\d{3})(P?\\d{2,3})(?:G(P?\\d{2,3}))?(KT|MPS|KMH)$");
    static const QRegularExpression variance("^(\\d{3})V(\\d{3})$");
    static const QRegularExpression visibility("^(\\d{4})(NDV|N|NE|E|SE|S|SW|W|NW)?$");
    static const QRegularExpression miles("^([MP]?)(?:(\\d+)/([1-9]\\d*)|(\\d+))SM$");
    static const QRegularExpression wholeMiles("^\\d+$");
    static const QRegularExpression rvr("^R(\\d{2}[LCR]?)/([MP]?\\d{4})(?:V([MP]?\\d{4}))?(FT)?/?([UDN])?$");
    static const QRegularExpression cloud("^(FEW|SCT|BKN|OVC|VV)(\\d{3}|///)(CB|TCU|///)?$");
    static const QRegularExpression temps("^(M?\\d{2}|//)/(M?\\d{2}|//)?$");
    static const QRegularExpression pressure("^(Q|A)(\\d{4}|////)$");
    static const QRegularExpression trendTime("^(FM|TL|AT)(\\d{4})$");
    static const QRegularExpression forecastFrom("^FM(\\d{2})(\\d{4})$");
    static const QRegularExpression extremeTemperature("^(TX|TN)(M?\\d{2})/(\\d{4})Z$");
    static const QRegularExpression windShear("^WS(\\d{3})/(\\d{3})(\\d{2,3})(KT|MPS|KMH)$");
    static const QHash<QString, QString> cover{
        {"FEW", "少云（1–2/8）"}, {"SCT", "疏云（3–4/8）"},
        {"BKN", "多云（5–7/8）"}, {"OVC", "阴天（8/8）"}, {"VV", "垂直能见度"}
    };
    static const QHash<QString, QString> clear{
        {"SKC", "晴空"}, {"CLR", "自动观测：12,000 ft 以下未探测到云"},
        {"NSC", "无重要云（不代表完全无云）"}, {"NCD", "自动观测未探测到云"}
    };
    QString scope = isTaf ? "基础预报 · " : "";
    bool hasWeather = false;
    bool nil = false;
    bool remarks = false;
    auto field = [&](const QString &name, const QString &code, const QString &description) {
        add(scope + name, code, description);
        if (scope.isEmpty() || scope == "基础预报 · ")
            hasWeather = true;
    };
    for (; index < tokens.size(); ++index) {
        const QString code = tokens[index];
        if (code == "RMK") {
            add("备注（原文）", tokens.mid(index).join(' '), "保留 RMK 内容，未逐项解码。");
            remarks = true;
            break;
        }
        if (code == "AUTO" || code == "COR") {
            add("报告标志", code, code == "AUTO" ? "自动观测报告" : "更正报告");
        } else if (code == "NIL") {
            add("报告状态", code, "无可用天气报告");
            nil = true;
        } else if (nil) {
            add("未识别", code, "NIL 后的额外内容，未作为天气解析。");
            result.warnings << code;
        } else if (isTaf && (code.startsWith("FM") || code == "BECMG" || code == "TEMPO"
                            || code.startsWith("PROB"))) {
            if (code.startsWith("FM")) {
                const auto match = forecastFrom.match(code);
                if (!match.hasMatch() || !validDay(match.captured(1)) || !validTime(match.captured(2))) {
                    result.error = "TAF 的 FM 变化组应为 FMDDHHMM，例如 FM071800。";
                    return result;
                }
                scope = "FM 预报 · ";
                add("变化组", code, QString("从 %1 日 %2 起，以以下条件替换之前的基本预报，直到下一个 FM 组或预报结束。")
                    .arg(match.captured(1), timeText(match.captured(2))));
            } else {
                QString raw = code;
                QString description;
                QString probability;
                bool temporary = code == "TEMPO";
                if (code.startsWith("PROB")) {
                    if (code != "PROB30" && code != "PROB40") {
                        result.error = "TAF 概率组应为 PROB30 或 PROB40。";
                        return result;
                    }
                    probability = code.mid(4) + "% ";
                    if (index + 1 < tokens.size() && tokens[index + 1] == "TEMPO") {
                        temporary = true;
                        raw += " " + tokens[++index];
                    }
                    description = "以下条件出现的概率为 " + probability.trimmed();
                    if (temporary)
                        description += "，属于暂时性变化";
                    scope = probability + (temporary ? "暂时变化 · " : "概率预报 · ");
                } else {
                    scope = temporary ? "暂时变化 · " : "逐渐变化 · ";
                    description = temporary ? "在以下时段暂时出现所列条件，时段外沿用基本预报。"
                        : "在以下时段逐渐转变为所列条件；未列出的要素沿用之前预报。";
                }
                if (index + 1 >= tokens.size() || !validPeriod(tokens[index + 1])) {
                    result.error = "TAF 的 " + raw + " 变化组缺少有效时段，应为 DDHH/DDHH。";
                    return result;
                }
                const QString period = tokens[++index];
                add("变化组", raw + " " + period, periodText(period) + "；" + description);
            }
            if (index + 1 >= tokens.size() || tokens[index + 1] == "RMK" || tokens[index + 1] == "BECMG"
                || tokens[index + 1] == "TEMPO" || tokens[index + 1].startsWith("FM") || tokens[index + 1].startsWith("PROB")) {
                result.error = "TAF 变化组后缺少预报天气要素。";
                return result;
            }
        } else if (isTaf && (code.startsWith("TX") || code.startsWith("TN"))) {
            const auto match = extremeTemperature.match(code);
            if (!match.hasMatch() || !validDayHour(match.captured(3), true)) {
                result.error = "TAF 最高／最低温度应为 TX温度/DDHHZ 或 TN温度/DDHHZ，例如 TX30/0714Z。";
                return result;
            }
            add(match.captured(1) == "TX" ? "预报最高温度" : "预报最低温度", code,
                temperature(match.captured(2)) + "，预计出现在 " + dayHourText(match.captured(3)));
        } else if (isTaf && code.startsWith("WS") && code != "WS") {
            const auto match = windShear.match(code);
            if (!match.hasMatch() || match.captured(2).toInt() > 360) {
                add(scope + "未识别", code, "无法解析低空风切变。");
                result.warnings << code;
                continue;
            }
            const QString unit = match.captured(4) == "KT" ? "kt" : match.captured(4) == "MPS" ? "m/s" : "km/h";
            field("低空风切变", code, QString("预报低空风切变；%1 ft AGL 处风向 %2°（真北），风速 %3 %4")
                .arg(match.captured(1).toInt() * 100).arg(match.captured(2)).arg(match.captured(3).toInt()).arg(unit));
        } else if (!isTaf && code == "NOSIG") {
            add("趋势", code, "未来两小时预计无显著变化");
            scope = "趋势 · ";
        } else if (code == "BECMG" || code == "TEMPO") {
            scope = code == "BECMG" ? "逐渐变化 · " : "暂时变化 · ";
            add("趋势", code, code == "BECMG" ? "预计逐渐转变为以下条件" : "预计暂时出现以下条件");
        } else if (const auto match = trendTime.match(code); !isTaf && match.hasMatch() && !scope.isEmpty()
                   && validTime(match.captured(2))) {
            const QString type = match.captured(1);
            add(scope + "时间", code, (type == "FM" ? "从 " : type == "TL" ? "至 " : "在 ")
                + timeText(match.captured(2)));
        } else if (const auto match = wind.match(code); match.hasMatch()
                   && (match.captured(1) == "VRB" || match.captured(1).toInt() <= 360)) {
            const QString unit = match.captured(4) == "KT" ? "kt" : match.captured(4) == "MPS" ? "m/s" : "km/h";
            QString description = match.captured(1) == "VRB" ? "风向不定" : "风向 " + match.captured(1) + "°（真北）";
            description += "，风速 " + boundValue(match.captured(2), unit);
            if (!match.captured(3).isEmpty())
                description += "，阵风 " + boundValue(match.captured(3), unit);
            if (match.captured(1) == "000" && match.captured(2).toInt() == 0)
                description = "静风";
            field("风", code, description);
        } else if (const auto match = variance.match(code); match.hasMatch()
                   && match.captured(1).toInt() <= 360 && match.captured(2).toInt() <= 360) {
            field("风向变化", code, QString("风向在 %1° 与 %2° 之间变化").arg(match.captured(1), match.captured(2)));
        } else if (code == "CAVOK") {
            field("能见度与天气", code, "能见度 ≥ 10 km；无重要天气；无低于 5,000 ft 或最高最低扇区高度（取较高者）的云，且无 CB/TCU。");
        } else if (const auto match = visibility.match(code); match.hasMatch()) {
            const int value = match.captured(1).toInt();
            QString description = value == 9999 ? "≥ 10 km" : value == 0 ? "< 50 m" : QString::number(value) + " m";
            const QString direction = match.captured(2);
            if (!direction.isEmpty())
                description += direction == "NDV" ? "（未报告方向变化）" : "（方向 " + direction + "）";
            field("能见度", code, description);
        } else if (const auto match = rvr.match(code); match.hasMatch()
                   && match.captured(1).left(2).toInt() >= 1 && match.captured(1).left(2).toInt() <= 36) {
            const QString unit = match.captured(4).isEmpty() ? "m" : "ft";
            QString description = "跑道 " + match.captured(1) + "：" + boundValue(match.captured(2), unit);
            if (!match.captured(3).isEmpty())
                description += " 至 " + boundValue(match.captured(3), unit);
            const QString trend = match.captured(5);
            if (!trend.isEmpty())
                description += trend == "U" ? "，上升趋势" : trend == "D" ? "，下降趋势" : "，无明显变化";
            field("跑道视程", code, description);
        } else if (code.endsWith("SM") || (wholeMiles.match(code).hasMatch() && index + 1 < tokens.size()
                   && tokens[index + 1].contains('/') && tokens[index + 1].endsWith("SM"))) {
            QString raw = code;
            double whole = 0;
            QString part = code;
            if (!code.endsWith("SM")) {
                whole = code.toDouble();
                part = tokens[++index];
                raw += " " + part;
            }
            const auto match = miles.match(part);
            if (!match.hasMatch()) {
                add(scope + "未识别", raw, "无法解析能见度。");
                result.warnings << raw;
                continue;
            }
            const double value = whole + (match.captured(4).isEmpty()
                ? match.captured(2).toDouble() / match.captured(3).toDouble() : match.captured(4).toDouble());
            const QString bound = match.captured(1) == "M" ? "< " : match.captured(1) == "P" ? "> " : "";
            field("能见度", raw, QString("%1%2 SM（%1%3 m）").arg(bound).arg(value).arg(value * 1609.344, 0, 'f', 0));
        } else if (const auto match = cloud.match(code); match.hasMatch()) {
            const bool vertical = match.captured(1) == "VV";
            const QString height = match.captured(2) == "///" ? "未报告"
                : QString::number(match.captured(2).toInt() * 100) + " ft AGL";
            QString description = cover.value(match.captured(1)) + (vertical ? "：" : "，云底 ") + height;
            if (match.captured(3) == "CB")
                description += "，积雨云";
            else if (match.captured(3) == "TCU")
                description += "，浓积云";
            field(vertical ? "垂直能见度" : "云层", code, description);
        } else if (clear.contains(code)) {
            field("云层", code, clear.value(code));
        } else if (const auto match = temps.match(code); match.hasMatch()) {
            field("温度／露点", code, "气温 " + temperature(match.captured(1)) + "，露点 " + temperature(match.captured(2)));
        } else if (const auto match = pressure.match(code); match.hasMatch()) {
            QString description;
            if (match.captured(2) == "////")
                description = "未报告";
            else if (match.captured(1) == "Q")
                description = QString::number(match.captured(2).toInt()) + " hPa";
            else {
                const double inHg = match.captured(2).toDouble() / 100;
                description = QString("%1 inHg（约 %2 hPa）").arg(inHg, 0, 'f', 2).arg(inHg * 33.8639, 0, 'f', 1);
            }
            field("修正海压 QNH", code, description);
        } else if (code == "NSW") {
            field("天气", code, "无重要天气");
        } else if (code.startsWith("RE") && !weather(code.mid(2)).isEmpty()) {
            field("近期天气", code, weather(code.mid(2)));
        } else if (code == "WS" && index + 1 < tokens.size()) {
            static const QRegularExpression runway("^R(0[1-9]|[12]\\d|3[0-6])[LCR]?$");
            if (index + 2 < tokens.size() && tokens[index + 1] == "ALL" && tokens[index + 2] == "RWY") {
                field("风切变", "WS ALL RWY", "所有跑道存在风切变");
                index += 2;
            } else if (runway.match(tokens[index + 1]).hasMatch()) {
                const QString runwayCode = tokens[++index];
                field("风切变", "WS " + runwayCode, "跑道 " + runwayCode.mid(1) + " 存在风切变");
            } else {
                add(scope + "未识别", code, "无法解析风切变跑道信息。");
                result.warnings << code;
            }
        } else if (code == "$") {
            add("设备状态", code, "自动观测设备需要维护");
        } else if (const QString description = weather(code); !description.isEmpty()) {
            field("天气", code, description);
        } else {
            add(scope + "未识别", code, "保留原文，未解码。");
            result.warnings << code;
        }
    }
    if (!hasWeather && !nil)
        result.warnings.prepend(isTaf ? "未找到可解析的基础预报天气要素。" : "未找到可解析的观测天气要素。");
    if (remarks)
        result.warnings << "备注 RMK 已保留原文，未逐项解码。";
    return result;
}
