#include "Automation/InterpolationExpression.h"
#include "EditorView.h"
#include <algorithm>
#include <cmath>
#include <format>
using namespace PhantomCore;

using namespace editor_ui;
namespace layout = editor_layout;
namespace theme = ui_colors::theme;

namespace
{
    constexpr float DialogWidth = 760, DialogHeight = 476, ListWidth = 196, RowHeight = 38;
    constexpr std::array<std::string_view, 3> BuiltIns{"Linear", "Exponential", "Harmonic"};
    constexpr std::array<std::string_view, 3> BuiltInFormulas{"A + (B-A)·u", "A^(1-u)·B^u", "AB / ((1-u)B + uA)"};
    // Built-in curves evaluated from 1 to 2 for the preview (Exponential/Harmonic need positive ends).
    double BuiltIn(std::size_t index, double u)
    {
        constexpr double a = 1, b = 2;
        if (index == 1)
            return std::pow(a, 1 - u) * std::pow(b, u);
        if (index == 2)
            return a * b / ((1 - u) * b + u * a);
        return a + (b - a) * u;
    }
    struct Check
    {
        std::string message;
        std::optional<std::size_t> position;
    };
    std::optional<Check> Validate(const std::string &expression)
    {
        try
        {
            static_cast<void>(InterpolationExpression(expression));
            return std::nullopt;
        }
        catch (const InterpolationSyntaxError &error)
        {
            return Check{error.what(), error.Position()};
        }
        catch (const std::exception &error)
        {
            return Check{error.what(), std::nullopt};
        }
    }
    bool ValidName(const std::string &name)
    {
        const bool reserved = std::ranges::find(BuiltIns, name) != BuiltIns.end();
        return !name.empty() && !reserved &&
               std::ranges::all_of(name, [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; });
    }
} // namespace

void EditorView::OpenCurves()
{
    curves_ = {};
    curves_.table = state_.Document().Effects().interpolations;
    for (const auto &[name, expression] : curves_.table)
        curves_.origins.emplace(name, name);
    curves_.selected = state_.effectForm.curve == AutomationCurve::Expression ? state_.effectForm.curveName
                       : state_.effectForm.curve == AutomationCurve::Exponential ? "Exponential"
                       : state_.effectForm.curve == AutomationCurve::Harmonic    ? "Harmonic"
                                                                                 : "Linear";
    curvesOpen_ = true;
    state_.RequestRebuild();
}

void EditorView::CloseCurves()
{
    curvesOpen_ = false;
    curves_ = {};
    state_.RequestRebuild();
}

void EditorView::ApplyCurves()
{
    for (const auto &[name, expression] : curves_.table)
        if (!ValidName(name) || Validate(expression))
            throw std::invalid_argument("Fix the marked curve before applying.");
    auto effects = state_.Document().Effects();
    effects.interpolations = curves_.table;
    // Renamed curves follow their effects; effects of a deleted curve fall back to Linear.
    for (auto &command : effects.commands)
    {
        if (command.curve != AutomationCurve::Expression)
            continue;
        const auto renamed = std::ranges::find_if(curves_.origins, [&](const auto &entry) { return entry.second == command.curveName; });
        if (renamed != curves_.origins.end())
            command.curveName = renamed->first;
        else
        {
            command.curve = AutomationCurve::Linear;
            command.curveName.clear();
        }
    }
    state_.Replace(state_.Document().Pattern(), std::move(effects));
    // The panel's form takes the chosen curve.
    auto &form = state_.effectForm;
    const auto builtIn = std::ranges::find(BuiltIns, curves_.selected);
    form.curve = builtIn == BuiltIns.begin()       ? AutomationCurve::Linear
                 : builtIn == BuiltIns.begin() + 1 ? AutomationCurve::Exponential
                 : builtIn == BuiltIns.begin() + 2 ? AutomationCurve::Harmonic
                                                   : AutomationCurve::Expression;
    form.curveName = builtIn == BuiltIns.end() ? curves_.selected : std::string{};
    CloseCurves();
}

