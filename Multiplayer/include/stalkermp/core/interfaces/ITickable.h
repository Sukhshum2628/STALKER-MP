// STALKER-MP — Base interfaces (Sprint-001, §7.8)
//
// ITickable: anything advanced once per frame/tick by its owner.
// Which thread calls Tick is determined by subsystem ownership
// (09_Threading.md); the interface itself is thread-agnostic.

#pragma once

namespace stalkermp::core
{
    class ITickable
    {
    public:
        virtual ~ITickable() = default;

        // deltaSeconds: wall-clock time since the previous tick.
        virtual void Tick(double deltaSeconds) = 0;
    };
} // namespace stalkermp::core
