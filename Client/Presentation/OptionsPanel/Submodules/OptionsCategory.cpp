#include "OptionsCategory.h"
#include "OptionsPanelStyle.h"
#include <utility>

using namespace PhantomCore;

using namespace options_ui;

OptionsPointer OptionsCategory::ProcessPointer(const InputState &, std::optional<Point>, NodeId)
{
    return OptionsPointer::Free;
}

void OptionsCategory::HandleKey(std::uint16_t) {}

void OptionsCategory::AppendCombos(std::vector<Visual2DNode *> &) const {}

void OptionsCategory::UpdateModal(NodeId) {}

Visual2DNode &OptionsCategory::CreateComboField(const std::string &name, Visual2DNode *&label,
                                                std::vector<std::wstring> items)
{
    auto &content = context_.Content();
    const float top = context_.NextField(ControlHeight);
    label = &CreateFieldLabel(content, top, "Options." + name + ".Label");
    auto &combo = CreateDropdown(
        content, TopLeft({ContentLeft, top + ControlOffset, FieldWidth, ControlHeight}, context_.ContentHeight()),
        std::move(items), "Options." + name + ".Combo");
    ConfigureCombo(combo);
    AddFieldLine(content, combo, "Options." + name + ".Line");
    return combo;
}
