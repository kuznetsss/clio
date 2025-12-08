//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#pragma once

#include "util/ObservableValue.hpp"
#include "util/prometheus/Bool.hpp"
#include "util/prometheus/Label.hpp"
#include "util/prometheus/Prometheus.hpp"

#include <atomic>

namespace etl {

class SystemStateNg {
public:
    /**
     * @brief State machine enum for state of ETL

    State transitions:
    - Any -> CorruptionDetected
    - Any -> AmendmentBlock
    - NotWriting -> StartWriting
    - NotWriting -> Writing
    - StartWriting -> Writing
    - StartWriting -> StopWriting
    - Writing -> StopWriting
    - StopWriting -> NotWriting
     */
    enum class State { ReadOnly, NotWriting, StartWriting, Writing, StopWriting, AmendmentBlock, CorruptionDetected };

private:
    util::ObservableValue<std::atomic<State>> state_;

    util::prometheus::Bool isStrictReadonly_ = PrometheusService::boolMetric(
        "read_only",
        util::prometheus::Labels{},
        "Whether the process is in strict read-only mode"
    );

    util::prometheus::Bool isWriting_ = PrometheusService::boolMetric(
        "etl_writing",
        util::prometheus::Labels{},
        "Whether the process is writing to the database"
    );

    util::prometheus::Bool isAmendmentBlocked_ = PrometheusService::boolMetric(
        "etl_amendment_blocked",
        util::prometheus::Labels{},
        "Whether clio detected an amendment block"
    );

    util::prometheus::Bool isCorruptionDetected_ = PrometheusService::boolMetric(
        "etl_corruption_detected",
        util::prometheus::Labels{},
        "Whether clio detected a corruption that needs manual attention"
    );

public:
    SystemStateNg(bool isReadOnly) : state_(isReadOnly ? State::ReadOnly : State::NotWriting)
    {
        isStrictReadonly_ = isReadOnly;
        state_.observe([this](State newState) {
            isWriting_ = newState == State::Writing;
            isAmendmentBlocked_ = newState == State::AmendmentBlock;
            isCorruptionDetected_ = newState == State::CorruptionDetected;
        });
    }

    ~SystemStateNg() = default;
    SystemStateNg(SystemStateNg&&) = delete;
    SystemStateNg(SystemStateNg const&) = delete;
    SystemStateNg&
    operator=(SystemStateNg&&) = delete;
    SystemStateNg&
    operator=(SystemStateNg const&) = delete;

    [[nodiscard]] bool
    isStrictReadonly() const;
    [[nodiscard]] bool
    isWriting() const;
    [[nodiscard]] bool
    isStopping() const;
    [[nodiscard]] bool
    isWriteConflict() const;
    [[nodiscard]] bool
    isAmendmentBlocked() const;
    [[nodiscard]] bool
    isCorruptionDetected() const;
};

}  // namespace etl
