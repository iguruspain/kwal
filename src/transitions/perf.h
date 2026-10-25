// TEMPORARY perf tracing for the transition-open latency diagnosis.
// Remove this header and its call sites once the breakdown is known.
#pragma once

#include <chrono>

inline long long perfMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
