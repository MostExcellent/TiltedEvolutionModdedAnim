#pragma once
#include "Misc/BSFixedString.h"

struct BSAnimationGraphEvent
{
    const BSFixedString  tag;      // 00
    const TESObjectREFR* holder;   // 08
    const BSFixedString  payload;  // 10
};
static_assert(sizeof(BSAnimationGraphEvent) == 0x18);
