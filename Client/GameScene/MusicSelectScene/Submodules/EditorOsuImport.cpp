#include "EditorOsuImport.h"
#include "OsuImportValidation.h"
#include "SongSelectionText.h"
#include "Texts/GameScene/MusicSelectScene/MusicSelectTexts.h"
#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cwctype>
#include <fstream>
#include <memory>
#include <shobjidl.h>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    class Handle final
    {
      public:
        explicit Handle(HANDLE value) : value_(value)
        {
            if (!value || value == INVALID_HANDLE_VALUE)
                throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
        }
        ~Handle()
        {
            CloseHandle(value_);
        }
        Handle(const Handle &) = delete;
        Handle &operator=(const Handle &) = delete;
        HANDLE Get() const noexcept
        {
            return value_;
        }

      private:
        HANDLE value_;
    };
    std::wstring Quote(std::wstring_view value)
    {
        std::wstring result = L"\"";
        std::size_t slashes{};
        for (const auto c : value)
        {
            if (c == L'\\')
            {
                ++slashes;
                continue;
            }
            result.append(c == L'\"' ? slashes * 2 + 1 : slashes, L'\\');
            slashes = 0;
            result.push_back(c);
        }
        result.append(slashes * 2, L'\\');
        return result + L'\"';
    }
    fs::path ConverterPath()
    {
        std::wstring file(32768, L'\0');
        const auto count = GetModuleFileNameW(nullptr, file.data(), static_cast<DWORD>(file.size()));
        if (!count || count >= file.size())
            throw std::runtime_error("Cannot locate the game executable.");
        file.resize(count);
        return fs::path(file).parent_path() / L"OsuTaikoConverter.exe";
    }
    class Capture final
    {
      public:
        Capture()
        {
            const auto parent = fs::temp_directory_path();
            const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
            for (int retry = 0; retry < 100; ++retry)
            {
                path = parent / (L"YOUMU-osu-" + std::to_wstring(nonce) + L"-" + std::to_wstring(retry));
                if (fs::create_directory(path))
                    return;
            }
            throw std::runtime_error("Cannot create converter output capture.");
        }
        ~Capture()
        {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
        Capture(const Capture &) = delete;
        Capture &operator=(const Capture &) = delete;
        fs::path path;
    };
    std::string ReadCapture(const fs::path &path)
    {
        const auto size = fs::file_size(path);
        if (size > 16 * 1024 * 1024)
            throw std::runtime_error("Converter output exceeds 16 MiB.");
        std::ifstream input(path, std::ios::binary);
        std::string text(static_cast<std::size_t>(size), '\0');
        if (!input.read(text.data(), static_cast<std::streamsize>(text.size())))
            throw std::runtime_error("Cannot read converter output.");
        return text;
    }
    DWORD Run(const fs::path &exe, std::wstring command, const fs::path &directory)
    {
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        Handle output(CreateFileW((directory / L"stdout").c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security,
                                  CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
        Handle errors(CreateFileW((directory / L"stderr").c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security,
                                  CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
        Handle input(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0,
                                 nullptr));
        SIZE_T size{};
        InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
        if (!size || GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            throw std::runtime_error("Cannot query process handle-list storage.");
        std::vector<std::byte> storage(size);
        auto *attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
        if (!attributes || !InitializeProcThreadAttributeList(attributes, 1, 0, &size))
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
        const auto cleanup = std::unique_ptr<_PROC_THREAD_ATTRIBUTE_LIST, decltype(&DeleteProcThreadAttributeList)>(
            attributes, DeleteProcThreadAttributeList);
        HANDLE inherited[]{output.Get(), errors.Get(), input.Get()};
        if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited),
                                       nullptr, nullptr))
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb = sizeof(startup);
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdOutput = output.Get();
        startup.StartupInfo.hStdError = errors.Get();
        startup.StartupInfo.hStdInput = input.Get();
        startup.lpAttributeList = attributes;
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, TRUE,
                            CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &startup.StartupInfo,
                            &process))
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
        Handle processHandle(process.hProcess), threadHandle(process.hThread);
        if (WaitForSingleObject(processHandle.Get(), INFINITE) != WAIT_OBJECT_0)
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
        DWORD code{};
        if (!GetExitCodeProcess(processHandle.Get(), &code))
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
        return code;
    }
    std::string Utf8(const fs::path &path)
    {
        const auto text = path.generic_u8string();
        return {reinterpret_cast<const char *>(text.data()), text.size()};
    }
    // The .osu files directly inside the folder, in name order.
    std::vector<fs::path> OsuFiles(const fs::path &folder)
    {
        std::vector<fs::path> files;
        for (const auto &entry : fs::directory_iterator(folder))
        {
            auto extension = entry.path().extension().native();
            std::ranges::transform(extension, extension.begin(),
                                   [](const wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
            if (entry.is_regular_file() && extension == L".osu")
                files.push_back(entry.path());
        }
        std::ranges::sort(files);
        return files;
    }
    // COM for the folder dialog. A thread that already uses another apartment model keeps its own.
    class ComApartment final
    {
      public:
        ComApartment() : initialized_(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {}
        ~ComApartment()
        {
            if (initialized_)
                CoUninitialize();
        }
        ComApartment(const ComApartment &) = delete;
        ComApartment &operator=(const ComApartment &) = delete;

      private:
        bool initialized_;
    };
    template <typename Interface> struct ComRelease
    {
        void operator()(Interface *pointer) const noexcept
        {
            pointer->Release();
        }
    };
    template <typename Interface> using ComPointer = std::unique_ptr<Interface, ComRelease<Interface>>;
    void RequireDialog(const HRESULT result)
    {
        if (FAILED(result))
            throw std::runtime_error("Cannot open the osu! folder dialog (" + std::to_string(result) + ").");
    }
} // namespace

// Shared by the object and its worker. The worker fills `charts` and the object reads it only
// after that worker's future is ready, so no lock is needed.
struct EditorOsuImport::Session
{
    Capture stage; // Every converter output of this import; deleted with the session.
    fs::path songsRoot;
    // Parallel to the scan's difficulties; a failed difficulty keeps an empty chart.
    std::vector<ConvertedOsuChart> charts;
    std::vector<std::string> labels;
};

namespace
{
    using Difficulty = OsuImportDifficulty;
    // Converts one .osu into its own subfolder of the session and checks the result. The
    // converter writes three files there and nothing into the osu! folder.
    Difficulty ConvertDifficulty(EditorOsuImport::Session &session, const fs::path &exe, const fs::path &map)
    {
        Difficulty row;
        row.source = map;
        const auto index = session.charts.size();
        session.charts.emplace_back();
        session.labels.push_back(Utf8(map.filename()));
        try
        {
            const auto work = session.stage.path / std::to_wstring(index);
            const auto output = work / L"files";
            fs::create_directories(output);
            const auto command = Quote(exe.native()) + L" --input " + Quote(map.native()) + L" --output-dir " +
                                 Quote(output.native());
            const auto code = Run(exe, command, work);
            if (code != 0)
            {
                row.problem = ReadCapture(work / L"stderr");
                if (row.problem.empty())
                    row.problem = "Converter failed (exit " + std::to_string(code) + ").";
                return row;
            }
            auto chart = ValidateConvertedOsuResult(ReadCapture(work / L"stdout"), output);
            chart.sourceFolder = map.parent_path();
            row.name = chart.pattern.name;
            row.maker = chart.pattern.makers.empty() ? std::string{} : chart.pattern.makers.front();
            const auto plan = PlanOsuPlacement(session.songsRoot, chart);
            row.problem = plan.problem;
            if (plan.problem.empty())
                row.state = plan.chartExists ? Difficulty::State::AlreadyImported : Difficulty::State::Ready;
            session.labels.back() = row.name;
            session.charts.back() = std::move(chart);
        }
        catch (const std::exception &error)
        {
            row.problem = error.what();
        }
        return row;
    }
    OsuImportScan Scan(EditorOsuImport::Session &session, const fs::path &folder)
    {
        try
        {
            const auto exe = ConverterPath();
            const auto maps = OsuFiles(folder);
            if (maps.empty())
                return {false, "The folder holds no .osu file: " + Utf8(folder), {}};
            OsuImportScan scan{true, {}, {}};
            for (const auto &map : maps)
                scan.difficulties.push_back(ConvertDifficulty(session, exe, map));
            return scan;
        }
        catch (const std::exception &error)
        {
            return {false, error.what(), {}};
        }
    }
    EditorOsuImportResult Place(const EditorOsuImport::Session &session, const std::vector<std::size_t> &chosen)
    {
        EditorOsuImportResult result{true, false, {}};
        for (const auto index : chosen)
        {
            try
            {
                (void)PlaceConvertedOsuChart(session.songsRoot, session.charts.at(index));
                result.changed = true;
            }
            catch (const std::exception &error)
            {
                result.success = false;
                const auto label = index < session.labels.size() ? session.labels[index] : std::string{"?"};
                result.message += label + ": " + error.what() + "\n";
            }
        }
        return result;
    }
} // namespace

std::optional<std::filesystem::path> SelectOsuFolder(const TextCatalog &texts)
{
    const ComApartment apartment;
    IFileOpenDialog *created{};
    RequireDialog(CoCreateInstance(__uuidof(FileOpenDialog), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&created)));
    const ComPointer<IFileOpenDialog> dialog(created);
    DWORD options{};
    RequireDialog(dialog->GetOptions(&options));
    RequireDialog(dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST |
                                     FOS_NOCHANGEDIR));
    RequireDialog(dialog->SetTitle(std::wstring(MusicSelect(texts).convertOsu).c_str()));
    const auto shown = dialog->Show(GetActiveWindow());
    if (shown == HRESULT_FROM_WIN32(ERROR_CANCELLED))
        return {};
    RequireDialog(shown);
    IShellItem *chosen{};
    RequireDialog(dialog->GetResult(&chosen));
    const ComPointer<IShellItem> item(chosen);
    PWSTR text{};
    RequireDialog(item->GetDisplayName(SIGDN_FILESYSPATH, &text));
    const fs::path folder(text);
    CoTaskMemFree(text);
    return folder;
}

