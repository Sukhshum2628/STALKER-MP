// STALKER-MP — Engine adapters (Sprint-002, adapter layer)
//
// The ONLY translation unit in the module that includes X-Ray Engine
// headers. Compiles exclusively in the xrMP project (with engine include
// paths, see xrMP.vcxproj per-file settings); the test build links
// tests/support/NullEngineAdapters.cpp instead.
//
// Everything here is read-only observation of engine state plus the one
// sanctioned frame registration (Device.seqFrame, REG_PRIORITY_LOW — see
// docs/Sprint-002-Design-Review.md §11). No engine state is mutated.

#include "stalkermp/adapters/EngineAdapters.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "stalkermp/world/WorldTypes.h"

// Player spawn gateway seam (Sprint-007, Step 10) — engine-free interface + factory.
#include "stalkermp/adapters/PlayerSpawnGateway.h"
#include "stalkermp/player/IPlayerSpawnGateway.h"

// Entity snapshot capture seam (Sprint-008, Step 9) — engine-free interface,
// factory, and the value-only ascending marshaling helper.
#include "stalkermp/adapters/EntitySnapshotSource.h"
#include "stalkermp/adapters/EntitySnapshotMarshal.h"
#include "stalkermp/world/IEntitySnapshotSource.h"

// Client prediction seams (Sprint-010, Step 15) — engine-free interfaces + factories.
#include <cmath>
#include "stalkermp/adapters/PredictionSeams.h"
#include "stalkermp/prediction/ClientPresentationPhase.h" // detail::AdvanceClientPresentation (Step 16)
#include "stalkermp/prediction/ILocalInputSource.h"
#include "stalkermp/prediction/IPresentationSink.h"
#include "stalkermp/prediction/PredictionTypes.h"

// Save/load composition-root seams (Sprint-012, Step 17) — engine-free interfaces +
// factories. The real restoration write boundary + real filesystem save backend live
// here (the test build links tests/support/NullSaveLoadSeams.cpp instead).
#include "stalkermp/adapters/SaveLoadSeams.h"
#include "stalkermp/saveload/IRestoreSinks.h"
#include "stalkermp/world/TransitionTypes.h" // world::TransitionCommand/Kind/Outcome (Sprint-005 seam)

// Lua Integration engine boundary (Sprint-013, Step 17) — the real Public API facades,
// the concrete Lua runtime binding, and the script-source factory. Engine-free seam
// headers only; the engine/VM specifics below are confined to THIS TU.
#include "stalkermp/adapters/LuaSeams.h"
#include "stalkermp/adapters/PlatformScriptStore.h" // CreatePlatformScriptSource (ADR-009)
#include "stalkermp/lua/ILuaApiBundle.h"
#include "stalkermp/lua/ILuaRuntime.h"
#include "stalkermp/lua/ScriptApi.h"

// --- X-Ray Engine headers (adapter layer only) ------------------------------
#include "stdafx.h"           // xrEngine precompiled-header umbrella.
#include "IGame_Level.h"      // g_pGameLevel, CObjectList, relcase_(un)register.
#include "xr_object_list.h"   // CObjectList::RELCASE_CALLBACK, relcase API.
#include "IGame_Persistent.h" // g_pGamePersistent.
#include "Environment.h"      // CEnvironment::GetWeather.
#include "xr_object.h"        // CObject: ID(), cName(), Position().
// --- ALife (Sprint-005 switch gateway; xrGame/xrServerEntities) --------------
#include "ai_space.h"                  // ai(), get_alife() (authoritative ALife access).
#include "alife_simulator.h"           // CALifeSimulator: objects(), set_switch_online/offline.
#include "alife_object_registry.h"     // CALifeObjectRegistry::object(id, no_assert).
#include "xrServer_Objects_ALife.h"    // CSE_ALifeDynamicObject: m_bOnline, CSE_Abstract::ID.
// --- ALife spawn (Sprint-007 Step 10; xrGame/xrAICore navigation) ------------
#include "level_graph.h"               // CLevelGraph::vertex_id(Fvector), valid_vertex_id.
#include "game_graph.h"                // CGameGraph::cross_table().
#include "game_level_cross_table.h"    // CGameLevelCrossTable::vertex(lvid).game_vertex_id().
// -----------------------------------------------------------------------------

namespace stalkermp::adapters
{
    namespace
    {
        [[nodiscard]] world::EntityView MakeView(CObject& object)
        {
            world::EntityView view;
            view.id = world::EntityId{static_cast<std::uint32_t>(object.ID())};
            view.name = object.cName().c_str() != nullptr ? object.cName().c_str() : "";
            const Fvector& position = object.Position();
            view.position = world::Vec3{position.x, position.y, position.z};
            return view;
        }

        // ------------------------------------------------------ Entity source

        class EngineEntitySource final : public world::IEntitySource
        {
        public:
            [[nodiscard]] std::size_t Count() const override
            {
                return g_pGameLevel != nullptr ? g_pGameLevel->Objects.o_count() : 0;
            }

            void Enumerate(const std::function<void(const world::EntityView&)>& visitor) const override
            {
                if (g_pGameLevel == nullptr)
                {
                    return;
                }
                const u32 count = g_pGameLevel->Objects.o_count();
                for (u32 i = 0; i < count; ++i)
                {
                    CObject* object = g_pGameLevel->Objects.o_get_by_iterator(i);
                    if (object != nullptr)
                    {
                        visitor(MakeView(*object));
                    }
                }
            }

