#include "AnimEventListener.h"

BSTEventResult AnimEventListener::OnEvent(const BSAnimationGraphEvent* event, const EventDispatcher<BSAnimationGraphEvent>* sender)
{
    // log the event, tag, holder ref id, payload
    if (Actor* holderActor = Cast<Actor>(event->holder))
        spdlog::info("AnimationGraphEvent - tag: {}, payload:{}, holder ID: {}",
            event->tag.AsAscii(), event->payload.AsAscii(), holderActor->formID);
    return BSTEventSink::OnEvent(event, sender);
}
