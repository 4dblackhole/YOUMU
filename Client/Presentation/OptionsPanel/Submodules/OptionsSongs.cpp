#include "OptionsPanelStyle.h"
#include "Presentation/OptionsPanel.h"
#include <ShObjIdl.h>
#include <memory>
#include <wrl/client.h>

using namespace PhantomCore;

using namespace options_ui;

void OptionsPanel::CreateSongsControls()
{
    // Path box and Browse on one row; the default-folder text button below them.
    constexpr float PathWidth = 213, BrowseWidth = 87, ButtonHeight = 32;
    const float top = NextField(ControlHeight + 6 + ButtonHeight);
    const float controlTop = top + ControlOffset;
    songsLabel_ = &CreateFieldLabel(*content_, top, "Options.Songs.Label");
    songsField_ = &CreateButton(*content_, TopLeft({ContentLeft, controlTop, PathWidth, ControlHeight}, contentHeight_),
                                L"", "Options.Songs.Folder");
    StyleFieldButton(*songsField_, false);
    AddFieldLine(*content_, *songsField_, "Options.Songs.Line");
    songsBrowse_ = &CreateButton(*content_,
                                 TopLeft({ContentLeft + FieldWidth - BrowseWidth, controlTop + 4, BrowseWidth, ButtonHeight},
                                         contentHeight_),
                                 L"", "Options.Songs.Browse");
    SetColors(*songsBrowse_, ui_colors::Rgb(0xFFFFFF, .72F), FieldHoverColor, FieldPressedColor, FieldDisabledColor);
    StyleCaption(RequireComponent<TextVisualComponent>(*songsBrowse_), 13, 800, ui_colors::theme::BrandText,
                 TextAlignment::Center);
    AddFieldLine(*content_, *songsBrowse_, "Options.Songs.BrowseLine");
    songsDefault_ = &CreateButton(
        *content_, TopLeft({ContentLeft, controlTop + ControlHeight + 6, 160, ButtonHeight}, contentHeight_), L"",
        "Options.Songs.Default");
    SetColors(*songsDefault_, {1, 1, 1, 0}, ui_colors::Rgb(0x1F4C99, .06F), ui_colors::Rgb(0x1F4C99, .12F),
              {1, 1, 1, 0});
    auto &defaultText = RequireComponent<TextVisualComponent>(*songsDefault_);
    StyleCaption(defaultText, 13, 800, ui_colors::theme::BrandText);
    defaultText.SetContentBounds({12, 0, 160 - 24, ButtonHeight});
}

void OptionsPanel::RefreshSongsControls()
{
    const std::array values{std::pair{songsLabel_, std::wstring(texts_.Get("Options.songsFolder"))},
                            std::pair{songsField_, options_.SongsRoot().wstring()},
                            std::pair{songsBrowse_, std::wstring(texts_.Get("Options.browse"))},
                            std::pair{songsDefault_, std::wstring(texts_.Get("Options.defaultSongsFolder"))}};
    for (const auto &[node, value] : values)
        SetCaption(*node, value);
}

void OptionsPanel::ChooseSongsFolder()
{
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    struct ComScope
    {
        HRESULT result;
        ~ComScope()
        {
            if (SUCCEEDED(result))
                CoUninitialize();
        }
    } scope{initialized};
    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
    HRESULT result =
        CoCreateInstance(__uuidof(FileOpenDialog), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
    if (SUCCEEDED(result))
        result = dialog->SetOptions(FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    if (SUCCEEDED(result))
        result = dialog->SetTitle(std::wstring(texts_.Get("Options.songsFolder")).c_str());
    if (SUCCEEDED(result))
        result = dialog->Show(GetActiveWindow());
    if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED))
        return;
    Microsoft::WRL::ComPtr<IShellItem> item;
    if (SUCCEEDED(result))
        result = dialog->GetResult(&item);
    PWSTR raw{};
    if (SUCCEEDED(result))
        result = item->GetDisplayName(SIGDN_FILESYSPATH, &raw);
    const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> path(raw, &CoTaskMemFree);
    if (FAILED(result) || !path)
    {
        status_ = Status::InvalidInput;
        statusDetail_ = "Unable to select the Songs directory.";
    }
    else
    {
        auto candidate = options_.Values();
        candidate.songsPath = path.get();
        SaveOptions(candidate);
    }
    RefreshSongsControls();
    RefreshStatus();
}