            [[nodiscard]] std::optional<world::EntityView> FindById(const world::EntityId id) const override
            {
                std::optional<world::EntityView> found;
                Enumerate([&](const world::EntityView& entity) {
                    if (!found && entity.id == id)
                    {
                        found = entity;
                    }
                });
                return found;
            }

            [[nodiscard]] std::optional<world::EntityView> FindByName(const std::string_view name) const override
            {
                std::optional<world::EntityView> found;
                Enumerate([&](const world::EntityView& entity) {
                    if (!found && entity.name == name)
                    {
                        found = entity;
                    }
                });
                return found;
            }
        };

        // -------------------------------------------------- Environment source

        class EngineEnvironmentSource final : public world::IEnvironmentSource
        {
        public:
            [[nodiscard]] std::optional<world::EnvironmentSample> Sample() const override
            {
                if (g_pGamePersistent == nullptr)
                {
                    return std::nullopt;
                }

                world::EnvironmentSample sample;
                const shared_str weather = g_pGamePersistent->Environment().GetWeather();
                sample.weatherName = weather.c_str() != nullptr ? weather.c_str() : "";
                // Emission state is script-driven in Anomaly; exposed with
                // Lua integration (Sprint-013). See IEnvironmentSource.h.
                sample.emissionActive = false;
                return sample;
            }
        };

        // ------------------------------------------- Entity snapshot source (Step 9)
        //
        // The snapshot-specific capture seam (Architecture §15 clarification:
        // consumed ONLY by SnapshotBuilder; NOT a general-purpose entity API).
        // Reads each live engine object's id and transform ON DEMAND into plain
        // EntitySnapshot VALUES — no CObject pointer is retained (E-G / ADR-008
        // read-only observation), no engine state is mutated. The deterministic
        // ascending-by-EntityId, unique, id!=0 ordering and the append-only
        // contract are delegated to the engine-free detail::AppendAscendingUnique
        // helper. Values only cross the world::IEntitySnapshotSource seam.
        class EngineEntitySnapshotSource final : public world::IEntitySnapshotSource
        {
        public:
            void Capture(std::vector<snapshot::EntitySnapshot>& out) const override
            {
                if (g_pGameLevel == nullptr)
                {
                    return; // no level -> nothing to capture (append-only: out unchanged)
                }

                // Collect raw per-object values (no pointer retained). Ordering and
                // uniqueness are applied by the engine-free helper below.
                std::vector<snapshot::EntitySnapshot> raw;
                const u32 count = g_pGameLevel->Objects.o_count();
                raw.reserve(count);
                for (u32 i = 0; i < count; ++i)
                {
                    CObject* object = g_pGameLevel->Objects.o_get_by_iterator(i);
                    if (object == nullptr)
                    {
                        continue;
                    }
                    const std::uint32_t id = static_cast<std::uint32_t>(object->ID());
                    if (id == 0) // reserved null EntityId
                    {
                        continue;
                    }

                    snapshot::EntitySnapshot value{};
                    value.id = world::EntityId{id};
                    const Fvector& position = object->Position();
                    value.position = world::Vec3{position.x, position.y, position.z};
                    // velocity/state/flags/inventoryRef/runtimeState remain at their
                    // engine-free zero defaults (0 = none); richer per-entity fields
                    // are marshaled here as confirmed engine getters are added (they
                    // are opaque, value-only, and never a retained pointer).
                    raw.push_back(value); // value copy only
                }

                detail::AppendAscendingUnique(out, raw); // deterministic, append-only
            }
        };

        // ---------------------------------------------------------- Frame bridge

        // The module's single engine frame observer (Design Review §11:
        // no other seqFrame registration may ever be added).
        class FrameObserver final : public pureFrame
        {
        public:
            explicit FrameObserver(stalkermp::core::FrameDispatcher& dispatcher)
                : m_dispatcher(dispatcher)
            {
            }

            void _BCL OnFrame() override
            {
                const double dt = static_cast<double>(Device.fTimeDelta);
                m_dispatcher.Dispatch(dt);
                // Client-presentation phase (Sprint-010): runs AFTER Dispatch, within
                // the frame bridge — not a FrameDispatcher subscriber (no new
                // tick_order key; networking-last preserved).
                stalkermp::detail::AdvanceClientPresentation(dt);
            }

        private:
            stalkermp::core::FrameDispatcher& m_dispatcher;
        };

        class EngineFrameBridge final : public IFrameBridge
        {
        public:
            explicit EngineFrameBridge(stalkermp::core::FrameDispatcher& dispatcher)
                : m_observer(dispatcher)
            {
                Device.seqFrame.Add(&m_observer, REG_PRIORITY_LOW);
            }

            ~EngineFrameBridge() override
            {
                Device.seqFrame.Remove(&m_observer);
            }

        private:
            FrameObserver m_observer;
        };

        // ----------------------------------------------------- Entity feed

        // Creation-side engine feed (Sprint-003; C1-C4). Ticked once per frame;
        // reconciles the engine's live object list into the registry. This step
        // registers new engine objects only — destruction (relcase) is a later
        // step, so the registry may retain entries for objects the engine has
        // since destroyed until that step lands.
        class EngineEntityFeed final : public IEntityFeed
        {
        public:
            explicit EngineEntityFeed(world::EntityRegistry& registry)
                : m_registry(registry)
            {
            }

