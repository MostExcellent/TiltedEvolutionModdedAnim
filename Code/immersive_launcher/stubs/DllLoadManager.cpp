#include "DllLoadManager.h"

#include "loader/PathRerouting.h"

#include <codecvt>
#include <ranges>
#include <winternl.h>
#include <spdlog/spdlog.h>

DllLoadManager* DllLoadManager::s_instance = nullptr;

namespace EngineFixesHandler = DllHandlerFunc::EngineFixes;
namespace EngineFixesSettings = DllHandlerFunc::EngineFixes::Settings;

#pragma region InitializeDllManager
void DllLoadManager::Initialize()
{
    // In the future we could register handler functions elsewhere,
    // but for now we'll just do it here.
    DllHandling& engineFixesPolicy = s_DllPolicies[L"EngineFixes.dll"];
    engineFixesPolicy.PreloadHandler = EngineFixesHandler::PreloadHandler;

    // Iterate through the map.
    // If a Dll is supposed to be intercepted but has no valid handler, we'll block it.
    for (auto& dll_handling : s_DllPolicies | std::views::values)
    {
        if (dll_handling.policy == DllPolicy::INTERCEPT && dll_handling.PreloadHandler == nullptr)
        {
            dll_handling.policy = DllPolicy::BLOCK;
        }
    }
    
    bIsInitialized = true;
}

bool DllLoadManager::IsInitialized() const
{
    return bIsInitialized;
}
#pragma endregion

bool DllLoadManager::HandleDllLoad(const wchar_t* apPath, uint32_t* apFlags, const UNICODE_STRING* apFileName,
    HANDLE* apHandle)
{
    if (!bIsInitialized)
    {
        Initialize();
    }
    bool bAllowLoad = true;
    std::wstring_view fileName(apFileName->Buffer, apFileName->Length / sizeof(wchar_t));
    if (size_t pos = fileName.find_last_of(L'\\');
        pos != std::wstring_view::npos && (pos + 1) != fileName.length())
    {
        // Find the dll in the map, check if it's allowed or intercepted
        if (const auto& dll_handling = s_DllPolicies.find(&fileName[pos + 1]); dll_handling != s_DllPolicies.end())
        {
            if (dll_handling->second.policy == DllPolicy::INTERCEPT)
            {
                spdlog::info("DLL: {} - Intercepted", pos + 1);
                bAllowLoad = dll_handling->second.PreloadHandler();
            }
            else if (dll_handling->second.policy == DllPolicy::BLOCK)
            {
                spdlog::info("DLL: {} - Blocked", pos + 1);
                bAllowLoad = false;
            }
        }
        //else
        //{
        //    bAllowLoad = true;
        //}
    }

    // Convert std::wstring_view to std::string
    // First convert wstring_view to wstring
    std::wstring fileNameWStr(apFileName->Buffer, apFileName->Length / sizeof(wchar_t));
    std::string fileNameStr = wstring_utils::wstring_to_string(fileNameWStr);

    spdlog::info("DLL: {} - Allowed: {}", fileNameStr, bAllowLoad);
    return bAllowLoad;
}

#pragma region EngineFixesCompat
bool EngineFixesHandler::PreloadHandler()
{
    spdlog::info("EngineFixes PreloadHandler called - EngineFixes.dll detected");
    const auto config_path = get_config_path();

    // If config doesn't exist, create a minimal safe one
    if (!exists(config_path))
    {
        spdlog::info("Creating minimal safe Engine Fixes configuration");
        return create_safe_config(config_path);
    }

    try
    {
        // First try to parse the existing config
        const auto table = toml::parse_file(config_path.string());

        // Load settings from the file
        bool needs_update = false;
        const auto& all_settings = AutoTOML::ISetting::get_settings();

        for (const auto& setting : all_settings)
        {
            try
            {
                setting->load(table);
            }
            catch (const std::exception& e)
            {
                spdlog::warn("Failed to load setting: {}", e.what());
                needs_update = true;
            }
        }

        // Check if any critical settings need updating
        if (EngineFixesSettings::memoryManager.get() ||
            EngineFixesSettings::formCaching.get() ||
            EngineFixesSettings::scaleformAllocator.get()) /*|| 
                !EngineFixesSettings::memoryAccessErrors.get())*/
        {
            needs_update = true;
        }

        // If settings are unsafe, create backup and update config
        if (needs_update)
        {
            spdlog::info("Updating Engine Fixes configuration for STR compatibility");

            // Create backup if it doesn't exist
            const std::filesystem::path backup_path = config_path.string() + ".backup";
            if (!std::filesystem::exists(backup_path))
            {
                copy_file(config_path, backup_path);
            }

            // Create new safe config
            return create_safe_config(config_path);
        }

        spdlog::info("Engine Fixes configuration is already safe for STR");
        return true;
    }
    catch (const toml::parse_error& e)
    {
        spdlog::error("Failed to parse Engine Fixes config:\n{}", e.description());
        return false;
    }
    catch (const std::exception& e)
    {
        spdlog::error("Error handling Engine Fixes config: {}", e.what());
        return false;
    }
}

std::filesystem::path EngineFixesHandler::get_config_path()
{
    // current_path() is game root, correct?
    // Log current_path to check
    spdlog::info("Current path: {}", std::filesystem::current_path().string());
    return std::filesystem::current_path() / 
               "Data" / "SKSE" / "Plugins" / "EngineFixes.toml";
}

bool DllHandlerFunc::EngineFixes::create_safe_config(const std::filesystem::path& path)
{
    try {
        // Create the necessary directories
        std::filesystem::create_directories(path.parent_path());

        // Create a minimal TOML with just our critical settings
        std::ofstream config(path);
        if (!config.is_open()) {
            spdlog::error("Failed to create Engine Fixes config at: {}", path.string());
            return false;
        }

        config << "# Minimal Engine Fixes configuration for STR compatibility\n\n"
              << "[Patches]\n"
              << "MemoryManager = false\n"
              << "FormCaching = false\n"
              << "ScaleformAllocator = false\n\n"
              << "[Fixes]\n"
              << "MemoryAccessErrors = true\n";

        return true;
    }
    catch (const std::exception& e) {
        spdlog::error("Failed to create Engine Fixes config: {}", e.what());
        return false;
    }
}

#pragma endregion
