
//
// Core support for modified behavior (as in Nemesis or Pandora behavior modification or extension).
//
// One of the greatest features ensuring the longevity of Bethesda games is their support
// for community modification, and many of the most popular mods also change the behavior 
// of creatures. STR and FTR don't support mods as a matter of policy. And supporting
// behavior mods is particularly difficult.
// 
// The game determines a set of behavior variables that must be synced and locks thatdown.
// Changing the list is difficult, because behavior mods won't just add to the list, they
// will change the order of the list also changing the shorthand numeric codes for behavior
// vars to sync.
//
// So, supporting behavior mods means:
//     1) We must be able the translate the built-in behavior var codes back to their string values.
//     2) We must then retranslate the string values back to their (new) numeric values.
//     3) We must add in the additional behavior variables requested by modders
//     4) Finally, that will push us over some hard-coded limits, in particular
//        a limit of 64 boolean vars that can be synced. Remove the limit.
// 
#ifdef MODDED_BEHAVIOR_COMPATIBILITY

#include <immersive_launcher/launcher.h>
#include <BSAnimationGraphManager.h>
#include <Games/ActorExtension.h>
#include "BehaviorVar.h"
#include "BehaviorVarsMap.h"

#if TP_SKYRIM64
#include <Structs/Skyrim/AnimationGraphDescriptor_Master_Behavior.h>
#include <Camera/TESCamera.h>       // Camera 1st person is only in Skyrim?
#include <Camera/PlayerCamera.h>
#endif

//#include <functional>

#include <mutex>
std::mutex mutex_lock;

// How long we failList a behavior hash that can't be translated.
// See explanation in BehaviorVar::Patch
using namespace std::literals;
const std::chrono::steady_clock::duration FAILLIST_DURATION(5min);

BehaviorVar* BehaviorVar::single = nullptr;
BehaviorVar* BehaviorVar::Get()
{
    if (!BehaviorVar::single)
        BehaviorVar::single = new BehaviorVar();
    return BehaviorVar::single;
}

// A simple function can be declared, caller file doesn't have to include our 
// class/struct header.
const AnimationGraphDescriptor* BehaviorVarPatch(BSAnimationGraphManager* apManager, Actor* apActor)
{
    return BehaviorVar::Get()->Patch(apManager, apActor);
}

// A note on some of the following changes relative to rfortier's code:
// Yes, it's probably overkill, but it was a bit of an exercise for me

