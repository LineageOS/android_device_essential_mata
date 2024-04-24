/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation, nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <unistd.h>
#include <PowerEvtHandler.h>
#include <LocationApiService.h>
#include <log_util.h>
#include <syslog.h>

#define ACK_TIMEOUT_US 300000 // 300 msec

LocationApiService* PowerEvtHandler::mLocationApiService = nullptr;

PowerEvtHandler* PowerEvtHandler::getPwrEvtHandler(LocationApiService* locServiceApiObj) {
    mLocationApiService = locServiceApiObj;
    static PowerEvtHandler instance;
    return &instance;
}

PowerEvtHandler::PowerEvtHandler() {
    int ret = pwr_state_notification_register(PowerEvtHandler::pwrStateCb);
    syslog(LOG_INFO, "PowerEvtHandler: register api returned: %d", ret);
}

PowerEvtHandler::~PowerEvtHandler() {
}

int PowerEvtHandler::pwrStateCb(power_state_t pwr_state) {
    client_ack_t client_ack;
    client_ack.ack = ERR;
    PowerStateType powerState = POWER_STATE_UNKNOWN;

    syslog(LOG_INFO, "PowerEvtHandler: pwrStateCb: %d", pwr_state.sys_state);
    switch (pwr_state.sys_state) {
        case SYS_SUSPEND:
            client_ack.ack = SUSPEND_ACK;
            powerState = POWER_STATE_SUSPEND;
            break;
        case SYS_RESUME:
            client_ack.ack = RESUME_ACK;
            powerState = POWER_STATE_RESUME;
            break;
        case SYS_SHUTDOWN:
            client_ack.ack = SHUTDOWN_ACK;
            powerState = POWER_STATE_SHUTDOWN;
            break;
    }

    if (powerState != POWER_STATE_UNKNOWN) {
        if (mLocationApiService) {
              mLocationApiService->onPowerEvent(powerState);
        }
    }

    //Allow some time to stop the session and write calibration data NVM.
    usleep(ACK_TIMEOUT_US);
    syslog(LOG_INFO, "PowerEvtHandler: pwrStateCb sending ack");
    send_acknowledgement(client_ack);

    return 0;
}

