#include "../../EditorSupport.h"
#include "TaikoEditorMode.h"
#include "Parsing/ChartParser.h"
#include <algorithm>
#include <format>
#include <sstream>
using namespace editor_ui;

namespace
{
    std::string Join(const std::vector<std::string> &values)
    {
        std::string result;
        for (const auto &value : values)
        {
            if (!result.empty())
                result += '\n';
            result += value;
        }
        return result;
    }
    std::string JoinTags(const std::vector<std::string> &tags)
    {
        std::string result;
        for (const auto &tag : tags)
            result += (result.empty() ? "" : ", ") + tag;
        return result;
    }
    std::vector<std::string> SplitTags(const std::string &value)
    {
        std::vector<std::string> result;
        std::istringstream in(value);
        std::string tag;
        while (std::getline(in, tag, ','))
        {
            const auto first = tag.find_first_not_of(" \t");
            if (first == std::string::npos)
                continue;
            result.push_back(tag.substr(first, tag.find_last_not_of(" \t") - first + 1));
        }
        return result;
    }
    std::vector<std::string> Lines(const std::string &value)
    {
        std::vector<std::string> result;
        std::istringstream in(value);
        std::string line;
        while (std::getline(in, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (!line.empty())
                result.push_back(line);
        }
        return result;
    }
    using Apply = std::function<void(PatternDocument &, const std::string &)>;
} // namespace

void TaikoEditorMode::DrawMetadata(IEditorModeCanvas &canvas, IEditorContext &state,
                                   const TextCatalog &catalog)
{
    const auto &text = Editor(catalog);
    const auto body = canvas.Areas().body;
    // Three columns: the pattern's own values, its files, and the hit-sound table.
    const float inner = body.width - 48 * 3;
    const std::array<float, 3> widths{inner / 3.05F, inner * 1.15F / 3.05F, inner * .9F / 3.05F};
    std::array<float, 3> lefts{};
    lefts[0] = body.x + 24;
    lefts[1] = lefts[0] + widths[0] + 48;
    lefts[2] = lefts[1] + widths[1] + 48;
    const float top = body.y + 14;
    for (std::size_t column = 1; column < lefts.size(); ++column)
        canvas.Box({lefts[column] - 24, body.y, 1, body.height}, ui_colors::theme::Rule);
    const std::array<std::string_view, 3> headings{"Editor.metadataPattern", "Editor.metadataFiles",
                                                   "Editor.metadataHitSounds"};
    for (std::size_t column = 0; column < headings.size(); ++column)
        canvas.Text({lefts[column], top, widths[column], 22}, std::wstring(catalog.Get(headings[column])), 16,
                    ui_colors::theme::Ink);

    // Fields commit on Enter or when focus leaves; the document validates the whole pattern.
    const auto field = [&state, &canvas](PhantomCore::Rect r, std::wstring title, std::string value, Apply apply,
                                         bool number = false) {
        canvas.TextField({r.x, r.y + 20, r.width, 32}, std::move(title), Wide(value),
                         [&state, apply](const std::string &typed)
                         {
                             auto p = state.Document().Pattern();
                             apply(p, typed);
                             state.Replace(p, state.Document().Effects());
                             state.RequestRebuild();
                         },
                         number);
    };
    const auto &p = state.Document().Pattern();
    const float x0 = lefts[0], w0 = widths[0], half0 = (w0 - 10) * .5F;
    float y = top + 36;
    // Each block starts with its romanized line, as in the file. An empty one is fine: the
    // romanized display then uses the text below it when that is already in Roman letters.
    field({x0, y, w0, 52}, std::wstring(catalog.Get("Editor.romanizedName")), p.romanizedName,
          [](auto &d, const auto &value) { d.romanizedName = value; });
    y += 62;
    field({x0, y, w0, 52}, std::wstring(text.patternName), p.name, [](auto &d, const auto &value) { d.name = value; });
    y += 62;
    field({x0, y, w0, 52}, std::wstring(catalog.Get("Editor.romanizedMakers")), p.romanizedMaker,
          [](auto &d, const auto &value) { d.romanizedMaker = value; });
    y += 62;
    // Chart by: one row per creator; the first (★) is the one shown in song select and results.
    auto makers = p.makers;
    // Add shows an empty row; it joins the document once a name is typed.
    if (makers.empty() || addingMaker_)
        makers.emplace_back();
    for (std::size_t i = 0; i < makers.size(); ++i)
    {
        const float rowY = y + 20 + 38 * static_cast<float>(i);
        // Only the first row carries the caption.
        canvas.TextField({x0, rowY, w0 - 38, 32}, i == 0 ? std::wstring(text.makers) : std::wstring{}, Wide(makers[i]),
                         [this, &state, i](const std::string &typed)
                         {
                             addingMaker_ = false;
                             auto d = state.Document().Pattern();
                             if (d.makers.size() <= i)
                                 d.makers.resize(i + 1);
                             d.makers[i] = typed;
                             std::erase_if(d.makers, [](const std::string &maker) { return maker.empty(); });
                             state.Replace(d, state.Document().Effects());
                             state.RequestRebuild();
                         });
        if (i == 0)
            canvas.Text({x0 + w0 - 28, rowY + 6, 20, 20}, L"★", 15, ui_colors::theme::Sun);
        else
            canvas.Button({x0 + w0 - 32, rowY + 2, 28, 28}, L"✕", [&state, i] {
                auto d = state.Document().Pattern();
                if (i < d.makers.size())
                    d.makers.erase(d.makers.begin() + static_cast<std::ptrdiff_t>(i));
                state.Replace(d, state.Document().Effects());
                state.RequestRebuild();
            });
    }
    y += 20 + 38 * static_cast<float>(makers.size());
    canvas.Button({x0, y, 84, 28}, std::wstring(catalog.Get("Editor.add")), [this, &state] {
        addingMaker_ = true;
        state.RequestRebuild();
    });
    y += 38;
    field({x0, y, w0, 52}, std::wstring(text.tags), JoinTags(p.tags), [](auto &d, const auto &value) { d.tags = SplitTags(value); });
    y += 62;
    field({x0, y, half0, 52}, std::wstring(catalog.Get("Editor.accuracyDifficulty")), p.judgementLevel.Format(),
          [](auto &d, const auto &value) { d.judgementLevel = ParseJudgeLevel(value); }, true);
    field({x0 + half0 + 10, y, half0, 52}, std::wstring(catalog.Get("Editor.hp")), p.hpLevel.Format(),
          [](auto &d, const auto &value) { d.hpLevel = HpLevel{Number(value)}; }, true);
    y += 62;
    field({x0, y, half0, 52}, std::wstring(text.baseBpm), std::format("{:g}", p.baseBpm),
          [](auto &d, const auto &value) { d.baseBpm = Number(value); }, true);
    field({x0 + half0 + 10, y, half0, 52}, std::wstring(text.offsetMilliseconds), std::format("{:g}", p.patternOffsetMilliseconds),
          [](auto &d, const auto &value) { d.patternOffsetMilliseconds = Number(value); }, true);

    const float x1 = lefts[1], w1 = widths[1];
    y = top + 36;
    // The song's YMM is decided by the song folder, so it is shown, not edited here.
    const auto ymm = canvas.Areas().musicMetadata.filename();
    canvas.Text({x1, y, w1, 18}, std::wstring(text.ymmFile), 12, ui_colors::theme::Caption);
    canvas.Text({x1, y + 26, w1, 22}, ymm.empty() ? std::wstring(L"—") : ymm.wstring(), 14, ui_colors::theme::InkSoft);
    y += 62;
    // The effect file is chosen by the first save; it is shown, not edited here.
    canvas.Text({x1, y, w1, 18}, std::wstring(catalog.Get("Editor.effectFile")), 12, ui_colors::theme::Caption);
    canvas.Text({x1, y + 26, w1, 22}, p.effectFile.empty() ? std::wstring(L"—") : p.effectFile.wstring(), 14,
                ui_colors::theme::InkSoft);
    y += 62;
    // BG: the whole image beside its path and x/y offset. It shows which image the chart uses, so the
    // offset does not move it; Preview and the play screen show where the offset puts it.
    const auto preview = canvas.Areas().background;
    const PhantomCore::Rect thumbnail{x1, y + 20, 128, 72};
    canvas.Box(thumbnail, ui_colors::Rgb(0xDCE7F5));
    if (preview.empty())
        canvas.Text({thumbnail.x + 34, thumbnail.y + 28, 80, 16}, L"NO IMAGE", 11, ui_colors::theme::Caption);
    else
        canvas.ContainedImage(thumbnail, preview, {1, 1, 1, 1});
    const float bgLeft = x1 + 140, bgWidth = w1 - 140;
    field({bgLeft, y, bgWidth, 52}, std::wstring(catalog.Get("Editor.backgroundFile")), Utf8(p.backgroundFile.wstring()),
          [](auto &d, const auto &value) { d.backgroundFile = std::filesystem::path(Wide(value)); });
    y += 62;
    // BG offset: x and y in 1280x720 logical units.
    const float halfBg = (bgWidth - 10) * .5F;
    field({bgLeft, y, halfBg, 52}, L"x", std::format("{:g}", p.backgroundOffsetX),
          [](auto &d, const auto &value) { d.backgroundOffsetX = Number(value); }, true);
    field({bgLeft + halfBg + 10, y, halfBg, 52}, L"y", std::format("{:g}", p.backgroundOffsetY),
          [](auto &d, const auto &value) { d.backgroundOffsetY = Number(value); }, true);

    const float x2 = lefts[2], w2 = widths[2];
    y = top + 36;
    std::string sounds;
    for (const auto &[id, path] : state.Document().Effects().hitSounds)
        sounds += id + ": " + Utf8(path.wstring()) + "\n";
    std::size_t row = 0;
    for (const auto &[id, path] : state.Document().Effects().hitSounds)
    {
        const float rowY = y + 42 * static_cast<float>(row++);
        canvas.Text({x2, rowY + 10, 28, 20}, Wide(id), 15, ui_colors::theme::Ink);
        canvas.Text({x2 + 32, rowY + 10, w2 - 32, 20}, path.wstring(), 13, ui_colors::theme::InkSoft);
        canvas.Box({x2, rowY + 41, w2, 1}, ui_colors::theme::Rule);
    }
    if (row == 0)
        canvas.Text({x2, y + 10, w2, 20}, L"—", 15, ui_colors::theme::Caption);
    canvas.Button({x2, y + 42 * static_cast<float>(row) + 12, std::min(w2, 140.0F), 32},
                  std::wstring(text.hitSoundTable),
                  [&state, &catalog, sounds]
                  {
                      const auto &labels = Editor(catalog);
                      if (auto edited = EditText(labels.hitSoundTable.data(), sounds, labels))
                      {
                          const auto parsed = ChartParser{}.ParseEffect("[HitSounds]\n" + *edited);
                          if (!parsed.Succeeded())
                              throw std::invalid_argument("Invalid YME hit sound table.");
                          auto effects = state.Document().Effects();
                          effects.hitSounds = parsed.document.hitSounds;
                          state.Replace(state.Document().Pattern(), std::move(effects));
                          state.RequestRebuild();
                      }
                  });
}
