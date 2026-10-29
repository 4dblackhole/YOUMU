#pragma once
#include "PhantomCore.h"
#include "Texts/TextCatalog.h"
#include <array>
#include <cstdint>
#include <optional>

enum class EditorMenuAction
{
    Save, OpenYmmFolder, OpenYmpFolder, OpenYmeFolder,
    Undo, Redo, Copy, Paste, PasteWithBlanks
};

struct EditorMenuInput
{
    std::optional<Point> pointer;
    bool leftPressed{}, rightPressed{};
    bool toggle{}, toggleEdit{}, accept{}, dismiss{};
    int selectStep{}, switchStep{};
    std::optional<EditorMenuAction> shortcutAction;
};

struct EditorMenuInputResult
{
    std::optional<EditorMenuAction> action;
    bool consumeKeyboard{}, consumePointer{};
};

// Owns menu state and Canvas observers; the editor executes returned actions.
class EditorMenuBar final
{
  public:
    static constexpr float Height = 28.0F;
    void Initialize(Visual2DCanvas &, TextCatalog &);
    void Resize();
    void Refresh(bool dirty);
    EditorMenuInputResult Update(const EditorMenuInput &, bool dirty);
    void Shutdown() noexcept;

  private:
    enum class Menu { None, File, Edit };
    struct Row
    {
        Visual2DNode *node{}, *caption{}, *shortcut{};
    };
    bool UpdateKeyboard(const EditorMenuInput &, EditorMenuInputResult &);
    void UpdatePointer(const EditorMenuInput &, EditorMenuInputResult &);
    void SetOpen(Menu, bool keyboardSelection);
    void RefreshHighlight();

    Visual2DCanvas *canvas_{};
    TextCatalog *texts_{};
    Visual2DNode *bar_{}, *file_{}, *edit_{}, *popup_{};
    std::array<Row, 5> rows_{};
    Menu open_{Menu::None};
    int selected_{-1}, hovered_{-1};
    bool hoverFile_{}, hoverEdit_{}, labelsInitialized_{}, dirty_{};
    std::uint64_t textRevision_{};
};
