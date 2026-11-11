#pragma once
#include "PhantomCore.h"
#include "Texts/TextCatalog.h"
#include <functional>

void TestEditorModes(PhantomCore::ScreenVisual2DManager &, const PhantomCore::EngineServices &,
                     TextCatalog &,
                     const std::function<const std::vector<PhantomCore::DrawPacket> &()> &);
