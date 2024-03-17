#pragma once
#include "Message.h"
#include <Structs/GameId.h>
#include <server/Game/Player.h>

// Is a server message the correct approach?
struct NotifyRemoveSpell final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyRemoveSpell;

    NotifyRemoveSpell() : ServerMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;

    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifyRemoveSpell& acRhs) const noexcept
    {
        return GetOpcode() == acRhs.GetOpcode() && TargetId == acRhs.TargetId && SpellId == acRhs.SpellId;
    }

    GameId TargetId{};
    //Player& Target;
    GameId SpellId{};
};
