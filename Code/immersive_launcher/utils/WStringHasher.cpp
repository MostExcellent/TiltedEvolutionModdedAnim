#include "WStringHasher.h"

#include "ComUtils.h"

#include <TiltedCore/Stl.hpp>

std::string wstring_utils::wstring_to_string(const std::wstring& wstr)
{
    if (wstr.empty()) {
        return {};
    }
    
    // Calculate required buffer size
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), 
                                        static_cast<int>(wstr.size()), nullptr, 0, 
                                        nullptr, nullptr);
    
    // Create string with the required size
    std::string strTo(size_needed, 0);
    
    // Perform the actual conversion
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()),
                        strTo.data(), size_needed, nullptr, nullptr);
    
    return strTo;
}
