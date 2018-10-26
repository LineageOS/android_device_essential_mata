/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INTERACTIONHANDLER_H
#define INTERACTIONHANDLER_H

#include <condition_variable>
#include <mutex>
#include <thread>

enum interaction_state {
    INTERACTION_STATE_UNINITIALIZED,
    INTERACTION_STATE_IDLE,
    INTERACTION_STATE_INTERACTION,
    INTERACTION_STATE_WAITING,
};

struct InteractionHandler {
    InteractionHandler();
    ~InteractionHandler();
    bool Init();
    void Exit();
    void Acquire(int32_t duration);

 private:
    void Release();
    void WaitForIdle(int32_t wait_ms, int32_t timeout_ms);
    void AbortWaitLocked();
    void Routine();

    void PerfLock();
    void PerfRel();

    long long CalcTimespecDiffMs(struct timespec start, struct timespec end);

    enum interaction_state mState;

    int mIdleFd;
    int mEventFd;

    int32_t mWaitMs;
    int32_t mMinDurationMs;
    int32_t mMaxDurationMs;
    int32_t mDurationMs;

    struct timespec mLastTimespec;

    std::unique_ptr<std::thread> mThread;
    std::mutex mLock;
    std::condition_variable mCond;
    int mHandle;
};

#endif //INTERACTIONHANDLER_H
