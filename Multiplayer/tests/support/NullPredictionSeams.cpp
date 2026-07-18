// STALKER-MP — Null client prediction seams (test build only)
//
// Test-build counterpart of the prediction seam factories in
// src/adapters/EngineAdapters.cpp: provides the same factory symbols without any
// engine dependency so composition-root code is identical in both builds. The null
// input source never yields input; the null presentation sink observes nothing.
// Both are inert and deterministic.

#include "stalkermp/adapters/PredictionSeams.h"

namespace stalkermp::adapters
{
    namespace
    {
        class NullLocalInputSource final : public prediction::ILocalInputSource
        {
        public:
            [[nodiscard]] bool PollInput(prediction::InputCommand&) const override { return false; }
        };

        class NullPresentationSink final : public prediction::IPresentationSink
        {
        public:
            void ApplyLocal(const prediction::PredictedState&) override {}
            void ApplyRemote(const prediction::InterpolatedState&) override {}
        };
    } // namespace

    std::unique_ptr<prediction::ILocalInputSource> CreateEngineLocalInputSource()
    {
        return std::make_unique<NullLocalInputSource>();
    }

    std::unique_ptr<prediction::IPresentationSink> CreateEnginePresentationSink()
    {
        return std::make_unique<NullPresentationSink>();
    }
} // namespace stalkermp::adapters
