#include "LocalAnimationComponent.h"

GraphListenerPair::~GraphListenerPair()
{
    if (listener != nullptr && graph != nullptr)
    {
        graph->eventDispatcher.UnRegisterSink(listener.get());
    }
}

bool GraphListenerPair::Set(BShkbAnimationGraph* in_graph, AnimEventListener* in_listener)
{
    if (in_graph != nullptr && in_listener != nullptr)
    {
        graph = in_graph;
        listener = TiltedPhoques::MakeUnique<AnimEventListener>(in_listener);
        return true;
    }
    return false;
}
