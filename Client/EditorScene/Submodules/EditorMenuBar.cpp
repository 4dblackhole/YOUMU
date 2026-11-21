#include "EditorMenuBar.h"
#include "EditorSupport.h"
#include "Presentation/UiFonts.h"
#include <algorithm>
#include <span>
#include <stdexcept>
#include <tuple>

using namespace PhantomCore;

namespace
{
    namespace theme = ui_colors::theme;
    using ui_colors::White;
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
        MenuItem{EditorMenuAction::PasteWithBlanks, "Editor.menuPasteWithBlanks", L"Ctrl+Shift+V"},
        MenuItem{EditorMenuAction::FindReplace, "Editor.menuFindReplace", L"Ctrl+F"}};
    static_assert(FileItems.size() <= 6 && EditItems.size() <= 6, "EditorMenuBar::rows_ holds one node per item.");
    constexpr float MenuWidth = 248, ItemHeight = 34, FileWidth = 56, EditWidth = 58, DocumentLeft = 130;

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
    void SetText(Visual2DNode &node, std::wstring value, float size, std::uint16_t weight, Color color,
                 bool number = false)
    {
        auto &label = Require<TextVisualComponent>(node);
        if (number)
        {
            auto face = ui_fonts::Number(weight);
            label.SetFont(std::move(face.font));
            label.SetFontWeight(face.weight);
        }
        else
        {
            label.SetFont(ui_fonts::Text(value));
            label.SetFontWeight(weight);
        }
        label.SetFontSize(size);
        label.SetTextColor(color);
        label.SetText(std::move(value));
    }
    void Tint(Visual2DNode &node, Color color)
    {
        Require<SpriteVisualComponent>(node).SetTint(color);
    }
}

void EditorMenuBar::Initialize(Visual2DNode &parent, TextCatalog &texts)
{
    if (parent_)
        throw std::logic_error("Editor menu is already initialized.");
    parent_ = &parent;
    texts_ = &texts;
    bar_ = &CreatePanel(parent, {}, "Editor.MenuBar");
    bar_->SetZIndex(100);
    Tint(*bar_, theme::BrandDeep);
    file_ = &CreateLabel(*bar_, {0, 0, FileWidth, Height}, L"", "Editor.Menu.File");
    edit_ = &CreateLabel(*bar_, {FileWidth, 0, EditWidth, Height}, L"", "Editor.Menu.Edit");
    for (auto *menu : {file_, edit_})
        Require<TextVisualComponent>(*menu).SetHorizontalAlignment(TextAlignment::Center);
    title_ = &CreateLabel(*bar_, {}, L"", "Editor.Document.Title");
    artist_ = &CreateLabel(*bar_, {}, L"", "Editor.Document.Artist");
    pattern_ = &CreateLabel(*bar_, {}, L"", "Editor.Document.Pattern");
    dirtyDot_ = &CreatePanel(*bar_, {}, "Editor.Document.Unsaved");
    Tint(*dirtyDot_, theme::Sun);
    Require<SpriteVisualComponent>(*dirtyDot_).SetCornerRadius(4);
    newBadge_ = &CreatePanel(*bar_, {}, "Editor.Document.New");
    Tint(*newBadge_, theme::Sun);
    newLabel_ = &CreateLabel(*newBadge_, {}, L"", "Editor.Document.NewLabel");
    Require<TextVisualComponent>(*newLabel_).SetHorizontalAlignment(TextAlignment::Center);
    modeBadge_ = &CreatePanel(*bar_, {}, "Editor.Document.Mode");
    Tint(*modeBadge_, {1, 1, 1, .12F});
    modeLabel_ = &CreateLabel(*modeBadge_, {}, L"", "Editor.Document.ModeLabel");
    Require<TextVisualComponent>(*modeLabel_).SetHorizontalAlignment(TextAlignment::Center);
    popup_ = &CreatePanel(*bar_, {}, "Editor.Menu.Popup");
    popup_->SetZIndex(10);
    Tint(*popup_, theme::RuleStrong);
    for (std::size_t i = 0; i < rows_.size(); ++i)
    {
        auto &row = rows_[i];
        const auto suffix = std::to_string(i);
        row.node = &CreatePanel(*popup_, {}, "Editor.Menu.Item." + suffix);
        row.caption = &CreateLabel(*row.node, {}, L"", "Editor.Menu.Caption." + suffix);
        row.shortcut = &CreateLabel(*row.node, {}, L"", "Editor.Menu.Shortcut." + suffix);
        Require<TextVisualComponent>(*row.shortcut).SetHorizontalAlignment(TextAlignment::Trailing);
    }
    Resize(width_);
    SetOpen(Menu::None, false);
}

void EditorMenuBar::Resize(float frameWidth)
{
    if (!parent_)
        return;
    width_ = frameWidth;
    bar_->SetBounds({-frameWidth * .5F, 360.0F - Height, frameWidth, Height});
    LayoutDocument();
}

void EditorMenuBar::SetDocument(EditorMenuDocument document)
{
    const bool changed = std::tie(document.title, document.artist, document.pattern, document.mode, document.created) !=
                         std::tie(document_.title, document_.artist, document_.pattern, document_.mode, document_.created);
    if (!changed)
        return;
    document_ = std::move(document);
    labelsInitialized_ = false;
}