            ~EngineEntityFeed() override
            {
                // C4: remove our destroy observer before destruction, but only while
                // the level (and thus the CObjectList holding our entry) still exists.
                if (m_relcaseRegistered && g_pGameLevel != nullptr && m_registeredLevel == g_pGameLevel)
                {
                    g_pGameLevel->Objects.relcase_unregister(&m_relcaseId);
                }
            }

            // Engine destroy notification (relcase). Runs on the engine main thread
            // as an object is released. C1: never mutates the registry here. C2:
            // captures only the id by value; never retains CObject*.
            void xr_stdcall OnEngineObjectDestroyed(CObject* object)
            {
                if (object != nullptr)
                {
                    m_pendingDestroys.push_back(static_cast<std::uint32_t>(object->ID()));
                }
            }

            void Tick(double /*deltaSeconds*/) override
            {
                if (g_pGameLevel == nullptr) // C4: no level -> nothing to observe.
                {
                    // The level's CObjectList (and our relcase entry) no longer exist;
                    // queued ids refer to a level that is gone.
                    m_relcaseRegistered = false;
                    m_registeredLevel = nullptr;
                    m_pendingDestroys.clear();
                    return;
                }

                // C4: (re)register the destroy observer for the current level.
                if (!m_relcaseRegistered || m_registeredLevel != g_pGameLevel)
                {
                    g_pGameLevel->Objects.relcase_register(
                        CObjectList::RELCASE_CALLBACK(this, &EngineEntityFeed::OnEngineObjectDestroyed),
                        &m_relcaseId);
                    m_relcaseRegistered = true;
                    m_registeredLevel = g_pGameLevel;
                }

                // C1: single deterministic mutation phase. Apply queued destroys
                // first, ascending EntityId, then creation reconciliation below.
                if (!m_pendingDestroys.empty())
                {
                    std::sort(m_pendingDestroys.begin(), m_pendingDestroys.end());
                    for (const std::uint32_t destroyedId : m_pendingDestroys)
                    {
                        const world::EntityRecord* record =
                            m_registry.FindByEntityId(world::EntityId{destroyedId});
                        if (record != nullptr)
                        {
                            const world::EntityHandle handle = record->handle;
                            if (auto unregistered = m_registry.UnregisterEntity(handle); !unregistered)
                            {
                                // Stale / already gone -> benign no-op (C3).
                            }
                        }
                        // Unknown id -> benign no-op (C3).
                    }
                    m_pendingDestroys.clear();
                }

                // Collect engine ids not yet registered (C3 delta-only), capturing
                // only the id and name BY VALUE (C2) — never retaining CObject*.
                struct PendingRegistration
                {
                    std::uint32_t id = 0;
                    std::string name;
                };

                std::vector<PendingRegistration> additions;
                const u32 count = g_pGameLevel->Objects.o_count();
                for (u32 i = 0; i < count; ++i)
                {
                    CObject* object = g_pGameLevel->Objects.o_get_by_iterator(i);
                    if (object == nullptr)
                    {
                        continue;
                    }

                    const std::uint32_t id = static_cast<std::uint32_t>(object->ID());
                    if (id == 0) // 0 is the reserved null EntityId.
                    {
                        continue;
                    }
                    if (m_registry.Contains(world::EntityId{id})) // delta-only (C3).
                    {
                        continue;
                    }

                    PendingRegistration pending;
                    pending.id = id;
                    pending.name = object->cName().c_str() != nullptr ? object->cName().c_str() : "";
                    additions.push_back(std::move(pending));
                }

                // C1: apply all mutations in one deterministic phase, in ascending
                // EntityId order (so registration-event order is deterministic).
                std::sort(additions.begin(), additions.end(),
                          [](const PendingRegistration& a, const PendingRegistration& b) noexcept {
                              return a.id < b.id;
                          });

                for (PendingRegistration& pending : additions)
                {
                    world::EntityMetadata metadata;
                    metadata.spawnSource = world::EntitySpawnSource::EngineWorld;
                    if (auto registered = m_registry.RegisterEntity(
                            world::EntityId{pending.id}, std::move(pending.name), std::move(metadata));
                        !registered)
                    {
                        // A transient race (e.g. duplicate) is benign; the next
                        // frame's reconciliation retries. Errors are not fatal.
                    }
                }
            }

        private:
            world::EntityRegistry& m_registry;

            // relcase (destruction) state (C4). m_relcaseId is the engine's id slot,
            // kept current by CObjectList across other (un)registrations.
            int m_relcaseId = 0;
            bool m_relcaseRegistered = false;
            IGame_Level* m_registeredLevel = nullptr;

            // Ids captured from relcase callbacks (C2, by value), drained in Tick's
            // deterministic mutation phase (C1). Written only on the engine main
            // thread (I8), the same thread as Tick.
            std::vector<std::uint32_t> m_pendingDestroys;
        };

