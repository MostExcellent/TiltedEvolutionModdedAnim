#include <Services/MagicService.h>

#include <GameServer.h>
#include <World.h>

#include <Messages/SpellCastRequest.h>
#include <Messages/InterruptCastRequest.h>
#include <Messages/AddTargetRequest.h>

#include <Messages/NotifySpellCast.h>
#include <Messages/NotifyInterruptCast.h>
#include <Messages/NotifyAddTarget.h>
#include <Messages/NotifyRemoveSpell.h>

MagicService::MagicService(World& aWorld, entt::dispatcher& aDispatcher) noexcept
    : m_world(aWorld)
{
    m_spellCastConnection = aDispatcher.sink<PacketEvent<SpellCastRequest>>().connect<&MagicService::OnSpellCastRequest>(this);
    m_interruptCastConnection = aDispatcher.sink<PacketEvent<InterruptCastRequest>>().connect<&MagicService::OnInterruptCastRequest>(this);
    m_addTargetConnection = aDispatcher.sink<PacketEvent<AddTargetRequest>>().connect<&MagicService::OnAddTargetRequest>(this);
    m_removeSpellConnection = aDispatcher.sink<PacketEvent<RemoveSpellRequest>>().connect<&MagicService::OnRemoveSpellRequest>(this);
}

void MagicService::OnSpellCastRequest(const PacketEvent<SpellCastRequest>& acMessage) const noexcept
{
    auto& message = acMessage.Packet;

    NotifySpellCast notify;
    notify.CasterId = message.CasterId;
    notify.SpellFormId = message.SpellFormId;
    notify.CastingSource = message.CastingSource;
    notify.IsDualCasting = message.IsDualCasting;
    notify.DesiredTarget = message.DesiredTarget;

    const auto entity = static_cast<entt::entity>(message.CasterId);
    if (!GameServer::Get()->SendToPlayersInRange(notify, entity, acMessage.GetSender()))
        spdlog::error("{}: SendToPlayersInRange failed", __FUNCTION__);
}

void MagicService::OnInterruptCastRequest(const PacketEvent<InterruptCastRequest>& acMessage) const noexcept
{
    auto& message = acMessage.Packet;

    NotifyInterruptCast notify;
    notify.CasterId = message.CasterId;
    notify.CastingSource = message.CastingSource;

    const auto entity = static_cast<entt::entity>(message.CasterId);
    if (!GameServer::Get()->SendToPlayersInRange(notify, entity, acMessage.GetSender()))
        spdlog::error("{}: SendToPlayersInRange failed", __FUNCTION__);
}

void MagicService::OnAddTargetRequest(const PacketEvent<AddTargetRequest>& acMessage) const noexcept
{
    auto& message = acMessage.Packet;

    NotifyAddTarget notify;
    notify.TargetId = message.TargetId;
    notify.SpellId = message.SpellId;
    notify.EffectId = message.EffectId;
    notify.Magnitude = message.Magnitude;
    notify.IsDualCasting = message.IsDualCasting;
    notify.ApplyHealPerkBonus = message.ApplyHealPerkBonus;
    notify.ApplyStaminaPerkBonus = message.ApplyStaminaPerkBonus;

    const auto entity = static_cast<entt::entity>(message.TargetId);
    if (!GameServer::Get()->SendToPlayersInRange(notify, entity, acMessage.GetSender()))
        spdlog::error("{}: SendToPlayersInRange failed", __FUNCTION__);
}

void MagicService::OnRemoveSpellRequest(const PacketEvent<RemoveSpellRequest>& acMessage) const noexcept
{
    auto& message = acMessage.Packet;

    NotifyRemoveSpell notify;
    // I don't think we need targetId since that can be inferred from the sender
    //notify.TargetId = acMessage.GetSender();
    notify.SpellId = message.SpellId;
    if (acMessage.GetSender()->GetCharacter().has_value())
    {
        const auto entity = static_cast<entt::entity>(acMessage.GetSender()->GetCharacter().value());
        //TargetID is entity as uint32_t
        notify.TargetId = World::ToInteger(entity);
        if (!GameServer::Get()->SendToPlayersInRange(notify, entity, acMessage.GetSender()))
            spdlog::error("{}: SendToPlayersInRange failed", __FUNCTION__);
    }
    else
    {
        spdlog::error("{}: Sender has no character", __FUNCTION__);
    }
}
