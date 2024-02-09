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

    // Add pool
    TiltedPhoques::Vector<TiltedPhoques::UniquePtr<Add>> addPool;
    bool isAddPatched;

    static BehaviorVarSig* Get();
    void initialize();
    void patch(BSAnimationGraphManager* apManager, Actor* apActor);
    void patchAdd(Add& aAdd);

  private:
    //static BehaviorVarSig* single;
    TiltedPhoques::UniquePtr<Sig> loadSigFromDir(const std::string aDir);
    TiltedPhoques::UniquePtr<Add> loadAddFromDir(const std::string aDir);
    void tryAddtoHash(BehaviorVarSig::Add& aAdd);
    TiltedPhoques::Vector<std::string> loadDirs(const std::string& acPATH);
    void vanillaPatch();
};
