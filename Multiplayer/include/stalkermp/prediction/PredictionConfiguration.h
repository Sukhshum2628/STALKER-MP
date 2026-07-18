// STALKER-MP — Prediction configuration (Sprint-010, Step 2)
//
// Parsed from the [prediction] section of server.cfg. Missing section => all
// defaults; present-but-invalid value => InvalidArgument (value outcome). All
// depths/delays are tick counts; correction thresholds are fixed-point integers
// (millimetres / milliradians) — no wall-clock in control flow. Engine-free and
// OS-free. ADR-007: no exceptions, no RTTI, no iostream; Expected<T> /
// common::Format.
//
// This file introduces the configuration value + FromConfig only — no buffers,
// prediction/interpolation logic, seams, driver, diagnostics, engine binding, or
// Bootstrap wiring.
//
// Cross-field: inputBufferDepth >= 1; stateBufferDepth >= 1; maxPredictionTicks
// >= 1; version >= 1. Interpolation delay and thresholds may be 0.

#pragma once

#include <cstdint>

#include "stalkermp/core/Config.h"   // core::ConfigStore
#include "stalkermp/core/Expected.h" // core::Expected

namespace stalkermp::prediction
{
    struct PredictionConfiguration
    {
        // Number of buffered local input commands retained for prediction + replay.
        // Must be >= 1.
        std::uint32_t inputBufferDepth = 128;

        // Number of predicted local states retained for reconciliation lookup.
        // Must be >= 1.
        std::uint32_t stateBufferDepth = 128;

        // Interpolation delay in ticks: remote entities are rendered this many ticks
        // behind the newest authoritative frame (jitter buffer). 0 = no delay.
        std::uint32_t interpolationDelayTicks = 2;

        // Maximum ticks the local player may be predicted ahead of the last
        // confirmed authoritative state (bounds prediction cost/drift). Must be >= 1.
        std::uint32_t maxPredictionTicks = 8;

        // Reconciliation thresholds (fixed-point). A predicted state within these of
        // authoritative is smoothed; beyond, it is snapped (host authority wins).
        std::uint32_t positionCorrectionThresholdMm = 250;   // millimetres
        std::uint32_t rotationCorrectionThresholdMrad = 100; // milliradians
        std::uint32_t velocityCorrectionThresholdMm = 500;   // millimetres/tick

        // Prediction/interpolation format version stamped into diagnostics
        // (forward-compatible). Must be >= 1.
        std::uint32_t version = 1;

        // Parses [prediction]. Missing section => defaults; present-but-invalid =>
        // InvalidArgument. Enforces the cross-field minimums documented above.
        [[nodiscard]] static core::Expected<PredictionConfiguration> FromConfig(const core::ConfigStore& config);
    };
} // namespace stalkermp::prediction
