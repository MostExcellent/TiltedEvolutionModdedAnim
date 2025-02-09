#include "LocalAnimationComponent.h"

GraphListenerPair::~GraphListenerPair()
{
    if (listener != nullptr)
    {
        if (graph != nullptr)
        {
            spdlog::info("UnRegistering listener on graph of actor {}", graph->holder->formID);
            graph->eventDispatcher.UnRegisterSink(listener);
        }
        else
        {
            delete listener;
        }
    }
}

bool GraphListenerPair::Set(BShkbAnimationGraph* in_graph, AnimEventListener* in_listener)
{
    if (in_graph != nullptr && in_listener != nullptr)
    {
        graph = in_graph;
        listener = in_listener;
        return true;
    }
    return false;
}

// LocalAnimationComponent::~LocalAnimationComponent()
// {
//         LocalAnimEventListeners.clear();
// }
