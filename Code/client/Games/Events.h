#pragma once

#include <Events/EventDispatcher.h>

enum class BSTEventResult
{
    kOk,
    kAbort
};

template <class T> struct BSTEventSink
{
    virtual ~BSTEventSink() = default;
    virtual BSTEventResult OnEvent(const T* apEvent, const EventDispatcher<T>* apSender) { return BSTEventResult::kOk; }
};
static_assert(sizeof(BSTEventSink<void>) == 0x8);

struct BSTEventSinkVoidPtr : BSTEventSink<void*>
{
    ~BSTEventSinkVoidPtr() override {}
    BSTEventResult OnEvent(void* const* apEvent, const EventDispatcher<void*>* apSender) override { return BSTEventResult::kOk; }
};
