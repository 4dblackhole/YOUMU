#pragma once
#include "EditorTabBase.h"

// Metadata: the mode draws the chart's properties over the body (IEditorMode::DrawMetadata).
// A new difficulty opens here and keeps the other tabs locked until it is created.
class EditorMetadataTab final : public EditorTabBase
{
  public:
    EditorMetadataTab(EditorView &view, EditorWorkspace &state, TextCatalog &texts) noexcept
        : EditorTabBase(view, state, texts)
    {
    }
    [[nodiscard]] std::string_view CaptionKey() const noexcept override { return "Editor.tabs.2"; }
    void Build() override;
};