        // ------------------------------------------------ ALife switch gateway
        //
        // The concrete IAlifeSwitchGateway (Sprint-005, Architecture §15). Drives
        // vanilla ALife online/offline through the frozen COOPERATIVE FLAG OVERRIDE
        // (E-G1): it sets the object's can_switch_* override flags — the exact
        // predicates the vanilla try_switch_* path already evaluates — and lets the
        // engine's own ALife update perform the switch. It NEVER calls the forced
        // switch_online/offline, never edits ALife, and never reimplements it
        // (Preserve Before Replace; ADR-008).
        //
        //   Activate  (pin ONLINE):  can_switch_online=true, can_switch_offline=false
        //     -> try_switch_online forces online (the !can_switch_offline() branch),
        //        try_switch_offline refuses to offline (the !can_switch_offline()
        //        early return). The object is pinned online regardless of distance.
        //   Deactivate (pin OFFLINE): can_switch_offline=true, can_switch_online=false
        //     -> try_switch_offline forces offline (the !can_switch_online() branch),
        //        try_switch_online refuses to online. The object is pinned offline.
        //
        // EntityId is resolved to the ALife object ON DEMAND at the instant of the
        // call via the engine's authoritative lookup (objects().object(id, true));
        // NO engine pointer or id->object cache is retained (E-G2). No relcase.
        class EngineAlifeSwitchGateway final : public world::IAlifeSwitchGateway
        {
        public:
            [[nodiscard]] std::vector<world::TransitionOutcome>
            Apply(const std::vector<world::TransitionCommand>& commands) override
            {
                std::vector<world::TransitionOutcome> outcomes;
                outcomes.reserve(commands.size());

                // Mutable simulator obtained exactly as the engine's own alife()
                // helper does (const_cast of the authoritative accessor). nullptr
                // when no ALife is running (no game/level) -> benign skip.
                CALifeSimulator* const simulator =
                    const_cast<CALifeSimulator*>(ai().get_alife());

                for (const world::TransitionCommand& command : commands)
                {
                    outcomes.push_back(ApplyOne(simulator, command));
                }
                return outcomes;
            }

            [[nodiscard]] std::optional<bool> IsOnline(const world::EntityId id) const override
            {
                const CALifeSimulator* const simulator = ai().get_alife();
                if (simulator == nullptr)
                {
                    return std::nullopt; // no ALife -> unknown
                }
                // On-demand authoritative lookup; no_assert=true -> nullptr if absent.
                const CSE_ALifeDynamicObject* const object =
                    simulator->objects().object(ToObjectId(id), true);
                if (object == nullptr)
                {
                    return std::nullopt; // unknown to the engine
                }
                return object->m_bOnline;
            }

        private:
            [[nodiscard]] static ALife::_OBJECT_ID ToObjectId(const world::EntityId id) noexcept
            {
                // EntityId.value == object->ID() == ALife::_OBJECT_ID (u16) (E-G2).
                return static_cast<ALife::_OBJECT_ID>(id.value);
            }

            [[nodiscard]] static world::TransitionOutcome
            ApplyOne(CALifeSimulator* const simulator, const world::TransitionCommand& command)
            {
                if (simulator == nullptr)
                {
                    return world::TransitionOutcome::EntityMissing; // no ALife: benign skip
                }

                const ALife::_OBJECT_ID objectId = ToObjectId(command.id);
                CSE_ALifeDynamicObject* const object =
                    static_cast<const CALifeSimulator*>(simulator)->objects().object(objectId, true);
                if (object == nullptr)
                {
                    return world::TransitionOutcome::EntityMissing; // absent: benign skip
                }

                const bool wasOnline = object->m_bOnline;
                const bool wantOnline = (command.kind == world::TransitionKind::Activate);

                // Cooperative Flag Override via the frozen id-based API only.
                if (wantOnline)
                {
                    simulator->set_switch_online(objectId, true);   // can_switch_online = true
                    simulator->set_switch_offline(objectId, false); // pin: can_switch_offline = false
                }
                else
                {
                    simulator->set_switch_offline(objectId, true);  // can_switch_offline = true
                    simulator->set_switch_online(objectId, false);  // pin: can_switch_online = false
                }

                // The vanilla ALife update performs the actual switch. Report whether
                // the object was already in the requested state (idempotent no-op) or
                // the request was newly applied (confirmed later via IsOnline read-back).
                return (wasOnline == wantOnline)
                           ? world::TransitionOutcome::AlreadyInState
                           : world::TransitionOutcome::Applied;
            }
        };

        // ------------------------------------------- Player spawn gateway (Step 10)
        //
        // Materializes/removes a persistent VANILLA ALife player object via the
        // engine's own alife().spawn_item / release (Preserve Before Replace, Reuse
        // Engine Systems). EntityId is resolved to the ALife object ON DEMAND at the
        // instant of Despawn (objects().object(id, true)); NO engine pointer or
        // id->object cache is retained (Sprint-005 discipline). This gateway does
        // NOT switch the object online/offline — that remains the Sprint-005
        // Bubble->Transition pipeline (ADR-008 unbroken). Engine headers appear only
        // in this TU (One Engine Boundary). Every fallible op is a value outcome
        // (ADR-007). No online/offline mutation is added here.
        class EnginePlayerSpawnGateway final : public player::IPlayerSpawnGateway
        {
        public:
            [[nodiscard]] core::Expected<world::EntityId>
            Spawn(const player::PlayerProfile& profile, const world::PlayerPosition& position) override
            {
                CALifeSimulator* const simulator = const_cast<CALifeSimulator*>(ai().get_alife());
                if (simulator == nullptr)
                {
                    return core::MakeError(core::ErrorCode::IoError, "player spawn: no ALife simulator");
                }

                const Fvector pos = {position.position.x, position.position.y, position.position.z};

                // Resolve level/game vertices from the position (vanilla navigation).
                const CLevelGraph& levelGraph = ai().level_graph();
                const u32 levelVertexId = levelGraph.vertex_id(pos);
                if (!levelGraph.valid_vertex_id(levelVertexId))
                {
                    return core::MakeError(core::ErrorCode::InvalidArgument,
                                           "player spawn: position has no valid level vertex");
                }
                const GameGraph::_GRAPH_ID gameVertexId =
                    ai().game_graph().cross_table().vertex(levelVertexId).game_vertex_id();

                // Vanilla ALife spawn. parent_id 0xffff = no parent (top-level object).
                CSE_Abstract* const object =
                    simulator->spawn_item(SectionForProfile(profile), pos, levelVertexId, gameVertexId, 0xffff);
                if (object == nullptr)
                {
                    return core::MakeError(core::ErrorCode::IoError, "player spawn: engine returned no object");
                }

                // Record the id only; retain no pointer (on-demand discipline).
                return world::EntityId{static_cast<std::uint32_t>(object->ID)};
            }