void EditorView::DrawCurvesDialog()
{
    if (!curvesOpen_)
        return;
    // The scrim owns clicks outside the dialog.
    Box({0, 0, frame_.width, layout::Height}, ui_colors::Rgb(0x142F5E, .42F));
    Control({0, 0, frame_.width, layout::Height}, [] {});
    const Rect dialog{(frame_.width - DialogWidth) * .5F, (layout::Height - DialogHeight) * .5F, DialogWidth, DialogHeight};
    Box(dialog, theme::Surface);
    Control(dialog, [] {});
    Label({dialog.x + 24, dialog.y + 16, 300, 28}, std::wstring(texts_.Get("Editor.curvesTitle")),
          {18, theme::Ink, TextAlignment::Leading, 900});
    const Rect close{dialog.x + dialog.width - 44, dialog.y + 14, 32, 32};
    Icon({close.x + 7, close.y + 7, 18, 18}, L"x", theme::InkSoft);
    Control(close, [this] { CloseCurves(); });

    // List: built-ins (read-only, formula shown) then the YME's own curves.
    const float listTop = dialog.y + 64;
    float y = listTop;
    Box({dialog.x + 24, y, ListWidth, 1}, theme::Rule);
    const auto row = [&](const std::string &name, std::wstring caption, bool bad) {
        const Rect r{dialog.x + 24, y, ListWidth, RowHeight};
        const bool on = curves_.selected == name;
        if (on)
        {
            Box(r, White);
            Box({r.x, r.y, 4, r.height}, theme::Brand);
        }
        Label({r.x + 12, r.y, r.width - 24, r.height}, Wide(name), {14, bad ? theme::Danger : theme::Ink, TextAlignment::Leading, 700});
        if (!caption.empty())
            Label({r.x + 12, r.y, r.width - 20, r.height}, std::move(caption), {11, theme::Caption, TextAlignment::Trailing, 600});
        Box({r.x, r.y + r.height - 1, r.width, 1}, theme::Rule);
        Control(r, [this, name] {
            curves_.selected = name;
            state_.RequestRebuild();
        });
        y += RowHeight;
    };
    for (std::size_t i = 0; i < BuiltIns.size(); ++i)
        row(std::string(BuiltIns[i]), {}, false);
    for (const auto &[name, expression] : curves_.table)
        row(name, {}, !ValidName(name) || Validate(expression).has_value());
    Button({dialog.x + 24, y + 6, 84, 30}, std::wstring(texts_.Get("Editor.add")), [this] {
        std::string name = "Curve";
        for (int i = 1; curves_.table.contains(name); ++i)
            name = "Curve" + std::to_string(i);
        curves_.table.emplace(name, "From + (To - From) * u");
        curves_.selected = name;
        state_.RequestRebuild();
    });

    // Editor: name and expression, check result, insertion chips and a preview.
    const float left = dialog.x + 24 + ListWidth + 20, editWidth = dialog.x + dialog.width - 24 - left;
    const auto builtIn = std::ranges::find(BuiltIns, curves_.selected);
    const bool readOnly = builtIn != BuiltIns.end();
    const auto entry = curves_.table.find(curves_.selected);
    std::string expression = readOnly ? std::string(BuiltInFormulas[static_cast<std::size_t>(builtIn - BuiltIns.begin())])
                             : entry != curves_.table.end() ? entry->second
                                                            : std::string{};
    float top = listTop;
    Label({left, top, editWidth, 18}, std::wstring(texts_.Get("Editor.curveName")), {12, theme::Caption, TextAlignment::Leading, 600});
    const Rect nameField{left, top + 20, editWidth, 32};
    Box(nameField, readOnly ? ui_colors::Rgb(0xEAF1FA) : Color{1, 1, 1, .74F});
    Box({nameField.x, nameField.y + nameField.height - 2, nameField.width, 2}, ui_colors::Rgb(0xB5CFF5));
    Label({nameField.x + 8, nameField.y, nameField.width - 16, nameField.height}, Wide(curves_.selected), {15, theme::Ink});
    if (!readOnly && entry != curves_.table.end())
        Control(nameField, [this] {
            if (auto edited = EditText(std::wstring(texts_.Get("Editor.curveName")), curves_.selected, Texts()))
                RenameCurve(*edited);
        });
    top += 62;
    Label({left, top, editWidth, 18}, std::wstring(texts_.Get("Editor.curveExpression")), {12, theme::Caption, TextAlignment::Leading, 600});
    const Rect expressionField{left, top + 20, editWidth, 32};
    Box(expressionField, readOnly ? ui_colors::Rgb(0xEAF1FA) : Color{1, 1, 1, .74F});
    Box({expressionField.x, expressionField.y + expressionField.height - 2, expressionField.width, 2}, ui_colors::Rgb(0xB5CFF5));
    Label({expressionField.x + 8, expressionField.y, expressionField.width - 16, expressionField.height}, Wide(expression),
          {14, theme::Ink, TextAlignment::Leading, 600});
    if (!readOnly && entry != curves_.table.end())
        Control(expressionField, [this, expression] {
            if (auto edited = EditText(std::wstring(texts_.Get("Editor.curveExpression")), expression, Texts()))
            {
                curves_.table[curves_.selected] = *edited;
                state_.RequestRebuild();
            }
        });
    top += 56;
    if (!readOnly)
    {
        const auto problem = Validate(expression);
        const bool nameOk = ValidName(curves_.selected);
        std::wstring status = !nameOk     ? std::wstring(texts_.Get("Editor.curveNameError"))
                              : problem   ? Wide(problem->message)
                                          : std::wstring(texts_.Get("Editor.curveValid"));
        if (problem && problem->position)
            status += texts_.Format("Editor.curvePositionFormat", *problem->position + 1);
        Label({left, top, editWidth, 18}, status, {12, nameOk && !problem ? theme::Ok : theme::Danger, TextAlignment::Leading, 700});
        // Insertion chips append at the end of the expression.
        float x = left;
        for (const std::string_view chip : {"u", "From", "To", "+", "-", "*", "/", "^", "(", ")", "Bezier(u, 0.25, 0.1, 0.25, 1)"})
        {
            const std::wstring caption = chip.size() > 6 ? L"Bezier" : Wide(std::string(chip));
            const float w = std::max(26.0F, EstimateTextWidth(caption, 12) + 14);
            Button({x, top + 22, w, 26}, caption, [this, chip] {
                auto &text = curves_.table[curves_.selected];
                text += (text.empty() || text.back() == ' ' ? "" : " ") + std::string(chip);
                state_.RequestRebuild();
            });
            x += w + 4;
        }
        top += 56;
    }
    else
        top += 4;

    // Preview: From 0 to To 1 over u, the dashed diagonal is Linear.
    const Rect plot{left, top, editWidth, dialog.y + dialog.height - 72 - top};
    Box(plot, White);
    Box({plot.x, plot.y + plot.height - 1, plot.width, 1}, theme::RuleStrong);
    Box({plot.x, plot.y, 1, plot.height}, theme::RuleStrong);
    for (float t = 0; t < 1; t += .04F)
        Box({plot.x + plot.width * t, plot.y + plot.height * (1 - t) - 1, 3, 1.5F}, ui_colors::Rgb(0x1F4C99, .35F));
    std::optional<InterpolationExpression> compiled;
    if (!readOnly && !Validate(expression))
        compiled.emplace(expression);
    const auto sample = [&](double u) -> std::optional<double> {
        try
        {
            if (readOnly)
                return BuiltIn(static_cast<std::size_t>(builtIn - BuiltIns.begin()), u) - 1;
            if (compiled)
                return compiled->Evaluate(u, 0, 1);
        }
        catch (const std::exception &)
        {
        }
        return std::nullopt;
    };
    std::optional<float> previous;
    for (int i = 0; i <= 96; ++i)
    {
        const double u = i / 96.0;
        const auto value = sample(u);
        if (!value || !std::isfinite(*value))
        {
            previous.reset();
            continue;
        }
        const float x = plot.x + plot.width * static_cast<float>(u);
        const float yValue = plot.y + plot.height * (1 - static_cast<float>(std::clamp(*value, -.1, 1.1)));
        const float from = previous.value_or(yValue);
        Box({x - 1, std::min(from, yValue) - 1, plot.width / 96 + 1, std::abs(yValue - from) + 2.5F}, theme::Brand);
        previous = yValue;
    }

    // Actions: Delete (own curves) at the left, Cancel and Apply at the right.
    const float actions = dialog.y + dialog.height - 52;
    if (!readOnly && entry != curves_.table.end())
        Button({dialog.x + 24, actions, 84, 32}, std::wstring(Texts().remove), [this] {
            curves_.table.erase(curves_.selected);
            std::erase_if(curves_.origins, [this](const auto &origin) { return origin.first == curves_.selected; });
            curves_.selected = "Linear";
            state_.RequestRebuild();
        });
    Button({dialog.x + dialog.width - 24 - 96 - 8 - 96, actions, 96, 32}, std::wstring(Texts().cancel), [this] { CloseCurves(); });
    Button({dialog.x + dialog.width - 24 - 96, actions, 96, 32}, std::wstring(texts_.Get("Editor.applyRow")),
           [this] { ApplyCurves(); }, true);
}

void EditorView::RenameCurve(const std::string &name)
{
    if (name == curves_.selected)
        return;
    if (curves_.table.contains(name) || std::ranges::find(BuiltIns, name) != BuiltIns.end())
        throw std::invalid_argument("A curve with this name already exists.");
    auto curve = curves_.table.extract(curves_.selected);
    curve.key() = name;
    curves_.table.insert(std::move(curve));
    // origins maps the current name to the name in the document, so references can follow.
    if (auto origin = curves_.origins.extract(curves_.selected))
    {
        origin.key() = name;
        curves_.origins.insert(std::move(origin));
    }
    curves_.selected = name;
    state_.RequestRebuild();
}
