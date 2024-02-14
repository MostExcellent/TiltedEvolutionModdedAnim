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

    // To store sigs and failed sigs
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
