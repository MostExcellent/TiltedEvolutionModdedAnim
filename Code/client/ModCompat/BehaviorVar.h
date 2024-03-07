#pragma once
#ifdef MODDED_BEHAVIOR_COMPATIBILITY

#include <Structs/AnimationGraphDescriptorManager.h>

struct BehaviorVar
{
    struct Replacer
    {
        uint64_t orgHash;
        uint64_t newHash;
        std::string signatureVar;
        std::string creatureName;
        std::vector<std::string> syncBooleanVar;
        std::vector<std::string> syncFloatVar;
        std::vector<std::string> syncIntegerVar;
    };

    static BehaviorVar* Get();
    const AnimationGraphDescriptor* Patch(BSAnimationGraphManager* apManager, Actor* apActor);
    boolean failListed(uint64_t hash);
    void failList(uint64_t hash);

    void Init();
    void Debug();

  private:
    static BehaviorVar* single;
    uint64_t invocations = 0;

    void seedAnimationVariables(
    uint64_t hash, 
    const AnimationGraphDescriptor* pDescriptor,
    std::map<const std::string, const uint32_t>& reversemap,
    std::set<uint32_t>& boolVars, 
    std::set<uint32_t>& floatVars,
    std::set<uint32_t>& intVars);

    const std::vector<std::string> tokenizeBehaviorSig(const std::string signature) const;

    std::vector<std::filesystem::path> loadDirs(const std::filesystem::path& acPATH);
    Replacer* loadReplacerFromDir(std::filesystem::path aDir);

    std::vector<Replacer> behaviorPool; // Pool for loaded behaviours
    std::map<uint64_t, std::chrono::steady_clock::time_point> failedBehaviors;
};

extern const AnimationGraphDescriptor* BehaviorVarPatch(BSAnimationGraphManager* apManager, Actor* apActor);

//namespace BehaviorVarUtils
//{
//void ProcessVariableSet(const std::map<const std::string, const uint32_t>& reversemap, std::set<uint32_t>& variableSet,
//                        const std::vector<std::string>& variables, const bool lowerFallback, const uint8 logLevel);
//}

#endif MODDED_BEHAVIOR_COMPATIBILITY