void EditorMenuBar::LayoutDocument()
{
    if (!parent_)
        return;
    using editor_ui::EstimateTextWidth;
    // Title · artist [difficulty] and the unsaved dot read left to right after the menus.
    const float right = width_ - 16 - 93 - 16;
    float x = DocumentLeft;
    const auto place = [&](Visual2DNode &node, const std::wstring &text, float size) {
        const float w = std::min(EstimateTextWidth(text, size) + 4, std::max(0.0F, right - x));
        node.SetBounds({x, (Height - size * 1.36F) * .5F, w, size * 1.36F});
        x += w + 8;
    };
    place(*title_, document_.title, 15);
    const auto artist = document_.artist.empty() ? std::wstring{} : L"· " + document_.artist;
    place(*artist_, artist, 14);
    const auto pattern = document_.pattern.empty() ? std::wstring{} : L"[" + document_.pattern + L"]";
    place(*pattern_, pattern, 14);
    newBadge_->SetVisible(!document_.created);
    if (!document_.created)
    {
        newBadge_->SetBounds({x, (Height - 18) * .5F, 40, 18});
        newLabel_->SetBounds({0, 1, 40, 16});
        x += 48;
    }
    dirtyDot_->SetBounds({x, Height * .5F - 4, 8, 8});
    modeBadge_->SetBounds({width_ - 16 - 93, 6, 93, 24});
    modeLabel_->SetBounds({0, 3, 93, 18});
}

void EditorMenuBar::Refresh(bool dirty)
{
    if (!parent_)
        return;
    if (labelsInitialized_ && dirty_ == dirty && textRevision_ == texts_->Revision())
        return;
    const auto items = Items(open_ == Menu::Edit);
    popup_->SetBounds({open_ == Menu::Edit ? FileWidth : 0.0F, -ItemHeight * static_cast<float>(items.size()) - 2,
                       MenuWidth, ItemHeight * static_cast<float>(items.size()) + 2});
    SetText(*file_, std::wstring(texts_->Get("Editor.fileMenu")), 14, 700, {1, 1, 1, .82F});
    SetText(*edit_, std::wstring(texts_->Get("Editor.editMenu")), 14, 700, {1, 1, 1, .82F});
    SetText(*title_, document_.title, 15, 800, White);
    SetText(*artist_, document_.artist.empty() ? std::wstring{} : L"· " + document_.artist, 14, 600, {1, 1, 1, .72F});
    SetText(*pattern_, document_.pattern.empty() ? std::wstring{} : L"[" + document_.pattern + L"]", 14, 800,
            theme::Sun);
    SetText(*modeLabel_, document_.mode, 12, 800, White, true);
    SetText(*newLabel_, std::wstring(texts_->Get("Editor.newBadge")), 12, 800, theme::BrandDeep, true);
    dirtyDot_->SetVisible(dirty);
    LayoutDocument();
    for (std::size_t i = 0; i < rows_.size(); ++i)
    {
        auto &row = rows_[i];
        row.node->SetVisible(i < items.size());
        if (i >= items.size())
            continue;
        row.node->SetBounds({1, 1 + ItemHeight * static_cast<float>(items.size() - 1 - i), MenuWidth - 2, ItemHeight});
        row.caption->SetBounds({14, 7, MenuWidth - 120, 20});
        row.shortcut->SetBounds({MenuWidth - 112, 8, 96, 18});
        SetText(*row.caption, std::wstring(texts_->Get(items[i].label)), 14, 600, theme::Ink);
        SetText(*row.shortcut, std::wstring(items[i].shortcut), 12, 600, theme::Caption, true);
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
    if (open_ == Menu::None)
        return false;
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
    if (!input.leftPressed && !input.rightPressed)
        return;
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
    if (!parent_)
        return result;
    Refresh(dirty);
    const auto before = std::tuple{open_, selected_, hovered_, hoverFile_, hoverEdit_};
    if (!UpdateKeyboard(input, result))
        UpdatePointer(input, result);
    if (before != std::tuple{open_, selected_, hovered_, hoverFile_, hoverEdit_})
        RefreshHighlight();
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
    const auto highlightMenu = [](Visual2DNode &node, bool active, bool hovered) {
        Tint(node, active ? Color{1, 1, 1, .14F} : hovered ? Color{1, 1, 1, .1F} : Color{1, 1, 1, 0});
        Require<TextVisualComponent>(node).SetTextColor(active || hovered ? White : Color{1, 1, 1, .82F});
    };
    highlightMenu(*file_, open_ == Menu::File, hoverFile_);
    highlightMenu(*edit_, open_ == Menu::Edit, hoverEdit_);
    for (std::size_t i = 0; i < rows_.size(); ++i)
    {
        const bool selected = static_cast<int>(i) == (hovered_ >= 0 ? hovered_ : selected_);
        Tint(*rows_[i].node, selected ? ui_colors::Rgb(0xE6EEFC) : White);
    }
}

void EditorMenuBar::Shutdown() noexcept
{
    if (bar_)
        bar_->SetVisible(false);
    parent_ = nullptr;
    texts_ = nullptr;
    bar_ = file_ = edit_ = popup_ = nullptr;
    title_ = artist_ = pattern_ = dirtyDot_ = modeBadge_ = modeLabel_ = newBadge_ = newLabel_ = nullptr;
    rows_ = {};
    open_ = Menu::None;
    selected_ = hovered_ = -1;
    hoverFile_ = hoverEdit_ = labelsInitialized_ = false;
}
