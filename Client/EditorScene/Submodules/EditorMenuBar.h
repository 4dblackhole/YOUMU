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
    std::optional<PhantomCore::Point> pointer;
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

// The document the menu bar names: representative title, artist and [difficulty].
struct EditorMenuDocument
{
    std::wstring title, artist, pattern, mode;
    // False for a new difficulty that Create has not written yet.
    bool created{true};
};

// Owns the navy menu bar, its menus and Canvas observers; the editor executes returned actions.
class EditorMenuBar final
{
  public:
    static constexpr float Height = 36.0F;
    // The bar spans the top of the editor frame under `parent` (the frame's centred node).
    void Initialize(PhantomCore::Visual2DNode &parent, TextCatalog &);
    void Resize(float frameWidth);
    void SetDocument(EditorMenuDocument);
    void Refresh(bool dirty);
    EditorMenuInputResult Update(const EditorMenuInput &, bool dirty);
    void Shutdown() noexcept;

  private:
    enum class Menu { None, File, Edit };
    struct Row
    {
        PhantomCore::Visual2DNode *node{}, *caption{}, *shortcut{};
    };
    bool UpdateKeyboard(const EditorMenuInput &, EditorMenuInputResult &);
    void UpdatePointer(const EditorMenuInput &, EditorMenuInputResult &);
    void SetOpen(Menu, bool keyboardSelection);
    void RefreshHighlight();
    void LayoutDocument();

    PhantomCore::Visual2DNode *parent_{};
    TextCatalog *texts_{};
    PhantomCore::Visual2DNode *bar_{}, *file_{}, *edit_{}, *popup_{};
    PhantomCore::Visual2DNode *title_{}, *artist_{}, *pattern_{}, *dirtyDot_{}, *modeBadge_{}, *modeLabel_{};
    PhantomCore::Visual2DNode *newBadge_{}, *newLabel_{};
    std::array<Row, 5> rows_{};
    EditorMenuDocument document_;
    float width_{1280};
    Menu open_{Menu::None};
    int selected_{-1}, hovered_{-1};
    bool hoverFile_{}, hoverEdit_{}, labelsInitialized_{}, dirty_{};
    std::uint64_t textRevision_{};
};