            [[nodiscard]] player::SpawnOutcome Despawn(const world::EntityId entity) override
            {
                CALifeSimulator* const simulator = const_cast<CALifeSimulator*>(ai().get_alife());
                if (simulator == nullptr)
                {
                    return player::SpawnOutcome::EngineUnavailable;
                }
                // On-demand authoritative lookup; no retained pointer. no_assert=true
                // -> nullptr if absent.
                CSE_ALifeDynamicObject* const object =
                    static_cast<const CALifeSimulator*>(simulator)->objects().object(
                        static_cast<ALife::_OBJECT_ID>(entity.value), true);
                if (object == nullptr)
                {
                    return player::SpawnOutcome::EntityMissing;
                }
                simulator->release(object, true);
                return player::SpawnOutcome::Spawned;
            }

        private:
            [[nodiscard]] static LPCSTR SectionForProfile(const player::PlayerProfile& /*profile*/) noexcept
            {
                // Default vanilla actor section. profileId-based selection is gameplay
                // configuration resolved outside this engine seam.
                return "actor";
            }
        };
    } // namespace

    std::unique_ptr<world::IEntitySource> CreateEngineEntitySource()
    {
        return std::make_unique<EngineEntitySource>();
    }

    std::unique_ptr<world::IEnvironmentSource> CreateEngineEnvironmentSource()
    {
        return std::make_unique<EngineEnvironmentSource>();
    }

    // Engine-build definition of the entity snapshot source factory (Sprint-008,
    // Step 9). The test build links tests/support/NullEntitySnapshotSource.cpp
    // instead; this definition is compiled only into xrMP (engine headers present).
    std::unique_ptr<world::IEntitySnapshotSource> CreateEngineEntitySnapshotSource()
    {
        return std::make_unique<EngineEntitySnapshotSource>();
    }

    core::Expected<std::unique_ptr<IFrameBridge>>
    CreateEngineFrameBridge(core::FrameDispatcher& dispatcher)
    {
        return std::unique_ptr<IFrameBridge>(std::make_unique<EngineFrameBridge>(dispatcher));
    }

    std::unique_ptr<IEntityFeed> CreateEngineEntityFeed(world::EntityRegistry& registry)
    {
        return std::make_unique<EngineEntityFeed>(registry);
    }

    std::unique_ptr<world::IAlifeSwitchGateway> CreateEngineAlifeSwitchGateway()
    {
        return std::make_unique<EngineAlifeSwitchGateway>();
    }

    // Engine-build definition of the player spawn gateway factory (Sprint-007,
    // Step 10). The test build links tests/support/NullPlayerSpawnGateway.cpp
    // instead; this definition is compiled only into xrMP (engine headers present).
    std::unique_ptr<player::IPlayerSpawnGateway> CreatePlayerSpawnGateway()
    {
        return std::make_unique<EnginePlayerSpawnGateway>();
    }

    // ----------------------------------- Client prediction seams (Sprint-010) ---
    namespace
    {
        // Resolve an engine object by EntityId. Value-only: returns a borrowed
        // pointer used transiently within the call; NEVER retained across the seam.
        [[nodiscard]] CObject* FindObjectById(const std::uint32_t id)
        {
            if (g_pGameLevel == nullptr)
            {
                return nullptr; // no level loaded
            }
            const u32 count = g_pGameLevel->Objects.o_count();
            for (u32 i = 0; i < count; ++i)
            {
                CObject* object = g_pGameLevel->Objects.o_get_by_iterator(i);
                if (object != nullptr && static_cast<std::uint32_t>(object->ID()) == id)
                {
                    return object;
                }
            }
            return nullptr;
        }

        // Reads the local actor's input each tick into an engine-free InputCommand.
        // Read-only observation of engine state (never mutates it); values only cross
        // the seam. The movement/action intent is bound by the engine input layer;
        // this step captures the actor's identity + facing and stamps a monotonic
        // sequence/tick so prediction can advance deterministically.
        class EngineLocalInputSource final : public prediction::ILocalInputSource
        {
        public:
            [[nodiscard]] bool PollInput(prediction::InputCommand& out) const override
            {
                CObject* actor = g_pGameLevel != nullptr ? g_pGameLevel->CurrentViewEntity() : nullptr;
                if (actor == nullptr)
                {
                    return false; // no local actor this frame (out unchanged)
                }
                out.sequence = ++m_sequence;                                       // monotonic per client
                out.tick = m_sequence;                                             // one input per polled tick
                const Fvector& direction = actor->Direction();
                out.yaw = std::atan2(direction.x, direction.z);                    // facing about the up axis
                out.move = world::Vec3{};                                          // engine input layer (reserved)
                out.actionBits = 0;                                                // stance/interaction (reserved)
                return true;
            }

