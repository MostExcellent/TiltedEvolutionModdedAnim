#include <BSAnimationGraphManager.h>
#include <Games/ActorExtension.h>
#include <ModCompat/BehaviorVarSig.h>
#include <Structs/AnimationGraphDescriptorManager.h>

#include <algorithm>
#include <fstream>
#include <iostream>

// debug
#ifdef __config_releasedbg__
#define D(...) spdlog::info(__VA_ARGS__)
#else
#define D(...)
#endif

//Credit to EDHO for original version of this code
//I did the smart pointer refactoring, and changes after that

BehaviorVarSig* BehaviorVarSig::Get() {
    static BehaviorVarSig instance;
    return &instance;
}

void removeWhiteSpace(std::string& aString)
{
    aString.erase(std::remove_if(aString.begin(), aString.end(), [](char c) { return std::isspace(c); }),
                  aString.end());
}

bool isDirExist(std::string aPath)
{
    return std::filesystem::is_directory(aPath);
}

void BehaviorVarSig::initialize()
{
    const std::string PATH = TiltedPhoques::GetPath().string() + "/behaviors";
    if (!isDirExist(PATH))
        return;

    auto dirs = loadDirs(PATH);
    for (auto& item : dirs)
    {
        auto sig = loadSigFromDir(item);
        if (sig)
        {
            sigPool.push_back(std::move(sig));
        }
        else
        {
            D("sig is null");
        }
    }
    m_initialized = true;
}

bool BehaviorVarSig::initialized()
{
    return m_initialized;
}

void BehaviorVarSig::patch(BSAnimationGraphManager* apManager, Actor* apActor)
{
    // retrieve the formID of the Actor
    uint32_t hexFormID = apActor->formID;

    // Retrieve the character's extended animation graph
    auto pExtendedActor = apActor->GetExtension();

    // Query the AnimationGraphDescriptorManager to retrieved the specified animation graph of our character
    const AnimationGraphDescriptor* pGraph =
        AnimationGraphDescriptorManager::Get().GetDescriptor(pExtendedActor->GraphDescriptorHash);

    // An animation graph already exists or the signature has already failed
    if (pGraph != nullptr || failedSig.find(pExtendedActor->GraphDescriptorHash) != failedSig.end())
        return;

    D("actor with formID {:x} with hash of {} has modded behavior", hexFormID, pExtendedActor->GraphDescriptorHash);

    // Retrieve all animation variables, which are currently stored in a sorted map
    auto dumpVar = apManager->DumpAnimationVariables(false);

    // Create a reverse unordered map
    std::unordered_map<std::string, uint32_t> reverseMap;
    for (auto& pair : dumpVar)
    {
        reverseMap.emplace(pair.second, pair.first);
    }

    // Print the name of all known behavior variables and their corresponding values
    D("known behavior variables");
    for (const auto& [name, value] : dumpVar)
    {
        D("{}: {}", name, value);
    }

    // Check if the current signature is satisfied by inspecting the animation variables
    for (const auto& sig : sigPool)
    {
        D("sig: {}", sig->sigName);

        bool isSig = true;
        for (const auto& sigVar : sig->sigStrings)
        {
            if (reverseMap.find(sigVar) == reverseMap.end())
            {
                isSig = false;
                break;
            }
        }
        if (!isSig)
            continue;

        for (const auto& negSigVar : sig->negSigStrings)
        {
            if (reverseMap.find(negSigVar) != reverseMap.end())
            {
                isSig = false;
                break;
            }
        }

        // Signature was found, calculate the hash and prepare an empty animation graph descriptor
        if (isSig)
        {
            D("sig found as {}", sig->sigName);

            // Calculate the hash of the animation graph
            uint64_t mHash = apManager->GetDescriptorKey();

            D("sig {} has an animation graph hash of {}", sig->sigName, mHash);

            // Create vectors for syncing the specified variables
            TiltedPhoques::Vector<uint32_t> boolVar;
            TiltedPhoques::Vector<uint32_t> floatVar;
            TiltedPhoques::Vector<uint32_t> intVar;

            // Iterate over all syncBooleanVar and add variable IDs to boolVar if they exist in reverseMap
            for (const auto& var : sig->syncBooleanVar)
            {
                const auto it = reverseMap.find(var);
                if (it == reverseMap.end())
                    continue;
                D("{}: {}", it->second, var);
                boolVar.push_back(it->second);
            }

            // Iterate over all syncFloatVar and add variable IDs to floatVar if they exist in reverseMap
            for (const auto& var : sig->syncFloatVar)
            {
                const auto it = reverseMap.find(var);
                if (it == reverseMap.end())
                    continue;
                D("{}: {}", it->second, var);
                floatVar.push_back(it->second);
            }

            // Iterate over all syncIntegerVar and add variable IDs to intVar if they exist in reverseMap
            for (const auto& var : sig->syncIntegerVar)
            {
                const auto it = reverseMap.find(var);
                if (it == reverseMap.end())
                    continue;
                D("{}: {}", it->second, var);
                intVar.push_back(it->second);
            }

            // Build a new animation graph descriptor with empty vectors for syncBoolVar, syncFloatVar, and
            // syncIntegerVar
            AnimationGraphDescriptor newDescriptor = AnimationGraphDescriptor({0}, {0}, {0});
            newDescriptor.BooleanLookUpTable = boolVar;
            newDescriptor.FloatLookupTable = floatVar;
            newDescriptor.IntegerLookupTable = intVar;

            // Add the brand new animation graph descriptor to AnimationGraphDescriptorManager() with the mHash key
            new AnimationGraphDescriptorManager::Builder(AnimationGraphDescriptorManager::Get(), mHash, newDescriptor);

            // Apply updated animation graph and exit the function
            pExtendedActor->GraphDescriptorHash = mHash;
            return;
        }
    }

    D("sig for actor {:x} failed with hash {}", hexFormID, pExtendedActor->GraphDescriptorHash);

    // Failed signature. Store in map to prevent future attempts.
    failedSig[pExtendedActor->GraphDescriptorHash] = true;
}