EditorOsuImport::EditorOsuImport() = default;
EditorOsuImport::~EditorOsuImport() = default;

void EditorOsuImport::StartScan(std::filesystem::path folder, std::filesystem::path songsRoot)
{
    if (Busy())
        throw std::logic_error("An osu! import is already running.");
    session_ = std::make_shared<Session>();
    session_->songsRoot = std::move(songsRoot);
    scan_ = std::async(std::launch::async,
                       [session = session_, folder = std::move(folder)] { return Scan(*session, folder); });
}
std::optional<OsuImportScan> EditorOsuImport::PollScan()
{
    if (!scan_.valid() || scan_.wait_for(std::chrono::seconds{0}) != std::future_status::ready)
        return {};
    try
    {
        return scan_.get();
    }
    catch (const std::exception &error)
    {
        return OsuImportScan{false, error.what(), {}};
    }
}
void EditorOsuImport::StartPlacement(std::vector<std::size_t> chosen)
{
    if (Busy() || !session_)
        throw std::logic_error("An osu! import can place difficulties only after its scan finished.");
    placement_ = std::async(std::launch::async,
                            [session = session_, chosen = std::move(chosen)] { return Place(*session, chosen); });
}
std::optional<EditorOsuImportResult> EditorOsuImport::PollPlacement()
{
    if (!placement_.valid() || placement_.wait_for(std::chrono::seconds{0}) != std::future_status::ready)
        return {};
    EditorOsuImportResult result;
    try
    {
        result = placement_.get();
    }
    catch (const std::exception &error)
    {
        result = {false, false, error.what()};
    }
    session_.reset(); // The converter outputs are no longer needed.
    return result;
}
void EditorOsuImport::Discard() noexcept
{
    if (!Busy())
        session_.reset();
}