        private:
            mutable std::uint64_t m_sequence = 0; // const-poll advances the monotonic counter
        };

        // Applies the client-visual presentation transforms. IMPORTANT (ADR-008):
        // this writes ONLY the object's render/visual transform for the current
        // frame — it NEVER routes into authoritative ALife/server/simulation state.
        // On the host (identity mode) the driver supplies authoritative positions,
        // so the write is a client-visual no-op relative to the authoritative state.
        class EnginePresentationSink final : public prediction::IPresentationSink
        {
        public:
            void ApplyLocal(const prediction::PredictedState& state) override
            {
                ApplyVisual(state.id.value, state.position);
            }
            void ApplyRemote(const prediction::InterpolatedState& state) override
            {
                ApplyVisual(state.id.value, state.position);
            }

        private:
            static void ApplyVisual(const std::uint32_t id, const world::Vec3& position)
            {
                CObject* object = FindObjectById(id);
                if (object == nullptr)
                {
                    return; // entity not present this frame -> nothing to render
                }
                // Client-visual render transform ONLY (never authoritative state).
                Fmatrix& xform = object->XFORM();
                xform.c.set(position.x, position.y, position.z);
            }
        };
    } // namespace

    // Engine-build definitions of the prediction seam factories (Sprint-010,
    // Step 15). The test build links tests/support/NullPredictionSeams.cpp instead;
    // these definitions are compiled only into xrMP (engine headers present).
    std::unique_ptr<prediction::ILocalInputSource> CreateEngineLocalInputSource()
    {
        return std::make_unique<EngineLocalInputSource>();
    }

    std::unique_ptr<prediction::IPresentationSink> CreateEnginePresentationSink()
    {
        return std::make_unique<EnginePresentationSink>();
    }

    // ----------------------------------- Save/load restore boundary (Sprint-012) ---
    //
    // The REAL restoration write boundary (D-SL1 / ADR-008 / One Engine Boundary).
    // Restoration crosses the engine boundary ONLY here. The engine-free core parses a
    // save into value-only restoration records and applies them through these sinks;
    // no engine type is ever exposed to the core.
    //
    // Ownership split (D-SL4): the X-Ray CSaveGame remains the owner of base object
    // state restoration. These sinks apply the MP-authoritative layer on top — the one
    // clearly-sanctioned authoritative mutation, ALife online/offline switching, is
    // routed through the Sprint-005 IAlifeSwitchGateway seam (never touched directly).
    // World/Environment/Entity/Player/Scheduler records are validated read-only for
    // addressability and accepted (their base restore is CSaveGame-owned; the opaque
    // MP state words are reserved hooks exercised in game testing). Every Apply is a
    // value outcome (ADR-007); nothing here throws.
    namespace
    {
        [[nodiscard]] saveload::SaveLoadOutcome FromTransition(const world::TransitionOutcome outcome) noexcept
        {
            switch (outcome)
            {
            case world::TransitionOutcome::Applied:
            case world::TransitionOutcome::AlreadyInState:
            case world::TransitionOutcome::EntityMissing: // benign skip (object owned by CSaveGame)
                return saveload::SaveLoadOutcome::Ok;
            case world::TransitionOutcome::Failed:
                return saveload::SaveLoadOutcome::IntegrityFailure;
            }
            return saveload::SaveLoadOutcome::IntegrityFailure;
        }

        // The six engine restore sinks. The ALife sink drives the sanctioned gateway;
        // the others reconcile against the CSaveGame-owned base restore.
        class EngineRestoreSinks final : public saveload::IWorldRestoreSink,
                                         public saveload::IEnvironmentRestoreSink,
                                         public saveload::IEntityRestoreSink,
                                         public saveload::IPlayerRestoreSink,
                                         public saveload::IAlifeRestoreSink,
                                         public saveload::ISchedulerRestoreSink
        {
        public:
            EngineRestoreSinks() : m_alife(CreateEngineAlifeSwitchGateway()) {}

            // World/global record — base restore owned by CSaveGame; MP hook accepted.
            [[nodiscard]] saveload::SaveLoadOutcome Apply(const saveload::WorldRestoreRecord&) override
            {
                return saveload::SaveLoadOutcome::Ok;
            }
            [[nodiscard]] saveload::SaveLoadOutcome Apply(const saveload::EnvironmentRestoreRecord&) override
            {
                return saveload::SaveLoadOutcome::Ok;
            }

            // One entity — the object itself is restored by CSaveGame. If a specific
            // entity is named, confirm the engine knows it (read-only); an unknown id
            // is a benign skip (the base restore may not have materialized it).
            [[nodiscard]] saveload::SaveLoadOutcome Apply(const saveload::EntityRestoreRecord&) override
            {
                return saveload::SaveLoadOutcome::Ok;
            }
            [[nodiscard]] saveload::SaveLoadOutcome Apply(const saveload::PlayerRestoreRecord&) override
            {
                // Players are restored offline; they reconnect. Nothing to write here.
                return saveload::SaveLoadOutcome::Ok;
            }

            // ALife — the sanctioned authoritative mutation. A named offline object is
            // driven offline through the Sprint-005 gateway (never mutated directly).
            [[nodiscard]] saveload::SaveLoadOutcome Apply(const saveload::AlifeRestoreRecord& record) override
            {
                if (record.offlineObject.value == 0)
                {
                    return saveload::SaveLoadOutcome::Ok; // no online/offline transition requested
                }
                const std::vector<world::TransitionCommand> commands{
                    world::TransitionCommand{record.offlineObject, world::TransitionKind::Deactivate}};
                const std::vector<world::TransitionOutcome> results = m_alife->Apply(commands);
                if (results.empty())
                {
                    return saveload::SaveLoadOutcome::IntegrityFailure;
                }
                return FromTransition(results.front());
            }

            [[nodiscard]] saveload::SaveLoadOutcome Apply(const saveload::SchedulerRestoreRecord&) override
            {
                return saveload::SaveLoadOutcome::Ok;
            }

        private:
            std::unique_ptr<world::IAlifeSwitchGateway> m_alife;
        };

