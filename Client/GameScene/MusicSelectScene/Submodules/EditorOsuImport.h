#pragma once
#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class TextCatalog;

// Folder dialog for an osu! song folder (the folder that holds the .osu files and their audio).
std::optional<std::filesystem::path> SelectOsuFolder(const TextCatalog &texts);

// One .osu file of the chosen folder, after the converter and the playability checks ran on it.
struct OsuImportDifficulty
{
    enum class State
    {
        Ready,           // Converted and playable; it can be placed into Songs.
        AlreadyImported, // Its chart set folder already exists in Songs.
        Failed,          // `problem` says why it cannot be imported.
    };
    std::filesystem::path source; // The .osu file.
    std::string name, maker;      // UTF-8, from the converted chart; empty when conversion failed.
    State state{State::Failed};
    std::string problem; // UTF-8 converter or validation message.
};
struct OsuImportScan
{
    bool success{};
    std::string message; // Why the folder could not be read at all.
    std::vector<OsuImportDifficulty> difficulties;
};
struct EditorOsuImportResult
{
    bool success{};      // Every chosen difficulty reached Songs.
    bool changed{};      // At least one did, so the song list must be read again.
    std::string message; // One line per difficulty that failed.
};

// Imports an osu! song folder in two background steps with the user's choice between them:
// a scan converts every .osu into a private temporary folder and checks it, then the chosen
// difficulties are placed into Songs. The osu! folder itself is only read.
// Workers own copied paths, their child processes and the temporary folder, never Scene/audio/UI objects.
class EditorOsuImport final
{
  public:
    EditorOsuImport();
    ~EditorOsuImport();
    void StartScan(std::filesystem::path folder, std::filesystem::path songsRoot);
    std::optional<OsuImportScan> PollScan();
    // Indices into the difficulties of the finished scan; only Ready ones may be chosen.
    void StartPlacement(std::vector<std::size_t> chosen);
    std::optional<EditorOsuImportResult> PollPlacement();
    // Forgets the finished scan and deletes its temporary folder. No job may be running.
    void Discard() noexcept;
    [[nodiscard]] bool Busy() const noexcept
    {
        return scan_.valid() || placement_.valid();
    }
    // One import's temporary folder and converted charts; defined with the workers.
    struct Session;

  private:
    std::shared_ptr<Session> session_;
    std::future<OsuImportScan> scan_;
    std::future<EditorOsuImportResult> placement_;
};
