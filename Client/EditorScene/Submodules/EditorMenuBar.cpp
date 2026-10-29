#include "EditorMenuBar.h"
#include "EditorSupport.h"
#include <algorithm>
#include <span>
#include <stdexcept>
#include <tuple>

namespace
{
    using namespace editor_ui;
    struct MenuItem
    {
        EditorMenuAction action;
        std::string_view label;
        std::wstring_view shortcut;
    };
    constexpr std::array FileItems{
        MenuItem{EditorMenuAction::Save, "Editor.menuSave", L"Ctrl+S"},
        MenuItem{EditorMenuAction::OpenYmmFolder, "Editor.openYmmFolder", L""},
        MenuItem{EditorMenuAction::OpenYmpFolder, "Editor.openYmpFolder", L""},
        MenuItem{EditorMenuAction::OpenYmeFolder, "Editor.openYmeFolder", L""}};
    constexpr std::array EditItems{
        MenuItem{EditorMenuAction::Undo, "Editor.menuUndo", L"Ctrl+Z"},
        MenuItem{EditorMenuAction::Redo, "Editor.menuRedo", L"Ctrl+Y"},
        MenuItem{EditorMenuAction::Copy, "Editor.menuCopy", L"Ctrl+C"},
        MenuItem{EditorMenuAction::Paste, "Editor.menuPaste", L"Ctrl+V"},
        MenuItem{EditorMenuAction::PasteWithBlanks, "Editor.menuPasteWithBlanks", L"Ctrl+Shift+V"}};

    std::span<const MenuItem> Items(bool edit)
    {
        return edit ? std::span<const MenuItem>{EditItems} : std::span<const MenuItem>{FileItems};
    }
    template <typename Component> Component &Require(Visual2DNode &node)
    {
        auto *component = node.GetComponent<Component>();
        if (!component)
            throw std::logic_error("Editor menu node is missing a component.");
        return *component;
    }
    void ConfigureText(Visual2DNode &node, TextCatalog &texts)
    {
        auto &label = Require<TextVisualComponent>(node);
        texts.ApplyFont(label);
        label.SetFontSize(18);
        label.SetTextColor(Ink);
    }
    void Tint(Visual2DNode &node, Color color)
    {
        Require<SpriteVisualComponent>(node).SetTint(color);
    }
}

void EditorMenuBar::Initialize(Visual2DCanvas &canvas, TextCatalog &texts)
{
    if (canvas_)
        throw std::logic_error("Editor menu is already initialized.");
    canvas_ = &canvas;
    texts_ = &texts;
    bar_ = &CreatePanel(canvas.Root(), {}, "Editor.MenuBar");
    bar_->SetZIndex(100);
    Tint(*bar_, editor_ui::Paper);
    file_ = &CreateLabel(*bar_, {8, 0, 92, Height}, L"", "Editor.Menu.File");
    edit_ = &CreateLabel(*bar_, {100, 0, 92, Height}, L"", "Editor.Menu.Edit");
    ConfigureText(*file_, texts);
    ConfigureText(*edit_, texts);
    popup_ = &CreatePanel(*bar_, {}, "Editor.Menu.Popup");
    popup_->SetZIndex(10);
    Tint(*popup_, editor_ui::Ink);
    for (std::size_t i = 0; i < rows_.size(); ++i)
    {
        auto &row = rows_[i];
        const auto suffix = std::to_string(i);
        row.node = &CreatePanel(*popup_, {}, "Editor.Menu.Item." + suffix);
        row.caption = &CreateLabel(*row.node, {}, L"", "Editor.Menu.Caption." + suffix);
        row.shortcut = &CreateLabel(*row.node, {}, L"", "Editor.Menu.Shortcut." + suffix);
        ConfigureText(*row.caption, texts);
        ConfigureText(*row.shortcut, texts);
        Require<TextVisualComponent>(*row.shortcut).SetHorizontalAlignment(TextAlignment::Trailing);
    }
    Resize();
    SetOpen(Menu::None, false);
}

void EditorMenuBar::Resize()
{
    if (!canvas_) return;
    const auto size = canvas_->LogicalSize();
    bar_->SetBounds({-size.width * .5F, size.height * .5F - Height, size.width, Height});
}

void EditorMenuBar::Refresh(bool dirty)
{
    if (!canvas_ || (labelsInitialized_ && dirty_ == dirty && textRevision_ == texts_->Revision()))
        return;
    const auto items = Items(open_ == Menu::Edit);
    const float width = open_ == Menu::Edit ? 430.0F : 320.0F;
    popup_->SetBounds({open_ == Menu::Edit ? 100.0F : 8.0F, -34.0F * static_cast<float>(items.size()), width, 34.0F * static_cast<float>(items.size())});
    Require<TextVisualComponent>(*file_).SetText(std::wstring(texts_->Get("Editor.fileMenu")));
    Require<TextVisualComponent>(*edit_).SetText(std::wstring(texts_->Get("Editor.editMenu")));
    ConfigureText(*file_, *texts_);
    ConfigureText(*edit_, *texts_);
    for (std::size_t i = 0; i < rows_.size(); ++i)
    {
        auto &row = rows_[i];
        row.node->SetVisible(i < items.size());
        if (i >= items.size()) continue;
        row.node->SetBounds({1, 1 + 34.0F * static_cast<float>(items.size() - 1 - i), width - 2, 32});
        row.caption->SetBounds({12, 4, width - 155, 24});
        row.shortcut->SetBounds({width - 137, 4, 122, 24});
        auto caption = std::wstring(texts_->Get(items[i].label));
        if (items[i].action == EditorMenuAction::Save && dirty) caption += L" *";
        Require<TextVisualComponent>(*row.caption).SetText(std::move(caption));
        Require<TextVisualComponent>(*row.shortcut).SetText(std::wstring(items[i].shortcut));
        ConfigureText(*row.caption, *texts_);
        ConfigureText(*row.shortcut, *texts_);
    }
    dirty_ = dirty;
    textRevision_ = texts_->Revision();
    labelsInitialized_ = true;
    RefreshHighlight();
}

