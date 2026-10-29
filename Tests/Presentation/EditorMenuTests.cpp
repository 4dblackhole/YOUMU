#include "EditorMenuTests.h"
#include "EditorScene/Submodules/EditorMenuBar.h"
#include <array>
#include <stdexcept>

namespace
{
    void Check(bool value, const char *message)
    {
        if (!value) throw std::runtime_error(message);
    }
    Visual2DNode *Find(Visual2DNode &node, std::string_view name)
    {
        if (node.Name() == name) return &node;
        for (const auto &child : node.Children())
            if (auto *found = Find(*child, name)) return found;
        return nullptr;
    }
}

void TestEditorMenu()
{
    Visual2DCanvas canvas({1920, 1080});
    TextCatalog texts(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Client/Texts/Languages");
    EditorMenuBar menu;
    menu.Initialize(canvas, texts);
    auto *bar = Find(canvas.Root(), "Editor.MenuBar");
    auto *file = Find(canvas.Root(), "Editor.Menu.File");
    auto *edit = Find(canvas.Root(), "Editor.Menu.Edit");
    auto *popup = Find(canvas.Root(), "Editor.Menu.Popup");
    auto *caption = Find(canvas.Root(), "Editor.Menu.Caption.0");
    if (!bar || !file || !edit || !popup || !caption) throw std::runtime_error("Editor menu nodes are missing.");
    Check(!popup->IsVisible(), "Menus must start closed.");
    Check(file->GetComponent<TextVisualComponent>()->Text() == L"파일" &&
          edit->GetComponent<TextVisualComponent>()->Text() == L"편집" &&
          caption->GetComponent<TextVisualComponent>()->Text() == L"저장하기",
          "Korean menu labels are missing.");
    menu.Refresh(true);
    Check(caption->GetComponent<TextVisualComponent>()->Text() == L"저장하기 *",
          "Dirty state must appear in Save.");
    texts.SetLanguage(Language::English);
    menu.Refresh(false);
    Check(file->GetComponent<TextVisualComponent>()->Text() == L"File" &&
          edit->GetComponent<TextVisualComponent>()->Text() == L"Edit" &&
          caption->GetComponent<TextVisualComponent>()->Text() == L"Save",
          "Menu must refresh language and dirty state.");

    const std::array fileActions{EditorMenuAction::Save, EditorMenuAction::OpenYmmFolder,
                                EditorMenuAction::OpenYmpFolder, EditorMenuAction::OpenYmeFolder};
    const std::array editActions{EditorMenuAction::Undo, EditorMenuAction::Redo, EditorMenuAction::Copy,
                                EditorMenuAction::Paste, EditorMenuAction::PasteWithBlanks};
    for (const Size viewport : {Size{1920, 1080}, {1280, 720}, {900, 720}, {2560, 1080}})
    {
        canvas.SetViewportSize(viewport);
        menu.Resize();
        const auto bounds = bar->BoundsInCanvas();
        Check(bounds.width == canvas.LogicalSize().width && bounds.y + bounds.height == 540,
              "Menu must stay at the viewport's top edge.");
        const auto pointer = [&](float x, float y) {
            return MapScreenPointer({x * canvas.PixelScale(), y * canvas.PixelScale()}, viewport, canvas);
        };
        const auto verifyMenu = [&](auto actions, float x) {
            for (std::size_t i = 0; i < actions.size(); ++i)
            {
                const auto opened = menu.Update({.pointer = pointer(x, 10), .leftPressed = true}, false);
                Check(opened.consumePointer && opened.consumeKeyboard && !opened.action && popup->IsVisible(),
                      "A menu must open without editing beneath it.");
                const auto idle = menu.Update({}, false);
                Check(idle.consumeKeyboard && idle.consumePointer && !idle.action,
                      "Open menus must own input between events.");
                const auto clicked = menu.Update(
                    {.pointer = pointer(x + 10, 45 + 34 * static_cast<float>(i)), .leftPressed = true}, false);
                Check(clicked.action == actions[i] && clicked.consumePointer && !popup->IsVisible(),
                      "Each row must emit its own action and close.");
                Check(!menu.Update({}, false).action, "A menu action must not repeat.");
            }
        };
        verifyMenu(fileActions, 20);
        verifyMenu(editActions, 112);
    }

    static_cast<void>(menu.Update({.toggle = true}, false));
    const auto dismissed = menu.Update({.dismiss = true}, false);
    Check(!dismissed.action && dismissed.consumeKeyboard && dismissed.consumePointer && !popup->IsVisible(),
          "Escape must dismiss without leaving the editor.");
    static_cast<void>(menu.Update({.toggle = true}, false));
    const auto accepted = menu.Update({.accept = true}, false);
    Check(accepted.action == EditorMenuAction::Save && accepted.consumeKeyboard && !popup->IsVisible(),
          "Enter must invoke the selected row.");
    Check(!menu.Update({.accept = true}, false).action, "A closed menu must not process Enter.");

    static_cast<void>(menu.Update({.toggle = true}, false));
    static_cast<void>(menu.Update({.selectStep = -1}, false));
    Check(menu.Update({.accept = true}, false).action == EditorMenuAction::OpenYmeFolder,
          "Up must wrap to the last file action.");
    static_cast<void>(menu.Update({.toggle = true}, false));
    static_cast<void>(menu.Update({.selectStep = 1}, false));
    Check(menu.Update({.accept = true}, false).action == EditorMenuAction::OpenYmmFolder,
          "Down must advance the file selection.");
    static_cast<void>(menu.Update({.toggle = true}, false));
    static_cast<void>(menu.Update({.switchStep = 1}, false));
    Check(menu.Update({.accept = true}, false).action == EditorMenuAction::Undo,
          "Left/right must switch to the Edit menu.");
    static_cast<void>(menu.Update({.toggleEdit = true}, false));
    Check(menu.Update({.shortcutAction = EditorMenuAction::Save}, false).action == EditorMenuAction::Save &&
          !popup->IsVisible(), "A shortcut must invoke its own action while another menu is open.");
    for (auto action : editActions)
        Check(menu.Update({.shortcutAction = action}, false).action == action,
              "Closed-menu shortcuts must share the action dispatch.");

    static_cast<void>(menu.Update({.toggle = true}, false));
    const auto outside = menu.Update({.pointer = Point{600, -200}, .leftPressed = true}, false);
    Check(!outside.action && outside.consumePointer && outside.consumeKeyboard && !popup->IsVisible(),
          "Outside dismissal must not click through.");
    const auto resumed = menu.Update({.pointer = Point{600, -200}, .leftPressed = true}, false);
    Check(!resumed.consumePointer && !resumed.consumeKeyboard, "Closed menus must release input.");
    static_cast<void>(menu.Update({.toggleEdit = true}, false));
    const auto right = menu.Update({.pointer = Point{600, -200}, .rightPressed = true}, false);
    Check(right.consumePointer && !right.action && !popup->IsVisible(),
          "Right-click dismissal must not erase a note.");
    menu.Shutdown();
    Check(!bar->IsVisible() && !menu.Update({.toggle = true}, false).consumeKeyboard,
          "Shutdown must hide menus and release Canvas observers.");
}
