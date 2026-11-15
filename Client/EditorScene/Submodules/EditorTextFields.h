#pragma once
#include "PhantomCore.h"
#include <functional>
#include <map>
#include <memory>
#include <string>

// Inline single-line editors for the chart editor's fields. The view declares each
// visible field while it builds; one native Windows editor per field is placed over
// it, and fields not declared in a build are hidden. Enter or leaving a field
// commits its text; Escape restores the document's value. Without a window
// (offscreen tests) no editor exists and fields are drawn only.
class EditorTextFields final
{
  public:
    using Commit = std::function<void(const std::string &)>;
    void Initialize(PhantomCore::IWindowControl *window) noexcept { window_ = window; }
    void Begin() noexcept;
    // `screen` is in window pixels; `text` is the document's current value.
    void Declare(const std::string &key, PhantomCore::TextInputBounds screen, std::uint32_t fontPixels,
                 const std::wstring &text, Commit commit, bool number);
    void End() noexcept;
    // Polls the editors; a commit that throws leaves the field's text for correction.
    // Returns the error of a rejected commit, if any.
    [[nodiscard]] std::optional<std::string> Update();
    [[nodiscard]] bool Editing() const noexcept;
    // A click outside every field ends editing (and commits).
    void BlurAll() noexcept;
    void Shutdown() noexcept;
    [[nodiscard]] bool Available() const noexcept { return window_ != nullptr; }

  private:
    struct Field
    {
        std::unique_ptr<PhantomCore::ITextInput> input;
        std::wstring shown, typed;
        Commit commit;
        bool declared{}, focused{}, edited{};
    };
    std::optional<std::string> CommitField(Field &);
    PhantomCore::IWindowControl *window_{};
    std::map<std::string, Field, std::less<>> fields_;
};
