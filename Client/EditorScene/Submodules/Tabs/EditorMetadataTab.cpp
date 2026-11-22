#include "EditorMetadataTab.h"

void EditorMetadataTab::Build()
{
    state_.Mode().DrawMetadata(view_, state_, texts_);
}