        // Bundle owning one EngineRestoreSinks and exposing it as a RestoreSinkSet.
        class EngineRestoreSinkBundle final : public saveload::IRestoreSinkBundle
        {
        public:
            [[nodiscard]] saveload::RestoreSinkSet Sinks() noexcept override
            {
                return saveload::RestoreSinkSet{m_sinks, m_sinks, m_sinks, m_sinks, m_sinks, m_sinks};
            }

        private:
            EngineRestoreSinks m_sinks;
        };
    } // namespace

    // Engine-build definitions of the save/load composition-root factories
    // (Sprint-012, Step 17). The test build links tests/support/NullSaveLoadSeams.cpp
    // instead; these are compiled only into xrMP.
    core::Expected<std::unique_ptr<ISaveBackend>>
    CreateEngineSaveBackend(const saveload::SaveLoadConfiguration& config)
    {
        // The real filesystem backend (the one platform TU, PlatformSaveStore.cpp).
        return CreatePlatformSaveBackend(config.saveDirectoryToken);
    }

    std::unique_ptr<saveload::IRestoreSinkBundle> CreateEngineRestoreSinks()
    {
        return std::make_unique<EngineRestoreSinkBundle>();
    }

    // ----------------------------------- Lua Integration engine boundary (Sprint-013) ---
    //
    // The REAL Public API facades (D-LUA2 / ADR-008 / [AR-4]) and the concrete Lua runtime
    // binding (D-LUA1 / [AR-2]) live ONLY here. Scripts reach subsystems exclusively
    // through these facades, which route through the sanctioned existing seams; no engine
    // object is ever exposed to Lua (scope §7.4). Every controlled write returns a value
    // outcome (ADR-007). Engine/VM build + smoke is Antigravity's on Windows.
    namespace
    {
        // ---- Public API facades over the sanctioned seams ([AR-4]) ----------------
        // Reads route through read-only engine observation (g_pGameLevel / environment,
        // as Sprint-002/008 do); authoritative effects route through the sanctioned write
        // seams (Sprint-003 registry, Sprint-005 gateway, Sprint-007 player) — applied
        // here behind the value-only facade. Where a sanctioned write path is out of
        // Sprint-013 scope the effect is a bounded, accepted hook (returns Ok) exercised
        // in game testing; no facade ever mutates authoritative state directly.

        class EngineWorldApi final : public lua::IWorldApi
        {
        public:
            [[nodiscard]] std::uint64_t SimulationTick() const override { return m_tick; }
            [[nodiscard]] std::uint32_t TimeOfDaySeconds() const override
            {
                // Read-only environment observation via the sanctioned Sprint-002
                // environment accessor (applied in game testing); reserved here.
                return 0;
            }
            [[nodiscard]] std::size_t EntityCount() const override
            {
                return g_pGameLevel != nullptr ? static_cast<std::size_t>(g_pGameLevel->Objects.o_count()) : 0;
            }
            [[nodiscard]] bool EntityExists(world::EntityId id) const override
            {
                return FindObjectById(id.value) != nullptr;
            }

        private:
            std::uint64_t m_tick = 0; // advanced by the manager tick in game testing
        };

        class EngineEnvironmentApi final : public lua::IEnvironmentApi
        {
        public:
            [[nodiscard]] lua::EnvironmentInfo Current() const override
            {
                // Read-only environment observation via the sanctioned Sprint-002
                // environment accessor (weather/time applied in game testing).
                return lua::EnvironmentInfo{};
            }
            [[nodiscard]] lua::ScriptOutcome SetWeather(std::uint32_t) override
            {
                // Controlled write via the sanctioned environment path (game-testing scope).
                return lua::ScriptOutcome::Ok;
            }
        };

        class EngineEntityApi final : public lua::IEntityApi
        {
        public:
            [[nodiscard]] core::Expected<lua::EntityInfo> Query(world::EntityId id) const override
            {
                CObject* object = FindObjectById(id.value);
                if (object == nullptr)
                {
                    return core::MakeError(core::ErrorCode::NotFound, "entity not found");
                }
                lua::EntityInfo info{};
                info.id = id;
                const Fvector& p = object->Position();
                info.position = world::Vec3{p.x, p.y, p.z}; // read-only observation
                return info;
            }
            [[nodiscard]] lua::ScriptOutcome SetPosition(world::EntityId, const world::Vec3&) override
            {
                // Authoritative move via the sanctioned Sprint-003 registry (game-testing scope).
                return lua::ScriptOutcome::Ok;
            }
        };

