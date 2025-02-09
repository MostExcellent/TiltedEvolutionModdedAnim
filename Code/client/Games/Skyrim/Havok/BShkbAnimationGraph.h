#pragma once

#include <Havok/hkbCharacter.h>

struct BSFixedString;
struct BShkbHkxDB;
struct hkbBehaviorGraph;
struct bhkWorldM;

struct BShkbAnimationGraph
{
    virtual ~BShkbAnimationGraph(){}; // 00

    uint8_t pad8[0x68 - 0x8];
    EventDispatcher<BSAnimationGraphEvent> eventDispatcher; // 068
    hkbCharacter character; // 0C0
    uint8_t pad160[0x200 - (0xC0 + sizeof(hkbCharacter))];
    BShkbHkxDB* hkxDB;
    hkbBehaviorGraph* behaviorGraph; // 208
    Actor* holder; // 210
    uint8_t pad218[0x238 - 0x218];
    bhkWorldM* hkWorldM; // 238
    uint16_t numAnimBones; // 240
    uint8_t pad240[0x250 - 0x242];

    // Re do not use
    bool ReSendEvent(BSFixedString* apEventName);
};
static_assert(sizeof(BShkbAnimationGraph) == 0x250);
