#pragma once

#include <functional>
#include <string>
#include <utils/ComUtils.h>
#include <winternl.h>
#include <AutoTOML.hpp>

//struct UNICODE_STRING;

class DllManager
{
public:
    // Represents how we want to handle a specific DLL
    enum class DllPolicy {
        ALLOW,              // Let the DLL load normally
        BLOCK,              // Prevent the DLL from loading
        INTERCEPT          // Run handler but allow loading
    };

    // Structure to hold handler(s) and policies for a DLL
    struct DllHandling {
        DllHandling() = default;
        DllHandling(DllPolicy aPolicy) : policy(aPolicy) {}

        DllPolicy policy = DllPolicy::BLOCK;
        std::function<bool()> PreloadHandler; // Return false in case of failure to prevent loading
        // For now we don't need this, but it could be handy in the future
        // std::function<void(HANDLE)> PostloadHandler;
    };

    private:
        static DllManager* s_instance;

        DllManager() = default;
        ~DllManager() = default;

    std::unordered_map<std::wstring, DllHandling> s_DllPolicies = {
        {L"EngineFixes.dll", DllHandling(DllPolicy::INTERCEPT)},
        {L"SkyrimSoulsRE.dll", DllHandling(DllPolicy::BLOCK)},
        {L"crashhandler64.dll", DllHandling(DllPolicy::BLOCK)},
        {L"fraps64.dll", DllHandling(DllPolicy::BLOCK)},
        {L"SpecialK64.dll", DllHandling(DllPolicy::BLOCK)},
        {L"ReShade64_SpecialK64.dll", DllHandling(DllPolicy::BLOCK)},
        {L"NvCamera64.dll", DllHandling(DllPolicy::BLOCK)}
    };

    // Should we later allow registering Dll policies from elsewhere?

    bool bIsInitialized = false;

public:
    static DllManager& Get() {
        if (!s_instance)
            s_instance = new DllManager();
        return *s_instance;
    }

    void Initialize();

    bool IsInitialized() const;

    // True if Dll is allowed, false if it should be blocked.
    bool HandleDllLoad(const wchar_t* apPath, uint32_t* apFlags, UNICODE_STRING* apFileName, HANDLE* apHandle);

    
};

namespace DllHandlerFunc
{
namespace EngineFixes
{
namespace Settings
{
// Memory manager is the most critical setting for STR compatibility
static AutoTOML::bSetting memoryManager{ "Patches", "MemoryManager", false };
        
// These settings need to be tested, and there may be others I haven't thought of
static AutoTOML::bSetting formCaching{ "Patches", "FormCaching", false };
static AutoTOML::bSetting scaleformAllocator{ "Patches", "ScaleformAllocator", false };
//static AutoTOML::bSetting memoryAccessErrors{ "Fixes", "MemoryAccessErrors", true };
} // namespace Settings
// Helpers

// Gets the path to the Engine Fixes config file
std::filesystem::path get_config_path();

// Creates a minimal safe configuration file, in case the original got deleted
bool create_safe_config(const std::filesystem::path& path);

// The main function that intercepts the config file and ensures it is safe for STR
bool PreloadHandler();
} // namespace EngineFixes
}