TiltedPhoques::Vector<std::string> BehaviorVarSig::loadDirs(const std::string& acPATH)
{
    TiltedPhoques::Vector<std::string> result;
    for (auto& p : std::filesystem::directory_iterator(acPATH))
        if (p.is_directory())
            result.push_back(p.path().string());
    return result;
}

TiltedPhoques::UniquePtr<BehaviorVarSig::Sig> BehaviorVarSig::loadSigFromDir(std::string aDir)
{

    D("creating sig");

    std::string nameVarFileDir;
    std::string sigFileDir;
    TiltedPhoques::Vector<std::string> floatVarFileDir;
    TiltedPhoques::Vector<std::string> intVarFileDir;
    TiltedPhoques::Vector<std::string> boolVarFileDir;

    ////////////////////////////////////////////////////////////////////////
    // Enumerate all files in this directory
    ////////////////////////////////////////////////////////////////////////

    for (auto& p : std::filesystem::directory_iterator(aDir))
    {
        std::string path = p.path().string();
        std::string base_filename = path.substr(path.find_last_of("/\\") + 1);

        D("base_path: {}", base_filename);

        if (base_filename.find("__name.txt") != std::string::npos)
        {
            nameVarFileDir = path;

            D("name file: {}", nameVarFileDir);
        }
        else if (base_filename.find("__sig.txt") != std::string::npos)
        {
            sigFileDir = path;

            D("sig file: {}", path);
        }
        else if (base_filename.find("__float.txt") != std::string::npos)
        {
            floatVarFileDir.push_back(path);

            D("float file: {}", path);
        }
        else if (base_filename.find("__int.txt") != std::string::npos)
        {
            intVarFileDir.push_back(path);

            D("int file: {}", path);
        }
        else if (base_filename.find("__bool.txt") != std::string::npos)
        {
            boolVarFileDir.push_back(path);

            D("bool file: {}", path);
        }
    }

    ////////////////////////////////////////////////////////////////////////
    // sanity check
    ////////////////////////////////////////////////////////////////////////
    if (nameVarFileDir == "" || sigFileDir == "")
    {
        return nullptr;
    }

    ////////////////////////////////////////////////////////////////////////
    // read the files
    ////////////////////////////////////////////////////////////////////////
    std::string name = "";
    TiltedPhoques::Vector<std::string> sig;
    TiltedPhoques::Vector<std::string> negSig;
    std::set<std::string> floatVar;
    std::set<std::string> intVar;
    std::set<std::string> boolVar;

    // read name var
    std::string tempString;
    std::ifstream file(nameVarFileDir);
    getline(file, tempString);
    name = tempString;
    removeWhiteSpace(name);
    file.close();
    if (name == "")
        return nullptr;

    // read sig var

    D("creating sig for {}", name);

    std::ifstream file1(sigFileDir);
    while (std::getline(file1, tempString))
    {
        removeWhiteSpace(tempString);
        if (tempString.find("~") != std::string::npos)
        {
            negSig.push_back(tempString.substr(tempString.find("~") + 1));

            D("~{}:{}", name, tempString.substr(tempString.find("~") + 1));
        }
        else
        {
            sig.push_back(tempString);

            D("{}:{}", name, tempString);
        }
    }
    file1.close();
    if (sig.size() < 1)
    {
        return nullptr;
    }

    D("reading float var", name, tempString);

    // read float var
    for (auto item : floatVarFileDir)
    {
        std::ifstream file2(item);
        while (std::getline(file2, tempString))
        {
            removeWhiteSpace(tempString);
            floatVar.insert(tempString);

            D(tempString);
        }
        file2.close();
    }

    D("reading int var", name, tempString);

    // read int var
    for (auto item : intVarFileDir)
    {
        std::ifstream file3(item);
        while (std::getline(file3, tempString))
        {
            removeWhiteSpace(tempString);
            intVar.insert(tempString);

            D(tempString);
        }
        file3.close();
    }

    D("reading bool var", name, tempString);

    // read bool var
    for (auto item : boolVarFileDir)
    {
        std::ifstream file4(item);
        while (std::getline(file4, tempString))
        {
            removeWhiteSpace(tempString);
            boolVar.insert(tempString);

            D(tempString);
        }
        file4.close();
    }

    // convert set to vector
    TiltedPhoques::Vector<std::string> floatVector;
    TiltedPhoques::Vector<std::string> intVector;
    TiltedPhoques::Vector<std::string> boolVector;

    for (auto item : floatVar)
    {
        floatVector.push_back(item);
    }

    for (auto item : intVar)
    {
        intVector.push_back(item);
    }

    for (auto item : boolVar)
    {
        boolVector.push_back(item);
    }

    // create the sig
    TiltedPhoques::UniquePtr<Sig> result(new Sig());

    result->sigName = name;
    result->sigStrings = sig;
    result->negSigStrings = negSig;
    result->syncBooleanVar = boolVector;
    result->syncFloatVar = floatVector;
    result->syncIntegerVar = intVector;

    return result;
}
