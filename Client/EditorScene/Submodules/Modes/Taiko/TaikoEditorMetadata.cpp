#include "../../EditorSupport.h"
#include "TaikoEditorMode.h"
#include "Parsing/ChartParser.h"
#include <algorithm>
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

    const auto field = [&state, &canvas, &catalog](PhantomCore::Rect r, std::wstring title, std::string value,
                                                   Apply apply) {
        canvas.Text({r.x, r.y, r.width, 18}, title, 12, ui_colors::theme::Caption);
        canvas.Button({r.x, r.y + 20, r.width, 32}, Wide(value),
                      [&state, &catalog, title, value, apply]
                      {
                          if (auto edited = EditText(title, value, Editor(catalog)))
                          {
                              auto p = state.Document().Pattern();
                              apply(p, *edited);
                              state.Replace(p, state.Document().Effects());
                              state.RequestRebuild();
                          }
                      });
    };
    const auto &p = state.Document().Pattern();
    const float x0 = lefts[0], w0 = widths[0], half0 = (w0 - 10) * .5F;
    float y = top + 36;
    field({x0, y, w0, 52}, std::wstring(text.patternName), p.name, [](auto &d, const auto &value) { d.name = value; });
    y += 62;
    field({x0, y, w0, 52}, std::wstring(text.makers), Join(p.makers),
          [](auto &d, const auto &value) { d.makers = Lines(value); });
    y += 62;
    field({x0, y, w0, 52}, std::wstring(text.tags), Join(p.tags), [](auto &d, const auto &value) { d.tags = Lines(value); });
    y += 62;
    field({x0, y, half0, 52}, std::wstring(catalog.Get("Editor.accuracyDifficulty")), p.judgementLevel.Format(),
          [](auto &d, const auto &value) { d.judgementLevel = ParseJudgeLevel(value); });
    field({x0 + half0 + 10, y, half0, 52}, std::wstring(catalog.Get("Editor.hp")), p.hpLevel.Format(),
          [](auto &d, const auto &value) { d.hpLevel = HpLevel{Number(value)}; });
    y += 62;
    field({x0, y, half0, 52}, std::wstring(text.baseBpm), std::to_string(p.baseBpm),
          [](auto &d, const auto &value) { d.baseBpm = Number(value); });
    field({x0 + half0 + 10, y, half0, 52}, std::wstring(text.offsetMilliseconds), std::to_string(p.patternOffsetMilliseconds),
          [](auto &d, const auto &value) { d.patternOffsetMilliseconds = Number(value); });

    const float x1 = lefts[1], w1 = widths[1];
    y = top + 36;
    field({x1, y, w1, 52}, std::wstring(text.ymmRelativePath), Utf8(p.musicMetadataFile.wstring()),
          [](auto &d, const auto &value) { d.musicMetadataFile = std::filesystem::path(Wide(value)); });
    y += 62;
    // The effect file is chosen by the first save; it is shown, not edited here.
    canvas.Text({x1, y, w1, 18}, std::wstring(catalog.Get("Editor.effectFile")), 12, ui_colors::theme::Caption);
    canvas.Text({x1, y + 26, w1, 22}, p.effectFile.empty() ? std::wstring(L"—") : p.effectFile.wstring(), 14,
                ui_colors::theme::InkSoft);
    y += 62;
    field({x1, y, w1, 52}, std::wstring(catalog.Get("Editor.backgroundFile")), Utf8(p.backgroundFile.wstring()),
          [](auto &d, const auto &value) { d.backgroundFile = std::filesystem::path(Wide(value)); });
    y += 62;
    field({x1, y, w1, 52}, std::wstring(catalog.Get("Editor.backgroundOffset")),
          std::to_string(p.backgroundOffsetX) + ", " + std::to_string(p.backgroundOffsetY),
          [](auto &d, const auto &value)
          {
              const auto parsed = ChartParser{}.ParsePattern("[Metadata]\noffset: " + value);
              if (!parsed.Succeeded())
                  throw std::invalid_argument("Background offset requires x, y numbers.");
              d.backgroundOffsetX = parsed.document.backgroundOffsetX;
              d.backgroundOffsetY = parsed.document.backgroundOffsetY;
          });

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