// Utility function to convert a string to lowercase
// This is useful as at least one vanilla variable, "Speed", is sometimes lowercase
// We'd like to be able to handle other vars for which this may be the case.
std::string toLowerCase(const std::string& str)
{
    std::string lowerCaseStr = str;
    std::transform(lowerCaseStr.begin(), lowerCaseStr.end(), lowerCaseStr.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return lowerCaseStr;
}

// Converts the keys of a map (which are const std::string) to lowercase.
// This is our "Plan C" if a vanilla var isn't found to make sure it isn't just a case-sensitivity issue.
template <typename T> std::map<const std::string, T> toLowerCaseKeys(const std::map<const std::string, T>& map)
{
    std::map<std::string, T> lowerCaseMap;
    for (const auto& item : map)
        lowerCaseMap.insert({toLowerCase(item.first), item.second});
    return lowerCaseMap;
}

// We can forward a message to spdlog with the desired log level so we can reuse processVariableSet
enum class LogLevel
{
    Info,
    Warn,
    Error
};

template <typename... Args> void logMessage(LogLevel logLevel, const char* const format, Args&&... args)
{
    switch (logLevel)
    {
    case LogLevel::Info:
        spdlog::info(format, std::forward<Args>(args)...);
        break;
    case LogLevel::Warn:
        spdlog::warn(format, std::forward<Args>(args)...);
        break;
    case LogLevel::Error:
        spdlog::error(format, std::forward<Args>(args)...);
        break;
    default:
        throw std::invalid_argument("Unknown log level");
    }
}

// Governs how to handle case sensitivity in the event of a variable not being found
// Vanilla vars should 100% be found, so we triple check.
// Replacer patches are expected to be correctly cased so we just move on if not found.
enum class CaseFallback
{
    None,
    Var, // Here for completeness, but imo no reason not to triple-check vanilla vars if not found
    VarAndMap
};

// Process a set of variables, adding them to the variableSet
void processVariableSet(const std::map<const std::string, const uint32_t>& reversemap, std::set<uint32_t>& variableSet,
                        const std::vector<std::string>& variables, const CaseFallback fallbackLevel, LogLevel logLevel)
{
    // Not filled until needed, which should be never.
    std::map<const std::string, const uint32_t> lowerCaseMap;

    for (const auto& item : variables)
    {
        auto found = reversemap.find(item);
        if (found != reversemap.end())
        {

            variableSet.insert(found->second);
        }
        else if (fallbackLevel == CaseFallback::Var || fallbackLevel == CaseFallback::VarAndMap)
        {
            logMessage(logLevel, "Unable to find variable {}, trying lowercase...", item);
            auto foundLower = reversemap.find(toLowerCase(item));
            if (foundLower != reversemap.end())
            {
                variableSet.insert(foundLower->second);
                spdlog::warn("Compensated for incorrect capitalization of variable '{}'", item);
            }
            else if (fallbackLevel == CaseFallback::VarAndMap)
            {
                logMessage(logLevel,
                            "Still unable to find variable {} (lowercase), trying case-insensitive search...", item);
                if (lowerCaseMap.empty())
                    lowerCaseMap = toLowerCaseKeys(reversemap);
                auto foundLowerMap = lowerCaseMap.find(toLowerCase(item));
                if (foundLowerMap != lowerCaseMap.end())
                {
                    variableSet.insert(foundLowerMap->second);
                    spdlog::warn("Compensated for incorrect capitalization of variable '{}' with case-insensitive search", item);
                }
                else
                {
                    logMessage(logLevel, "Unable to find variable {} with case-insensitive search", item);
                }
            }
            else
            {
                logMessage(logLevel, "Unable to find variable {} (lowercase)", item);
            }
        }
        else
        {
            logMessage(logLevel, "Unable to find variable {}", item);
        }
    }
}

//
// Translate modded behavior numeric values.
// When a behavior is modified (at least by Nemesis) it can rearrange the order of BehaviorVars.
// Since their corresponding numeric value is based on the order, we have to translate the
// BehaviorVars chosen (numerically) by the STR devs to their new numeric values.
// We do this with a hack to translate old numeric value back to a string, then we
// can forward-translate the string to its new numeric value.
// 
// The machine-generated table hack to do this can be removed with STR-devs'
// permission to also embed the string information in the Code\encoding\structs files.
//
void BehaviorVar::seedAnimationVariables(uint64_t hash, const AnimationGraphDescriptor* pDescriptor,
                                         std::map<const std::string, const uint32_t>& reversemap,
                                         std::set<uint32_t>& boolVars, std::set<uint32_t>& floatVars,
                                         std::set<uint32_t>& intVars)
{
    auto& origVars = BehaviorVarsMap::getInstance();

    // Prepare lists of variables to process
    std::vector<std::string> boolVarNames;
    std::vector<std::string> floatVarNames;
    std::vector<std::string> intVarNames;

    // Populate lists from the original descriptor
    for (auto& item : pDescriptor->BooleanLookUpTable)
    {
        auto strValue = origVars.find(hash, item);
        if (!strValue.empty())
            boolVarNames.push_back(strValue);
    }
    for (auto& item : pDescriptor->FloatLookupTable)
    {
        auto strValue = origVars.find(hash, item);
        if (!strValue.empty())
            floatVarNames.push_back(strValue);
    }
    for (auto& item : pDescriptor->IntegerLookupTable)
    {
        auto strValue = origVars.find(hash, item);
        if (!strValue.empty())
            intVarNames.push_back(strValue);
    }

    // Process each set of variables
    processVariableSet(reversemap, boolVars, boolVarNames, CaseFallback::VarAndMap, LogLevel::Error);
    processVariableSet(reversemap, floatVars, floatVarNames, CaseFallback::VarAndMap, LogLevel::Error);
    processVariableSet(reversemap, intVars, intVarNames, CaseFallback::VarAndMap, LogLevel::Error);
}

const std::vector<std::string> BehaviorVar::tokenizeBehaviorSig(const std::string signature) const
{
    // Syntax is [!]sig1[,!sig2]... Must be at least one. Each signature var possibly negated
    // (meaning it MUST NOT be in the BehaviorVars of the actor). Separated by commas.
    // Requires that whitespace has already been deleted (was, when file was read).
    // Tokenize on ','
    //
    std::vector<std::string> retVal;
    size_t commaPos;
    size_t offset = 0;

    do
    {
        commaPos = signature.find(',', offset);
        retVal.push_back(signature.substr(offset, commaPos));
        offset = commaPos + 1;

    } while (commaPos != std::string::npos);

    return retVal;
}

const AnimationGraphDescriptor* BehaviorVar::Patch(BSAnimationGraphManager* apManager, Actor* apActor)
{
    // Serialize, Actors are multi-threaded
    std::lock_guard guard(mutex_lock);

    // Check if the animation descriptor has already been built.
    uint32_t hexFormID = apActor->formID;
    auto pExtendedActor = apActor->GetExtension();
    auto hash = pExtendedActor->GraphDescriptorHash;
    const AnimationGraphDescriptor* pGraph = AnimationGraphDescriptorManager::Get().GetDescriptor(hash);

#if TP_SKYRIM64
    // If it is the player-character AND they are in 1st person, we don't have the data to mod them;
    // Used the base game animation graphs until a Master Behavior actor enters the room. Could be an NPC,
    // but will always happen no later than when the 2nd person joins the server and room.
    // Remote players are ALWAYS in 3rd person by definition
    if (hexFormID == 0x14 && PlayerCamera::Get()->IsFirstPerson())
    {
        hash   = AnimationGraphDescriptor_Master_Behavior::m_key;
        pGraph = AnimationGraphDescriptorManager::Get().GetDescriptor(hash);
    }
#endif

    // If we found the descriptor we're done.
    if (pGraph)
        return pGraph;
    
    // If BehaviorVar replacement fails, the hash is failListed for a period of time.
    // This handles a number of special cases for us. Many behaviors are not synced; the
    // game just ignores them and let's the multiple clients each handle it.
    // Examples are the "Root" behavior (not synced as a behavior, but handled),
    // or any number of lower-priority actors such as Wisps (location is always synced,
    // but otherwise behavior sync not needed).
    //
    // FailListed for minutes to avoid performance hit of constantly checking for
    // modded behavior. Only failListed for minutes to occasionally get log messages
    // so we can think about fixing it. Or the case of dynamic behavior and behavior
    // signature changes that might work later (yes, mods like that exist)
    if (failListed(hash))
        return nullptr;

    // Up to here the routine is pretty cheap, and WILL be called a bunch of times
    // if the only humanoid Actor is the Dragonborn/player in 1st person.
    // With that case filtered out, keep a counter to see if we need to defend against other 
    // cases (like a modded behavior where we CAN't find the signature var).
    if (invocations++ == 100)
        spdlog::warn("BehaviorVar::Patch: warning, more than 100 invocations, investigate why");
 
    spdlog::info("BehaviorVar::Patch: actor with formID {:x} with hash of {} has modded (or not synced) behavior", hexFormID, hash);

    // Get all animation variables for this actor, then create a reversemap to go from strings to animation enum.
    auto pDumpVar = apManager->DumpAnimationVariables(false);
    std::map<const std::string, const uint32_t> reversemap;
    spdlog::info("Known behavior variables for formID {:x}:", hexFormID);
    for (auto& item : pDumpVar)
    {
        spdlog::info("    {}:{}", item.first, item.second);
        reversemap.insert({static_cast<const std::string>(item.second), item.first});
    }

    // See if these animation variables include a signature for one of the replacers.
    // Since some of the original behaviors don't have a single variable name that is completely
    // unique, we allow a syntax of "!?signaturevar\(,!?signaturevar\)*". That is, an initial
    // (possibly negated) signaturevar, followed by zero or more comma separated additional
    // signaturevars (possibly negated).
    //
    // That's enough info to create a unique signature in the base game descriptors. At least,
    // it was at the time of writing.
    // 
    // O(N) loop, but this typically only gets executed a few times (once for each modded
    // creature TYPE, not each modded creature).
    //
    auto biter = behaviorPool.begin();
    bool found = false;

    for (; !found && biter < behaviorPool.end(); biter++)
    {
        auto tokens = tokenizeBehaviorSig(biter->signatureVar);
        found = tokens.size() > 0;

        for (auto titer = tokens.begin(); found && titer < tokens.end(); titer++)
        {
            bool negSig = titer->starts_with("!");
       
            if (negSig)
                found = reversemap.find(titer->substr(1)) == reversemap.end();
            else
                found = reversemap.find(*titer) != reversemap.end();
        }
        if (found)
            break;  // to prevent incrementing biter.
    }

    if (!found)
    {
        spdlog::warn("No original behavior found for behavior hash {:x} (found on formID {:x}), adding to fail list", hash, hexFormID);
        failList(hash);
        return nullptr;
    }
    spdlog::info("Found match, behavior hash {:x} (found on formID {:x}) has original behavior {} signature {}", hash, hexFormID, biter->creatureName, biter->signatureVar);

    // Build the set of BehaviorVar strings as sets (not vectors) to eliminate dups
    // Also, we want the set sorted, so these have to be std::sets. TiltedPhoques::Set
    // uses a hash map instead of a sorted tree, and I don't want to have to figure
    // out how to override that.
    std::set<uint32_t> boolVar;
    std::set<uint32_t> floatVar;
    std::set<uint32_t> intVar;

    // If we can find the original behavior that is being modded, 
    // get the descriptor and seed the behavior vars with it. 
    // That way mod developers only need to know what vars their
    // mod adds, they don't have to know what the STR devs picked
    const AnimationGraphDescriptor* pTmpGraph = nullptr;
    if (biter->orgHash && (pTmpGraph = AnimationGraphDescriptorManager::Get().GetDescriptor(biter->orgHash)))
    {
        seedAnimationVariables(biter->orgHash, pTmpGraph, reversemap, boolVar, floatVar, intVar); 
        spdlog::info("Original game descriptor with hash {} has {} boolean, {} float, {} integer behavior vars",
                     biter->orgHash, boolVar.size(), floatVar.size(), intVar.size());
    }

    // Check requested behavior vars for those that ARE legit
    // behavior vars for this Actor AND are not yet synced
    int foundCount = 0;
    for (auto& item : biter->syncBooleanVar)
    {
        bool found = reversemap.find(item) != reversemap.end();
        if (found && boolVar.find(reversemap[item]) == boolVar.end())
        {
            if (foundCount++ == 0)
                spdlog::info("Boolean variables added to sync:");
            boolVar.insert(reversemap[item]);
            spdlog::info("    {}", item);
        }
 
    }
    if (foundCount)
        spdlog::info("Now have {} boolVar descriptors after searching {} BehavivorVar strings", boolVar.size(), biter->syncBooleanVar.size());

    foundCount = 0;
    for (auto& item : biter->syncFloatVar)
    {
        bool found = reversemap.find(item) != reversemap.end();
        if (found && floatVar.find(reversemap[item]) == floatVar.end())
        {
            if (foundCount++ == 0)
                spdlog::info("Float variables added to sync:");
            floatVar.insert(reversemap[item]);
            spdlog::info("    {}", item);
        }
    }
    if (foundCount)
        spdlog::info("Now have {} floatVar descriptors after searching {} BehavivorVar strings", floatVar.size(), biter->syncFloatVar.size());

    foundCount = 0;
    for (auto& item : biter->syncIntegerVar)
    {
        bool found = reversemap.find(item) != reversemap.end();
        if (found && intVar.find(reversemap[item]) == intVar.end())
        {
            if (foundCount++ == 0)
                spdlog::info("Int variables added to sync:");
            intVar.insert(reversemap[item]);
            spdlog::info("    {}", item);
        }
    }
    if (foundCount)
        spdlog::info("Now have {} intVar descriptors after searching {} BehavivorVar strings", intVar.size(), biter->syncIntegerVar.size());

#if 0 // There are no limits, flex arrays now. Will come back and delete if it works.
     
    // Ensure we aren't over the limits. If we are, we won't update
    // the animation. Performance will be terrible unless we kill some of the logging
    // or keep track of failed signatures.
    std::string msgString;
    if (boolVar.size() > 64)
        msgString = "boolean";

    else if ((floatVar.size() + intVar.size()) > 64)
        msgString = "float+integer";

    if (msgString.size() > 0)
    {
        spdlog::error("Too many {} behavior vars to sync for actor, max is 64.", msgString);
        spdlog::error("Actor with formID {:x}, signature {}, original hash {} cannot be synced", 
                      hexFormID, iter->orgHash, iter->signatureVar);
        spdlog::error("Fail listing behavior hash {:x} found on formID {:x}", hash, hexFormID);
        failList(hash);
        return nullptr;
    }
 #endif

    // Reshape the (sorted, unique) sets to vectors
    TiltedPhoques::Vector<uint32_t> boolVector(boolVar.begin(), boolVar.end());
    TiltedPhoques::Vector<uint32_t> floatVector(floatVar.begin(), floatVar.end());
    TiltedPhoques::Vector<uint32_t> intVector(intVar.begin(), intVar.end());

    // Construct a new descriptor
    auto panimGraphDescriptor = new AnimationGraphDescriptor();

    // This is a bit grubby, but it IS a struct with public members.
    // Maybe the devs will help us out by agreeing to an additional constructor.
    panimGraphDescriptor->BooleanLookUpTable = boolVector;
    panimGraphDescriptor->FloatLookupTable = floatVector;
    panimGraphDescriptor->IntegerLookupTable = intVector;

    // Add the new graph to the known behavior graphs
    new AnimationGraphDescriptorManager::Builder(AnimationGraphDescriptorManager::Get(), hash, *panimGraphDescriptor);
    return AnimationGraphDescriptorManager::Get().GetDescriptor(hash);
}

// Check if the behavior hash is on the failed liist
boolean BehaviorVar::failListed(uint64_t hash)
{
    auto iter = failedBehaviors.find(hash);
    return iter != failedBehaviors.end() && std::chrono::steady_clock::now() < iter->second;
}

//Place the behavior hash on the failed list
void BehaviorVar::failList(uint64_t hash)
{
    failedBehaviors.insert_or_assign(hash, std::chrono::steady_clock::now() + FAILLIST_DURATION);
}

// Find all the subdirectories of behavior variables
std::vector<std::filesystem::path> BehaviorVar::loadDirs(const std::filesystem::path& acPATH)
{
    std::vector<std::filesystem::path> result;
    for (auto& p : std::filesystem::directory_iterator(acPATH))
        if (p.is_directory())
            result.push_back(p.path().string());
    return result;
}

BehaviorVar::Replacer* BehaviorVar::loadReplacerFromDir(std::filesystem::path aDir)
{
    // Enumerate all files in the directory and push bools, ints and floats 
    // to their respective vectors

    // The hashFile *__hash.txt file should contain the hashed signature of the ORIGINAL GAME
    // actor. That enables us to look up the synced animation variables selected by 
    // the STR devs and include them in the updated behavior/animation sync. Mod
    // devs don't have to know anything but what they've added, and this support for
    // Nemesis will work with more versions of the game.
    //
    // The signatureFile *__sig.txt should contain the name of a uniquely named variable for the
    // modded actor, that we can use to connect the actor to their modded behavior variables.
    // It's not actually sync'ed, just there so we can find the match. For example, currently
    // bSTRMaster is used for humanoid/Player actors, and bSTRDragon for dragon behavior.
    std::string hashFile;
    std::string signatureFile;
    std::vector<std::string> floatVarsFile;
    std::vector<std::string> intVarsFile;
    std::vector<std::string> boolVarsFile;
    
    for (auto& p : std::filesystem::directory_iterator(aDir))
    {
        std::string path = p.path().string();
        std::string base_filename = path.substr(path.find_last_of("/\\") + 1);

        spdlog::debug("base_path: {}", base_filename);

        if (base_filename.find("__sig.txt") != std::string::npos)
        {
            signatureFile = path;

            spdlog::debug("signature variable file: {}", signatureFile);
        }
        else if (base_filename.find("__hash.txt") != std::string::npos)
        {
            hashFile = path;

            spdlog::debug("hash file: {}", hashFile);
        }
        else if (base_filename.find("__float.txt") != std::string::npos)
        {
            floatVarsFile.push_back(path);

            spdlog::debug("float file: {}", path);
        }
        else if (base_filename.find("__int.txt") != std::string::npos)
        {
            intVarsFile.push_back(path);

            spdlog::debug("int file: {}", path);
        }
        else if (base_filename.find("__bool.txt") != std::string::npos)
        {
            boolVarsFile.push_back(path);

            spdlog::debug("bool file: {}", path);
        }
    }

    // Check that there is a signature file (an identifying variable)
    // When an Actor's behavior is modified, it's animation signature doesn't match
    // any of the base STR built-in descriptors. We try to match up on a distinguishing
    // animation variable added by the animation. But without such a distinquishing
    if (signatureFile == "")
        return nullptr;

    // Prepare reading files
    std::string sigVar;
    std::string creatureName = aDir.filename().string();
    std::vector<std::string> floatVar;
    std::vector<std::string> intVar;
    std::vector<std::string> boolVar;
    std::string tempString;  // Temp string

    std::ifstream fileSig(signatureFile);
    getline(fileSig, sigVar);  // grab the signature variable
    fileSig.close();
    erase_if(sigVar, isspace); // removes any inadvertant whitespace
    if (sigVar.size() == 0)
        return nullptr;
    spdlog::info("BehaviorVar::loadReplacerFromDir found {} with signature variable {}", creatureName, sigVar);

    // Check to see if there is a hash file. 
    // This is recommended, and shouild contain the ORIGINAL hash, 
    // before modding, of the behavior. It's not reasonable for a mod
    // developer to have to "know" the complete set of variables the STR 
    // devs have selected, so we'll want to get them from the original behaviors
    // via the original hash. If provided, we'll start with those vars and the
    // mod developer only lists the added ones they need.
    // 
    // The orginal version of this patch by Spvvd also included the "new" hash,
    // but that seems to be another thing a mod dev has no easy way to get their hands
    // on. So stripped off the need to provide the new hash, we'll just calculate it.
    // 
    uint64_t orgHash = 0;
    if (hashFile.size())
    {
        std::ifstream file(hashFile);
        getline(file, tempString);
        file.close();
        erase_if(tempString, isspace); // removes any inadvertant whitespace
        orgHash = std::strtoull(tempString.c_str(), nullptr, 10);
        spdlog::info("Replacer specifies original hash {} for {}", orgHash, creatureName);
    }

    // Build lists of variables to sync
    // Read float behavior variables
    spdlog::debug("reading float var");
    for (auto& item : floatVarsFile)
    {
        std::ifstream file(item);
        while (std::getline(file, tempString))
        {
            floatVar.push_back(tempString);

            spdlog::debug("    " + tempString);
        }
        file.close();
    }

    // Read integer behavior variables
    spdlog::debug("reading int vars");
    for (auto& item : intVarsFile)
    {
        std::ifstream file(item);
        while (std::getline(file, tempString))
        {
            intVar.push_back(tempString);

            spdlog::debug("    " + tempString);
        }
        file.close();
    }

    // Read boolean behavior variables
    spdlog::debug("reading bool vars");
    for (auto& item : boolVarsFile)
    {
        std::ifstream file(item);
        while (std::getline(file, tempString))
        {
            boolVar.push_back(tempString);

            spdlog::debug("     " + tempString);
        }
        file.close();
    }

    // Create the replacer
    Replacer* result = new Replacer();

    result->orgHash = orgHash;
    result->signatureVar   = sigVar;
    result->creatureName   = creatureName;
    result->syncBooleanVar = boolVar;
    result->syncFloatVar   = floatVar;
    result->syncIntegerVar = intVar;

    return result;
}

void BehaviorVar::Init()
{
    // Initialize original (base STR) behaviors so we can search them.
    BehaviorOrig::BehaviorOrigInit();
    
    // Check if the behaviors folder exists
    std::filesystem::path pBehaviorsPath;
    pBehaviorsPath = launcher::GetLaunchContext()->gamePath / L"Data" / L"SkyrimTogetherRebornBehaviors";

    if (!std::filesystem::is_directory(pBehaviorsPath))
        return;

    auto behaviorDirs = loadDirs(pBehaviorsPath);

    for (auto& dir : behaviorDirs)
    {
        Replacer* sig = loadReplacerFromDir(dir);
        if (sig)
        {
            behaviorPool.push_back(*sig);
        }
    }
}

void BehaviorVar::Debug()
{
    auto descriptors = AnimationGraphDescriptorManager::Get().GetDescriptors();

    // Descriptor hash dump
    spdlog::info("Dumping descriptor hashes:");
    for (const auto& pair : descriptors)
    {
        spdlog::info("Hash: {}", pair.first);
    }

    // Loaded behavior dump
    spdlog::info("Dumping behavior sigs:");
    for (const auto& sig : behaviorPool)
    {
        spdlog::info("orgHash: {} => newHash: {}", sig.orgHash, sig.newHash);

        // bools
        spdlog::info("bools:");
        for (const auto& var : sig.syncBooleanVar)
        {
            spdlog::info(var);
        }
        // bools
        spdlog::info("floats:");
        for (const auto& var : sig.syncFloatVar)
        {
            spdlog::info(var);
        }
        // bools
        spdlog::info("integers:");
        for (const auto& var : sig.syncIntegerVar)
        {
            spdlog::info(var);
        }
    }
}

#endif MODDED_BEHAVIOR_COMPATIBILITY