bool EditorMenuBar::UpdateKeyboard(const EditorMenuInput &input, EditorMenuInputResult &result)
{
    if (input.shortcutAction)
    {
        result.action = input.shortcutAction;
        result.consumeKeyboard = true;
        result.consumePointer = open_ != Menu::None;
        SetOpen(Menu::None, false);
        return true;
    }
    if (input.toggle || input.toggleEdit)
    {
        const auto target = input.toggleEdit ? Menu::Edit : Menu::File;
        SetOpen(open_ == target ? Menu::None : target, true);
        result.consumeKeyboard = result.consumePointer = true;
        return true;
    }
    if (open_ == Menu::None) return false;
    result.consumeKeyboard = result.consumePointer = true;
    if (input.dismiss)
        SetOpen(Menu::None, false);
    else if (input.switchStep != 0)
        SetOpen(open_ == Menu::File ? Menu::Edit : Menu::File, true);
    else if (input.selectStep != 0)
    {
        const auto count = static_cast<int>(Items(open_ == Menu::Edit).size());
        selected_ = (std::max(0, selected_) + input.selectStep + count) % count;
    }
    else if (input.accept)
    {
        result.action = Items(open_ == Menu::Edit)[static_cast<std::size_t>(std::max(0, selected_))].action;
        SetOpen(Menu::None, false);
    }
    else
        return false;
    return true;
}

void EditorMenuBar::UpdatePointer(const EditorMenuInput &input, EditorMenuInputResult &result)
{
    const auto &point = input.pointer;
    hoverFile_ = point && file_->BoundsInCanvas().Contains(*point);
    hoverEdit_ = point && edit_->BoundsInCanvas().Contains(*point);
    hovered_ = -1;
    if (open_ != Menu::None && point)
        for (std::size_t i = 0; i < Items(open_ == Menu::Edit).size(); ++i)
            if (rows_[i].node->BoundsInCanvas().Contains(*point))
                hovered_ = static_cast<int>(i);
    result.consumePointer = (point && bar_->BoundsInCanvas().Contains(*point)) || open_ != Menu::None;
    if (!input.leftPressed && !input.rightPressed) return;
    if (input.leftPressed && (hoverFile_ || hoverEdit_))
    {
        const auto target = hoverEdit_ ? Menu::Edit : Menu::File;
        SetOpen(open_ == target ? Menu::None : target, false);
        result.consumeKeyboard = result.consumePointer = true;
    }
    else if (open_ != Menu::None)
    {
        // A dismissal owns this click; it cannot edit the surface underneath.
        result.consumeKeyboard = result.consumePointer = true;
        if (input.leftPressed && hovered_ >= 0)
            result.action = Items(open_ == Menu::Edit)[static_cast<std::size_t>(hovered_)].action;
        SetOpen(Menu::None, false);
    }
}

EditorMenuInputResult EditorMenuBar::Update(const EditorMenuInput &input, bool dirty)
{
    EditorMenuInputResult result;
    if (!canvas_) return result;
    Refresh(dirty);
    const auto before = std::tuple{open_, selected_, hovered_, hoverFile_, hoverEdit_};
    if (!UpdateKeyboard(input, result)) UpdatePointer(input, result);
    if (before != std::tuple{open_, selected_, hovered_, hoverFile_, hoverEdit_}) RefreshHighlight();
    return result;
}

void EditorMenuBar::SetOpen(Menu menu, bool keyboardSelection)
{
    open_ = menu;
    selected_ = keyboardSelection ? 0 : -1;
    hovered_ = -1;
    popup_->SetVisible(menu != Menu::None);
    labelsInitialized_ = false;
    Refresh(dirty_);
}

void EditorMenuBar::RefreshHighlight()
{
    const auto highlightTab = [](Visual2DNode &node, bool active, bool hovered) {
        Tint(node, active ? editor_ui::Blue : hovered ? editor_ui::Pale : editor_ui::Paper);
        Require<TextVisualComponent>(node).SetTextColor(active ? editor_ui::White : editor_ui::Ink);
    };
    highlightTab(*file_, open_ == Menu::File, hoverFile_);
    highlightTab(*edit_, open_ == Menu::Edit, hoverEdit_);
    for (std::size_t i = 0; i < rows_.size(); ++i)
    {
        const bool selected = static_cast<int>(i) == (hovered_ >= 0 ? hovered_ : selected_);
        auto &row = rows_[i];
        Tint(*row.node, selected ? editor_ui::Blue : editor_ui::Paper);
        Require<TextVisualComponent>(*row.caption).SetTextColor(selected ? editor_ui::White : editor_ui::Ink);
        Require<TextVisualComponent>(*row.shortcut).SetTextColor(selected ? editor_ui::White : editor_ui::Ink);
    }
}

void EditorMenuBar::Shutdown() noexcept
{
    if (bar_) bar_->SetVisible(false);
    canvas_ = nullptr;
    texts_ = nullptr;
    bar_ = file_ = edit_ = popup_ = nullptr;
    rows_ = {};
    open_ = Menu::None;
    selected_ = hovered_ = -1;
    hoverFile_ = hoverEdit_ = labelsInitialized_ = false;
}
