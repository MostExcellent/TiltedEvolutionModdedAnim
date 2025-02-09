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
    // GraphListenerPair(BShkbAnimationGraph* in_graph, AnimEventListener* in_listener)
    //     : graph(in_graph), listener(in_listener) {}
    ~GraphListenerPair();

    bool Set(BShkbAnimationGraph* in_graph, AnimEventListener* in_listener);

    BShkbAnimationGraph* graph = nullptr;
    AnimEventListener* listener = nullptr;
    
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
