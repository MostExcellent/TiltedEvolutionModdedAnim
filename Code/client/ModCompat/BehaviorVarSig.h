#pragma once

struct BehaviorVarSig
{
    struct Sig
    {
        std::string sigName;
        TiltedPhoques::Vector<std::string> sigStrings;
        TiltedPhoques::Vector<std::string> negSigStrings;
        TiltedPhoques::Vector<std::string> syncBooleanVar;
        TiltedPhoques::Vector<std::string> syncFloatVar;
        TiltedPhoques::Vector<std::string> syncIntegerVar;
    };

    struct Add
    {
        std::uint64_t mHash;
        TiltedPhoques::Vector<std::uint32_t> syncBooleanVar;
        TiltedPhoques::Vector<std::uint32_t> syncFloatVar;
        TiltedPhoques::Vector<std::uint32_t> syncIntegerVar;
    };

    // Sig pool
    TiltedPhoques::Vector<TiltedPhoques::UniquePtr<Sig>> sigPool;
    std::unordered_map<uint64_t, bool> failedSig;

    static BehaviorVarSig* Get();
    void initialize();
    bool initialized();
    void patch(BSAnimationGraphManager* apManager, Actor* apActor);

  private:
    bool m_initialized = false;
    TiltedPhoques::UniquePtr<Sig> loadSigFromDir(const std::string aDir);
    TiltedPhoques::Vector<std::string> loadDirs(const std::string& acPATH);
};
