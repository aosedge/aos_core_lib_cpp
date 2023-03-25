
/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_INSTANCE_HPP_
#define AOS_INSTANCE_HPP_

#include "aos/common/types.hpp"

namespace aos {
namespace sm {
namespace launcher {

/**
 * Launcher instance.
 */
class Instance {
public:
    /**
     * Creates instance.
     *
     * @param info instance info.
     */
    explicit Instance(const InstanceInfo& info);

    /**
     * Starts instance.
     *
     * @return Error
     */
    Error Start();

    /**
     * Stops instance.
     *
     * @return Error
     */
    Error Stop();

    /**
     * Returns instance ID.
     *
     * @return const String& instance ID.
     */
    const String& InstanceID() const { return mInstanceID; };

    /**
     * Returns instance info.
     *
     * @return const InstanceInfo& instance info.
     */
    const InstanceInfo& Info() const { return mInfo; };

    /**
     * Returns instance run state.
     *
     * @return const InstanceRunState& run state.
     */
    const InstanceRunState& RunState() const { return mRunState; };

    /**
     * Returns instance run error.
     *
     * @return const Error& run error.
     */
    const Error& RunError() const { return mRunError; };

    /**
     * Returns instance service Aos version.
     *
     * @return uint64_t Aos version.
     */
    uint64_t AosVersion() const { return 0; };

    /**
     * Compares instances.
     *
     * @param instance instance to compare.
     * @return bool.
     */
    bool operator==(const Instance& instance) const { return mInfo == instance.mInfo; }

    /**
     * Compares instance info.
     *
     * @param instance instance to compare.
     * @return bool.
     */
    bool operator!=(const Instance& instance) const { return !operator==(instance); }

    /**
     * Compares instance with instance info.
     *
     * @param info info to compare.
     * @return bool.
     */
    bool operator==(const InstanceInfo& info) const { return mInfo == info; }

    /**
     * Compares instance with instance info.
     *
     * @param info info to compare.
     * @return bool.
     */
    bool operator!=(const InstanceInfo& info) const { return !operator==(info); }

    /**
     * Outputs instance to log.
     *
     * @param log log to output.
     * @param instance instance.
     *
     * @return Log&.
     */
    friend Log& operator<<(Log& log, const Instance& instance) { return log << instance.mInstanceID; }

private:
    static constexpr auto cInstanceIDLen = 16;

    static size_t sInstanceID;

    StaticString<cInstanceIDLen> mInstanceID;
    InstanceInfo                 mInfo;
    InstanceRunState             mRunState;
    Error                        mRunError;
};

} // namespace launcher
} // namespace sm
} // namespace aos

#endif
