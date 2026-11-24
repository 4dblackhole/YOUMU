#pragma once
#include "PhantomCore.h"
#include "Texts/TextCatalog.h"
#include <functional>

// The editor's native number inputs use the registered Barlow face; text inputs stay Segoe UI.
void TestEditorTextFieldFonts();
void TestEditorModes(PhantomCore::ScreenVisual2DManager &, const PhantomCore::EngineServices &,
                     TextCatalog &,
                     const std::function<const std::vector<PhantomCore::DrawPacket> &()> &);
