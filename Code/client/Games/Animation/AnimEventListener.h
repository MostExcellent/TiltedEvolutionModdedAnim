#pragma once

#include "Games/Events.h"

#include <Games/Animation/BSAnimationGraphEvent.h>

struct AnimEventListener : BSTEventSink<BSAnimationGraphEvent>
{
    BSTEventResult OnEvent(const BSAnimationGraphEvent* event, const EventDispatcher<BSAnimationGraphEvent>* sender) override;
};
