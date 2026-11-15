#include "TaikoEditorNoteInspector.h"
#include "../../EditorSupport.h"
#include "Taiko/Submodules/TaikoNoteOptions.h"
#include "TaikoEditorPlacement.h"
#include "TaikoEditorTool.h"
#include "Texts/EditorScene/Taiko/TaikoEditorTexts.h"
#include <format>
using namespace editor_ui;
namespace
{

    void SetOption(PatternNote &note, std::string_view name, std::string value, bool removeBare = false)
    {
        std::erase_if(
            note.extraData,
            [&](const std::string &field)
            {
                const auto equals = field.find('=');
                return equals != std::string::npos
                           ? EqualsInsensitive(Trim(std::string_view(field).substr(0, equals)), name)
                           : removeBare &&
                                 std::ranges::all_of(Trim(field), [](char c) { return c >= '0' && c <= '9'; });
            });
        if (!value.empty())
            note.extraData.push_back(std::string(name) + "=" + value);
    }
    void ValidateCount(const std::string &value, bool allowAuto)
    {
        if (value.empty() && allowAuto)
            return;
        const auto count = Integer(value);
        if (count < 1 || count > 1024)
            throw std::invalid_argument("Note count/division must be 1..1024.");
    }
} // namespace

void TaikoEditorNoteInspector::Select(const IEditorDocument &document, std::size_t id)
{
    selected_ = document.FindNote(id);
    draftChanged_ = false;
    revision_ = 0;
    Refresh(document);
}
bool TaikoEditorNoteInspector::Clear() noexcept
{
    const bool hadSelection = selected_.has_value();
    selected_.reset();
    draftChanged_ = false;
    return hadSelection;
}
bool TaikoEditorNoteInspector::IsSelected(std::size_t id) const noexcept
{
    return selected_ &&
           (selected_->head.note.sourceOrder == id || (selected_->tail && selected_->tail->note.sourceOrder == id));
}
void TaikoEditorNoteInspector::Refresh(const IEditorDocument &document)
{
    if (!selected_ || revision_ == document.Revision())
        return;
    selected_ = document.FindNote(selected_->head.note.sourceOrder);
    revision_ = document.Revision();
    if (!selected_ || draftChanged_)
        return;
    const auto &note = selected_->head.note;
    draft_.startMeasure = std::to_string(note.position.measure + 1);
    draft_.startBeat = Fraction(note.position.fraction);
    draft_.endMeasure = selected_->tail ? std::to_string(selected_->tail->note.position.measure + 1) : "";
    draft_.endBeat = selected_->tail ? Fraction(selected_->tail->note.position.fraction) : "";
    draft_.tool = note.keyType;
    if (note.keyType == 17)
    {
        std::vector<Diagnostic> diagnostics;
        if (ReadBuzzAction(note, diagnostics) == TaikoAction::Kat)
            draft_.tool = 18;
    }
    draft_.hitCount = std::string(FindOption(note, "HitCount").value_or(""));
    if (draft_.hitCount.empty())
        for (const auto &field : note.extraData)
            if (field.find('=') == std::string::npos)
            {
                draft_.hitCount = std::string(Trim(field));
                break;
            }
    draft_.tickDivision = std::string(FindOption(note, "TickDivision").value_or("16"));
    draft_.hitSound = note.hitSound;
}
void TaikoEditorNoteInspector::Field(IEditorModeCanvas &canvas, IEditorContext &state,
                                     const TextCatalog &catalog, float y, std::wstring_view label,
                                     std::string &value, float x, float width)
{
    static_cast<void>(catalog);
    canvas.TextField({x, y + 20, width, 32}, std::wstring(label), Wide(value),
                     [this, &state, &value](const std::string &typed)
                     {
                         value = typed;
                         draftChanged_ = true;
                         state.RequestRebuild();
                     },
                     true);
}
void TaikoEditorNoteInspector::PositionFields(IEditorModeCanvas &canvas, IEditorContext &state,
                                              const TextCatalog &catalog, float y,
                                              std::wstring_view label, std::string &measure, std::string &beat)
{
    const auto &text = TaikoEditor(catalog);
    const auto body = canvas.Areas().inspector;
    const float half = (body.width - 8) * .5F;
    Field(canvas, state, catalog, y, std::wstring(label) + L" " + std::wstring(text.measure), measure, body.x, half);
    Field(canvas, state, catalog, y, text.beatPosition, beat, body.x + half + 8, half);
}
void TaikoEditorNoteInspector::Draw(IEditorModeCanvas &canvas, IEditorContext &state,
                                    const TextCatalog &catalog)
{
    Refresh(state.Document());
    const auto &text = TaikoEditor(catalog);
    const auto body = canvas.Areas().inspector;
    if (!selected_)
    {
        canvas.Text({body.x, body.y, body.width, 40}, std::wstring(text.selectNoteHelp), 13, ui_colors::theme::Caption);
        return;
    }
    const auto millisecondsText = [&catalog](double milliseconds) {
        return std::vformat(catalog.Get("Result.millisecondsFormat"), std::make_wformat_args(milliseconds));
    };
    canvas.Button({body.x, body.y, body.width, 30}, std::wstring(Name(draft_.tool, text.toolVariants)),
                  [this, &state]
                  {
                      draft_.tool = NextInGroup(draft_.tool);
                      draftChanged_ = true;
                      state.RequestRebuild();
                  });
    const auto &head = selected_->head;
    float y = body.y + 42;
    PositionFields(canvas, state, catalog, y, text.startPosition, draft_.startMeasure, draft_.startBeat);
    canvas.Text({body.x, y + 56, body.width, 18}, millisecondsText(head.timing.count() / 1000.0), 13,
                ui_colors::theme::InkSoft);
    y += 80;
    const auto *tool = Find(draft_.tool);
    if (tool && FindTaikoNote(static_cast<int>(tool->note))->longNote)
    {
        PositionFields(canvas, state, catalog, y, text.endPosition, draft_.endMeasure, draft_.endBeat);
        if (selected_->tail)
            canvas.Text({body.x, y + 56, body.width, 18}, millisecondsText(selected_->tail->timing.count() / 1000.0),
                        13, ui_colors::theme::InkSoft);
        y += 80;
    }
    const float half = (body.width - 8) * .5F;
    if (UsesHitCount(draft_.tool))
        Field(canvas, state, catalog, y, text.hitCount, draft_.hitCount, body.x, half);
    if (UsesTickDivision(draft_.tool))
        Field(canvas, state, catalog, y, text.tickDivision, draft_.tickDivision, body.x, half);
    const bool counted = UsesHitCount(draft_.tool) || UsesTickDivision(draft_.tool);
    Field(canvas, state, catalog, y, text.hitSound, draft_.hitSound, counted ? body.x + half + 8 : body.x,
          counted ? half : body.width);
    y += 60;
    if (selected_->tail && UsesTickDivision(head.note.keyType))
    {
        std::vector<Diagnostic> diagnostics;
        const auto division = ReadPositiveOption(head.note, "TickDivision", 16, diagnostics);
        if (division)
        {
            auto count = state.Document().Timeline().CountSubdivisions(head.note.position,
                                                                       selected_->tail->note.position, *division);
            if (head.note.keyType == 17 && count != 0)
                --count;
            canvas.Text({body.x, y, body.width, 20}, std::wstring(text.tickCount) + L" " + std::to_wstring(count), 13,
                        ui_colors::theme::InkSoft);
        }
    }
    const float actions = body.y + body.height - 32;
    canvas.Button({body.x + body.width - 96, actions, 96, 32}, std::wstring(text.apply), [this, &state] { Apply(state); },
                  true);
    canvas.Button({body.x + body.width - 96 - 8 - 96, actions, 96, 32}, std::wstring(text.resetDraft),
                  [this, &state]
                  {
                      Select(state.Document(), selected_->head.note.sourceOrder);
                      state.RequestRebuild();
                  });
}
void TaikoEditorNoteInspector::Apply(IEditorContext &state)
{
    Refresh(state.Document());
    if (!selected_)
        throw std::out_of_range("Selected note no longer exists.");
    const auto *tool = Find(draft_.tool);
    if (!tool)
        throw std::invalid_argument("Unknown Taiko note type.");
    const auto *sourceTool = Find(selected_->head.note.keyType);
    if (!sourceTool || tool->group != sourceTool->group)
        throw std::invalid_argument("Note type changes must stay within the selected note's tool group.");
    auto head = selected_->head.note;
    head.position = Position(draft_.startMeasure, draft_.startBeat);
    head.keyType = static_cast<int>(tool->note);
    head.hitSound = draft_.hitSound;
    std::optional<MusicalPosition> end;
    if (FindTaikoNote(head.keyType)->longNote)
    {
        if (draft_.endMeasure.empty() || draft_.endBeat.empty())
            throw std::invalid_argument("A long note requires an end measure and beat.");
        end = Position(draft_.endMeasure, draft_.endBeat);
    }
    if (UsesHitCount(draft_.tool))
    {
        ValidateCount(draft_.hitCount, true);
        SetOption(head, "HitCount", draft_.hitCount, true);
    }
    if (UsesTickDivision(draft_.tool))
    {
        ValidateCount(draft_.tickDivision, false);
        SetOption(head, "TickDivision", draft_.tickDivision);
    }
    if (head.keyType == 17)
        SetOption(head, "Action", draft_.tool == 18 ? "Kat" : "Don");
    if (UsesHitCount(draft_.tool))
    {
        if (!end)
            throw std::invalid_argument("A counted note requires an end position.");
        std::vector<Diagnostic> diagnostics;
        if (!ReadHitCount(head, state.Document().Timeline().CountSubdivisions(head.position, *end, 12),
                                   diagnostics))
            throw std::invalid_argument(diagnostics.front().message);
    }
    ValidatePlacement(state.Document(), head.position, head.keyType, end, head.sourceOrder);
    const auto id = head.sourceOrder;
    state.UpdateNote(id, std::move(head), end);
    draftChanged_ = false;
    revision_ = 0;
    Refresh(state.Document());
}
