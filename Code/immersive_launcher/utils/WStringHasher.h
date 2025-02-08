#pragma once

#include <string>

struct WStringHasher {
    using is_transparent = void;  // Mark as transparent
    
    size_t operator()(const std::wstring& str) const {
        return std::hash<std::wstring>{}(str);
    }
    
    size_t operator()(const wchar_t* str) const {
        return std::hash<std::wstring>{}(str);
    }
};

namespace wstring_utils
{
std::string wstring_to_string(const std::wstring& wstr);
}