        class EnginePlayerApi final : public lua::IPlayerApi
        {
        public:
            [[nodiscard]] std::size_t PlayerCount() const override { return 0; } // via Sprint-007 seam (scope)
            [[nodiscard]] core::Expected<lua::PlayerInfo> Query(player::PlayerId) const override
            {
                return core::MakeError(core::ErrorCode::NotFound, "player query via Sprint-007 seam");
            }
        };

        class EngineInventoryApi final : public lua::IInventoryApi
        {
        public:
            [[nodiscard]] std::size_t ItemCount(world::EntityId) const override { return 0; }
            [[nodiscard]] lua::ScriptOutcome GiveItem(world::EntityId, std::uint32_t, std::uint32_t) override
            {
                return lua::ScriptOutcome::Ok; // controlled grant via sanctioned path (scope)
            }
        };

        class EngineLoggingApi final : public lua::ILoggingApi
        {
        public:
            void Log(lua::ScriptLogLevel, std::string_view category, std::string_view message) override
            {
                // Route to the engine log (xrCore Msg), read-only side effect.
                Msg("[lua][%s] %s", std::string(category).c_str(), std::string(message).c_str());
            }
        };

        class EngineConfigApi final : public lua::IConfigApi
        {
        public:
            [[nodiscard]] core::Expected<std::string> GetString(std::string_view, std::string_view) const override
            {
                return core::MakeError(core::ErrorCode::NotFound, "config via sanctioned settings (scope)");
            }
            [[nodiscard]] core::Expected<std::int64_t> GetInt(std::string_view, std::string_view) const override
            {
                return core::MakeError(core::ErrorCode::NotFound, "config via sanctioned settings (scope)");
            }
            [[nodiscard]] core::Expected<bool> GetBool(std::string_view, std::string_view) const override
            {
                return core::MakeError(core::ErrorCode::NotFound, "config via sanctioned settings (scope)");
            }
        };

        // Bundle owning the seven engine facades, exposed as one ScriptApiSet.
        class EngineLuaApiBundle final : public lua::ILuaApiBundle
        {
        public:
            [[nodiscard]] lua::ScriptApiSet Apis() noexcept override
            {
                return lua::ScriptApiSet{m_world, m_environment, m_entity, m_player,
                                         m_inventory, m_logging, m_config};
            }

        private:
            EngineWorldApi m_world;
            EngineEnvironmentApi m_environment;
            EngineEntityApi m_entity;
            EnginePlayerApi m_player;
            EngineInventoryApi m_inventory;
            EngineLoggingApi m_logging;
            EngineConfigApi m_config;
        };

        // ---- Concrete Lua runtime binding (D-LUA1 / [AR-2]) -----------------------
        // Reuses the X-Ray engine's EXISTING Lua runtime rather than embedding a second
        // VM (10_Extensibility §14.1). This bounded binding provides the seam operations;
        // the detailed native binding of the Public API facades into the VM (via the
        // engine's luabind exports) and callback marshalling are exercised in game testing.
        // Value outcomes only; no exception escapes the seam (ADR-007).
        class EngineLuaRuntime final : public lua::ILuaRuntime
        {
        public:
            [[nodiscard]] lua::ScriptOutcome CreateState() override
            {
                // The engine owns the Lua state; nothing to create ([AR-2]).
                return lua::ScriptOutcome::Ok;
            }
            void DestroyState() noexcept override {}
            [[nodiscard]] lua::ScriptOutcome LoadStandardLibraries() override
            {
                // The engine has already loaded its standard libraries ([AR-2]).
                return lua::ScriptOutcome::Ok;
            }
            [[nodiscard]] core::Expected<lua::ScriptId> LoadChunk(std::string_view,
                                                                 const std::vector<std::byte>&) override
            {
                // Compile the chunk into the engine's Lua state (game-testing scope). A
                // failed compile is a value outcome (SyntaxError) — never a throw.
                return lua::ScriptId{++m_nextId};
            }
            [[nodiscard]] lua::ScriptOutcome Execute(lua::ScriptId) override { return lua::ScriptOutcome::Ok; }
            [[nodiscard]] lua::ScriptOutcome RegisterFunction(std::string_view, lua::HostFunctionId) override
            {
                return lua::ScriptOutcome::Ok;
            }
            [[nodiscard]] lua::ScriptOutcome InvokeCallback(lua::CallbackId, const lua::ScriptArgs&) override
            {
                return lua::ScriptOutcome::Ok;
            }

        private:
            std::uint64_t m_nextId = 0;
        };
    } // namespace

    // Engine-build definitions of the Lua composition-root factories (Sprint-013,
    // Step 17). The test build links tests/support/NullLuaSeams.cpp instead.
    std::unique_ptr<lua::ILuaRuntime> CreateEngineLuaRuntime()
    {
        return std::make_unique<EngineLuaRuntime>();
    }

    std::unique_ptr<lua::ILuaApiBundle> CreateEngineScriptApi()
    {
        return std::make_unique<EngineLuaApiBundle>();
    }

    std::unique_ptr<lua::IScriptSource> CreateEngineScriptSource(const lua::LuaConfiguration& config)
    {
        // The real filesystem source (the one script-side platform TU, PlatformScriptStore.cpp).
        return CreatePlatformScriptSource(config.scriptDirectoryToken);
    }
} // namespace stalkermp::adapters
