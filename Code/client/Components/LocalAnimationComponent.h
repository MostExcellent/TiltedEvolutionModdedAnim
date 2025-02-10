#pragma once

#ifndef TP_INTERNAL_COMPONENTS_GUARD
#error Include Components.h instead
#endif

#include "Games/Animation/AnimEventListener.h"
#include "Havok/BShkbAnimationGraph.h"

#include <Structs/ActionEvent.h>

struct GraphListenerPair
{
    GraphListenerPair() = default;

    // Delete copy operations
    GraphListenerPair(const GraphListenerPair&) = delete;
    GraphListenerPair& operator=(const GraphListenerPair&) = delete;

    // Define move operations
    GraphListenerPair(GraphListenerPair&& other) noexcept
        : graph(other.graph)
        , listener(std::move(other.listener))
    {
        other.graph = nullptr;
    }

    GraphListenerPair& operator=(GraphListenerPair&& other) noexcept
    {
        if (this != &other)
        {
            // Clean up existing registration
            if (listener && graph)
            {
                graph->eventDispatcher.UnRegisterSink(listener.get());
            }
            
            graph = other.graph;
            listener = std::move(other.listener);
            other.graph = nullptr;
        }
        return *this;
    }

    ~GraphListenerPair();

    bool Set(BShkbAnimationGraph* in_graph, AnimEventListener* in_listener);

    BShkbAnimationGraph* graph = nullptr;
    UniquePtr<AnimEventListener> listener = nullptr;
    
};

struct LocalAnimationComponent
{
    ~LocalAnimationComponent() = default;
    
    Vector<ActionEvent> Actions;
    ActionEvent LastProcessedAction;
    GraphListenerPair LocalAnimEventListeners;

    [[nodiscard]] Outcome<ActionEvent, bool> GetLatestAction() const noexcept
    {
        if (Actions.empty())
            return false;

        return Actions[Actions.size() - 1];
    }

    void Append(const ActionEvent& acEvent) noexcept { Actions.push_back(acEvent); }
};
