#include "OptionsGeneral.h"
#include "OptionsPanelStyle.h"
#include <ShObjIdl.h>
#include <array>
#include <memory>
#include <wrl/client.h>

using namespace PhantomCore;

using namespace options_ui;
namespace
{
    // Songs: path box and Browse on one row; the default-folder text button below them.
    constexpr float PathWidth = 213, BrowseWidth = 87, ButtonHeight = 32;
    constexpr float SongsControlHeight = ControlHeight + 6 + ButtonHeight;
} // namespace

std::vector<float> OptionsGeneral::FieldHeights() const
{
    return {FieldHeight(ControlHeight), FieldHeight(SongsControlHeight)};
}

void OptionsGeneral::Create()
{
    CreateLanguageControls();
    CreateSongsControls();
}

void OptionsGeneral::CreateLanguageControls()
{
    std::vector<std::wstring> languageNames;
    for (const LanguageProfile &profile : context_.Texts().Languages())
        languageNames.emplace_back(profile.displayName);
    languageCombo_ = &CreateComboField("Language", languageLabel_, std::move(languageNames));
}

void OptionsGeneral::CreateSongsControls()
{
    auto &content = context_.Content();
    const float contentHeight = context_.ContentHeight();
    const float top = context_.NextField(SongsControlHeight);
    const float controlTop = top + ControlOffset;
    songsLabel_ = &CreateFieldLabel(content, top, "Options.Songs.Label");
    songsField_ = &CreateButton(content, TopLeft({ContentLeft, controlTop, PathWidth, ControlHeight}, contentHeight), L"",
                                "Options.Songs.Folder");
    StyleFieldButton(*songsField_, false);
    AddFieldLine(content, *songsField_, "Options.Songs.Line");
    songsBrowse_ = &CreateButton(
        content, TopLeft({ContentLeft + FieldWidth - BrowseWidth, controlTop + 4, BrowseWidth, ButtonHeight}, contentHeight),
        L"", "Options.Songs.Browse");
    SetColors(*songsBrowse_, ui_colors::Rgb(0xFFFFFF, .72F), FieldHoverColor, FieldPressedColor, FieldDisabledColor);
    StyleCaption(RequireComponent<TextVisualComponent>(*songsBrowse_), 13, 800, ui_colors::theme::BrandText,
                 TextAlignment::Center);
    AddFieldLine(content, *songsBrowse_, "Options.Songs.BrowseLine");
    songsDefault_ = &CreateButton(
        content, TopLeft({ContentLeft, controlTop + ControlHeight + 6, 160, ButtonHeight}, contentHeight), L"",
        "Options.Songs.Default");
    SetColors(*songsDefault_, {1, 1, 1, 0}, ui_colors::Rgb(0x1F4C99, .06F), ui_colors::Rgb(0x1F4C99, .12F),
              {1, 1, 1, 0});
    auto &defaultText = RequireComponent<TextVisualComponent>(*songsDefault_);
    StyleCaption(defaultText, 13, 800, ui_colors::theme::BrandText);
    defaultText.SetContentBounds({12, 0, 160 - 24, ButtonHeight});
}

void OptionsGeneral::Refresh()
{
    auto &texts = context_.Texts();
    SetCaption(*languageLabel_, std::wstring(Options(texts).language));
    auto &combo = RequireDropdown(*languageCombo_);
    const auto languages = texts.Languages();
    for (std::size_t index = 0; index < languages.size(); ++index)
    {
        if (languages[index].language == texts.CurrentLanguage())
        {
            combo.SetSelectedIndex(index);
            break;
        }
    }
    RefreshSongsControls();
}

void OptionsGeneral::RefreshSongsControls()
{
    const auto &texts = context_.Texts();
    const std::array values{std::pair{songsLabel_, std::wstring(texts.Get("Options.songsFolder"))},
                            std::pair{songsField_, context_.Settings().SongsRoot().wstring()},
                            std::pair{songsBrowse_, std::wstring(texts.Get("Options.browse"))},
                            std::pair{songsDefault_, std::wstring(texts.Get("Options.defaultSongsFolder"))}};
    for (const auto &[node, value] : values)
        SetCaption(*node, value);
}

bool OptionsGeneral::ProcessAction(const Action &action)
{
    if (action.type == ActionType::SelectionChanged && action.source == languageCombo_->Id())
        return SelectLanguage(action.selectedIndex);
    if (action.type != ActionType::Clicked)
        return false;
    if (action.source == songsField_->Id() || action.source == songsBrowse_->Id())
    {
        context_.CancelKeyboardEdit();
        ChooseSongsFolder();
        return true;
    }
    if (action.source == songsDefault_->Id())
    {
        UseDefaultSongsFolder();
        return true;
    }
    return false;
}

bool OptionsGeneral::SelectLanguage(const std::size_t index)
{
    auto &texts = context_.Texts();
    const auto languages = texts.Languages();
    if (index >= languages.size())
        return false;
    auto candidate = context_.Settings().Values();
    candidate.language = languages[index].language;
    if (context_.Save(candidate))
        texts.SetLanguage(candidate.language);
    context_.RefreshTexts();
    return true;
}

void OptionsGeneral::ChooseSongsFolder()
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
        result = dialog->SetTitle(std::wstring(context_.Texts().Get("Options.songsFolder")).c_str());
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
        context_.SetStatus(OptionsStatus::InvalidInput);
        context_.StatusDetail() = "Unable to select the Songs directory.";
    }
    else
    {
        auto candidate = context_.Settings().Values();
        candidate.songsPath = path.get();
        context_.Save(candidate);
    }
    RefreshSongsControls();
    context_.RefreshStatus();
}

void OptionsGeneral::UseDefaultSongsFolder()
{
    auto candidate = context_.Settings().Values();
    candidate.songsPath.clear();
    context_.Save(candidate);
    RefreshSongsControls();
    context_.RefreshStatus();
}

void OptionsGeneral::AppendCombos(std::vector<Visual2DNode *> &combos) const
{
    combos.push_back(languageCombo_);
}

void OptionsGeneral::Shutdown() noexcept
{
    languageLabel_ = languageCombo_ = nullptr;
    songsLabel_ = songsField_ = songsBrowse_ = songsDefault_ = nullptr;
}
