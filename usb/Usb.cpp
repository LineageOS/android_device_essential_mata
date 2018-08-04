/*
 * Copyright (C) 2016 The Android Open Source Project
 * Copyright (C) 2018 The LineageOS Project
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

#include <assert.h>
#include <chrono>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <pthread.h>
#include <regex>
#include <stdio.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

#include <cutils/uevent.h>
#include <sys/epoll.h>
#include <utils/Errors.h>
#include <utils/StrongPointer.h>

#include "Usb.h"

namespace android {
namespace hardware {
namespace usb {
namespace V1_1 {
namespace implementation {

const char ESSENTIAL_USB_VENDOR_ID_STR[] = "2e17";
const char ESSENTIAL_USBC_35_ADAPTER_UNPLUGGED_ID_STR[] = "a001";

// Set by the signal handler to destroy the thread
volatile bool destroyThread;

static void checkUsbDeviceAutoSuspend(const std::string& devicePath);
static bool isUsbAudioDevice();

static int32_t readFile(std::string filename, std::string& contents) {
    std::ifstream file(filename);

    if (file.is_open()) {
        getline(file, contents);
        file.close();
        return 0;
    }
    return -1;
}

static int32_t writeFile(const std::string &filename,
                         const std::string &contents) {
    FILE *fp;
    int ret;

    fp = fopen(filename.c_str(), "w");
    if (fp != NULL) {
        ret = fputs(contents.c_str(), fp);
        fclose(fp);
        if (ret == EOF) {
            ALOGE("fputs failed in writeFile %s", filename.c_str());
            return -1;
        }
        return 0;
    } else {
        ALOGE("fopen failed in writeFile %s, errno=%d", filename.c_str(), errno);
    }
    return -1;
}

std::string appendRoleNodeHelper(const std::string portName, PortRoleType type) {
    std::string node("/sys/class/dual_role_usb/" + portName);

    switch(type) {
        case PortRoleType::DATA_ROLE:
            return node + "/data_role";
        case PortRoleType::POWER_ROLE:
            return node + "/power_role";
        default:
            return node + "/mode";
    }
}

std::string convertRoletoString(PortRole role) {
    if (role.type == PortRoleType::POWER_ROLE) {
        if (role.role == static_cast<uint32_t> (PortPowerRole::SOURCE))
            return "source";
        else if (role.role ==  static_cast<uint32_t> (PortPowerRole::SINK))
            return "sink";
    } else if (role.type == PortRoleType::DATA_ROLE) {
        if (role.role == static_cast<uint32_t> (PortDataRole::HOST))
            return "host";
        if (role.role == static_cast<uint32_t> (PortDataRole::DEVICE))
            return "device";
    } else if (role.type == PortRoleType::MODE) {
        if (role.role == static_cast<uint32_t> (PortMode_1_1::UFP))
            return "ufp";
        if (role.role == static_cast<uint32_t> (PortMode_1_1::DFP))
            return "dfp";
    }
    return "none";
}

Return<void> Usb::switchRole(const hidl_string& portName,
        const PortRole& newRole) {
    std::string filename = appendRoleNodeHelper(std::string(portName.c_str()),
        newRole.type);
    std::ofstream file(filename);
    std::string written;
    bool roleSwitch = false;

    if (filename == "") {
      ALOGE("Fatal: invalid node type");
      return Void();
    }

    pthread_mutex_lock(&mRoleSwitchLock);

    ALOGI("filename write: %s role:%d", filename.c_str(), newRole.role);

    if (file.is_open()) {
        file << convertRoletoString(newRole).c_str();
        file.close();
        if (!readFile(filename, written)) {
            ALOGI("written: %s", written.c_str());
            if (written == convertRoletoString(newRole)) {
                ALOGI("Role switch successfull");
                Return<void> ret =
                    mCallback_1_0->notifyRoleSwitchStatus(portName, newRole,
                    Status::SUCCESS);
                if (!ret.isOk())
                    ALOGE("RoleSwitchStatus error %s",
                        ret.description().c_str());
            }
        }
    }

    pthread_mutex_lock(&mLock);
    if (mCallback_1_0 != NULL) {
        Return<void> ret = mCallback_1_0->notifyRoleSwitchStatus(portName, newRole,
                roleSwitch ? Status::SUCCESS : Status::ERROR);
        if (!ret.isOk())
            ALOGE("RoleSwitchStatus error %s", ret.description().c_str());
    } else {
        ALOGE("Not notifying the userspace. Callback is not set");
    }
    pthread_mutex_unlock(&mLock);
    pthread_mutex_unlock(&mRoleSwitchLock);

    return Void();
}

Status getCurrentRoleHelper(const std::string &portName,
        PortRoleType type, uint32_t &currentRole)  {
    std::string filename;
    std::string roleName;

    if (type == PortRoleType::POWER_ROLE) {
        filename = "/sys/class/dual_role_usb/" +
            portName + "/power_role";
        currentRole = static_cast<uint32_t>(PortPowerRole::NONE);
    } else if (type == PortRoleType::DATA_ROLE) {
        filename = "/sys/class/dual_role_usb/" +
            portName + "/data_role";
        currentRole = static_cast<uint32_t> (PortDataRole::NONE);
    } else if (type == PortRoleType::MODE) {
        filename = "/sys/class/dual_role_usb/" +
            portName + "/mode";
        currentRole = static_cast<uint32_t> (PortMode_1_1::NONE);
    }

    if (readFile(filename, roleName)) {
        ALOGE("getCurrentRole: Failed to open filesystem node");
        return Status::ERROR;
    }

    if (roleName == "dfp")
        currentRole = static_cast<uint32_t> (PortMode_1_1::DFP);
    else if (roleName == "ufp")
        currentRole = static_cast<uint32_t> (PortMode_1_1::UFP);
    else if (roleName == "source")
        currentRole = static_cast<uint32_t> (PortPowerRole::SOURCE);
    else if (roleName == "sink")
        currentRole = static_cast<uint32_t> (PortPowerRole::SINK);
    else if (roleName == "host")
        currentRole = static_cast<uint32_t> (PortDataRole::HOST);
    else if (roleName == "device")
        currentRole = static_cast<uint32_t> (PortDataRole::DEVICE);
    else if (roleName != "none") {
        if (isUsbAudioDevice())
            currentRole = static_cast<uint32_t> (PortMode_1_1::AUDIO_ACCESSORY);
        else {
             /* case for none has already been addressed.
              * so we check if the role isnt none.
              */
            return Status::UNRECOGNIZED_ROLE;
        }
    }
    return Status::SUCCESS;
}

Status getTypeCPortNamesHelper(std::vector<std::string>& names) {
    DIR *dp;

    dp = opendir("/sys/class/dual_role_usb");
    if (dp != NULL)
    {
rescan:
        int32_t ports = 0;
        int32_t current = 0;
        struct dirent *ep;

        while ((ep = readdir (dp))) {
            if (ep->d_type == DT_LNK) {
                ports++;
            }
        }

        if (ports == 0) {
            closedir(dp);
            return Status::SUCCESS;
        }

        names.resize(ports);
        rewinddir(dp);

        while ((ep = readdir (dp))) {
            if (ep->d_type == DT_LNK) {
                /* Check to see if new ports were added since the first pass. */
                if (current >= ports) {
                    rewinddir(dp);
                    goto rescan;
                }
                names[current++] = ep->d_name;
            }
        }

        closedir (dp);
        return Status::SUCCESS;
    }

    ALOGE("Failed to open /sys/class/dual_role_usb");
    return Status::ERROR;
}

bool canSwitchRoleHelper(const std::string &portName, PortRoleType type)  {
    std::string filename = appendRoleNodeHelper(portName, type);
    std::ofstream file(filename);

    if (file.is_open()) {
        file.close();
        return true;
    }
    return false;
}

Status getPortModeHelper(const std::string portName, V1_0::PortMode& portMode)  {
    std::string filename = "/sys/class/dual_role_usb/" +
    std::string(portName.c_str()) + "/supported_modes";
    std::string modes;

    if (readFile(filename, modes)) {
        ALOGE("getSupportedRoles: Failed to open filesystem node");
        return Status::ERROR;
    }

    if (modes == "ufp dfp")
        portMode = V1_0::PortMode::DRP;
    else  if (modes == "ufp")
        portMode = V1_0::PortMode::UFP;
    else if  (modes == "dfp")
        portMode = V1_0::PortMode::DFP;
    else
        return Status::UNRECOGNIZED_ROLE;

        return Status::SUCCESS;
}

Status getPortMode_1_1Helper(const std::string portName, PortMode_1_1& portMode)  {
    std::string filename = "/sys/class/dual_role_usb/" +
    std::string(portName.c_str()) + "/supported_modes";
    std::string modes;

    if (readFile(filename, modes)) {
        ALOGE("getSupportedRoles: Failed to open filesystem node");
        return Status::ERROR;
    }

    if (modes == "ufp dfp")
        portMode = PortMode_1_1::DRP;
    else  if (modes == "ufp")
        portMode = PortMode_1_1::UFP;
    else if  (modes == "dfp")
        portMode = PortMode_1_1::DFP;
    else
        return Status::UNRECOGNIZED_ROLE;

        return Status::SUCCESS;
}

/*
 * Reuse the same method for both V1_0 and V1_1 callback objects.
 * The caller of this method would reconstruct the V1_0::PortStatus
 * object if required.
 */
Status getPortStatusHelper(hidl_vec<PortStatus_1_1> *currentPortStatus_1_1, bool V1_0) {
    std::vector<std::string> names;
    Status result = getTypeCPortNamesHelper(names);

    if (result == Status::SUCCESS) {
        currentPortStatus_1_1->resize(names.size());
        for(std::vector<std::string>::size_type i = 0; i < names.size(); i++) {
            ALOGI("%s", names[i].c_str());
            (*currentPortStatus_1_1)[i].status.portName = names[i];

            uint32_t currentRole;
            if (getCurrentRoleHelper(names[i], PortRoleType::POWER_ROLE,
                    currentRole) == Status::SUCCESS) {
                (*currentPortStatus_1_1)[i].status.currentPowerRole =
                static_cast<PortPowerRole> (currentRole);
            } else {
                ALOGE("Error while retreiving portNames");
                goto done;
            }

            if (getCurrentRoleHelper(names[i],
                    PortRoleType::DATA_ROLE, currentRole) == Status::SUCCESS) {
                (*currentPortStatus_1_1)[i].status.currentDataRole =
                        static_cast<PortDataRole> (currentRole);
            } else {
                ALOGE("Error while retreiving current port role");
                goto done;
            }

            if (getCurrentRoleHelper(names[i], PortRoleType::MODE,
                    currentRole) == Status::SUCCESS) {
                (*currentPortStatus_1_1)[i].currentMode =
                    static_cast<PortMode_1_1> (currentRole);
                (*currentPortStatus_1_1)[i].status.currentMode =
                    static_cast<V1_0::PortMode>(currentRole);
            } else {
                ALOGE("Error while retreiving current data role");
                goto done;
            }

            (*currentPortStatus_1_1)[i].status.canChangeMode =
                canSwitchRoleHelper(names[i], PortRoleType::MODE);
            (*currentPortStatus_1_1)[i].status.canChangeDataRole =
                canSwitchRoleHelper(names[i], PortRoleType::DATA_ROLE);
            (*currentPortStatus_1_1)[i].status.canChangePowerRole =
                canSwitchRoleHelper(names[i], PortRoleType::POWER_ROLE);

            ALOGI("canChangeMode: %d canChagedata: %d canChangePower:%d",
                (*currentPortStatus_1_1)[i].status.canChangeMode,
                (*currentPortStatus_1_1)[i].status.canChangeDataRole,
                (*currentPortStatus_1_1)[i].status.canChangePowerRole);

            if (V1_0) {
                if (getPortModeHelper(names[i], (*currentPortStatus_1_1)[i].status.supportedModes)
                      != Status::SUCCESS) {
                    ALOGE("Error while retrieving port modes");
                    goto done;
                }
            } else {
                (*currentPortStatus_1_1)[i].supportedModes = PortMode_1_1::UFP | PortMode_1_1::DFP;
                (*currentPortStatus_1_1)[i].status.supportedModes = V1_0::PortMode::NONE;
                (*currentPortStatus_1_1)[i].status.currentMode = V1_0::PortMode::NONE;
            }
        }
        return Status::SUCCESS;
    }
done:
    return Status::ERROR;
}

Return<void> Usb::queryPortStatus() {
    hidl_vec<PortStatus_1_1> currentPortStatus_1_1;
    hidl_vec<V1_0::PortStatus> currentPortStatus;
    Status status;
    sp<IUsbCallback> callback_V1_1 = IUsbCallback::castFrom(mCallback_1_0);

    pthread_mutex_lock(&mLock);
    if (mCallback_1_0 != NULL) {
        if (callback_V1_1 != NULL) {
            status = getPortStatusHelper(&currentPortStatus_1_1, false);
        } else {
            status = getPortStatusHelper(&currentPortStatus_1_1, true);
            currentPortStatus.resize(currentPortStatus_1_1.size());
            for (unsigned long i = 0; i < currentPortStatus_1_1.size(); i++)
                currentPortStatus[i] = currentPortStatus_1_1[i].status;
        }

        Return<void> ret;

        if (callback_V1_1 != NULL)
            ret = callback_V1_1->notifyPortStatusChange_1_1(currentPortStatus_1_1, status);
        else
            ret = mCallback_1_0->notifyPortStatusChange(currentPortStatus, status);

        if (!ret.isOk())
            ALOGE("queryPortStatus_1_1 error %s", ret.description().c_str());
    } else {
        ALOGI("Notifying userspace skipped. Callback is NULL");
    }
    pthread_mutex_unlock(&mLock);

    return Void();
}

struct data {
    int uevent_fd;
    android::hardware::usb::V1_1::implementation::Usb *usb;
};

static void uevent_event(uint32_t /*epevents*/, struct data *payload) {
    char msg[UEVENT_MSG_LEN + 2];
    char *cp;
    int n;

    n = uevent_kernel_multicast_recv(payload->uevent_fd, msg, UEVENT_MSG_LEN);
    if (n <= 0)
        return;
    if (n >= UEVENT_MSG_LEN)   /* overflow -- discard */
        return;

    msg[n] = '\0';
    msg[n + 1] = '\0';
    cp = msg;

    while (*cp) {
        std::cmatch match;
        if (!strcmp(cp, "SUBSYSTEM=dual_role_usb")) {
            hidl_vec<PortStatus_1_1> currentPortStatus_1_1;
            ALOGE("uevent received %s", cp);
            pthread_mutex_lock(&payload->usb->mLock);
            if (payload->usb->mCallback_1_0 != NULL) {
                sp<IUsbCallback> callback_V1_1 = IUsbCallback::castFrom(payload->usb->mCallback_1_0);
                Return<void> ret;

                // V1_1 callback
                if (callback_V1_1 != NULL) {
                    Status status = getPortStatusHelper(&currentPortStatus_1_1, false);
                    ret = callback_V1_1->notifyPortStatusChange_1_1(
                            currentPortStatus_1_1, status);
                } else { // V1_0 callback
                    Status status = getPortStatusHelper(&currentPortStatus_1_1, true);

                    /*
                     * Copying the result from getPortStatusHelper
                     * into V1_0::PortStatus to pass back through
                     * the V1_0 callback object.
                     */
                    hidl_vec<V1_0::PortStatus> currentPortStatus;
                    currentPortStatus.resize(currentPortStatus_1_1.size());
                    for (unsigned long i = 0; i < currentPortStatus_1_1.size(); i++)
                        currentPortStatus[i] = currentPortStatus_1_1[i].status;

                    ret = payload->usb->mCallback_1_0->notifyPortStatusChange(
                              currentPortStatus, status);
                }
                if (!ret.isOk())
                    ALOGE("error %s", ret.description().c_str());
            } else {
                ALOGI("Notifying userspace skipped. Callback is NULL");
            }
            pthread_mutex_unlock(&payload->usb->mLock);
            break;
        } else if (std::regex_match(cp, match,
            std::regex("add@(/devices/soc/a800000\\.ssusb/a800000\\.dwc3/xhci-hcd\\.0\\.auto/"
                       "usb\\d/\\d-\\d)/.*"))) {
            if (match.size() == 2) {
                std::csub_match submatch = match[1];
                checkUsbDeviceAutoSuspend("/sys" +  submatch.str());
            }
        }
        /* advance to after the next \0 */
        while (*cp++);
    }
}

void* work(void* param) {
    int epoll_fd, uevent_fd;
    struct epoll_event ev;
    int nevents = 0;
    struct data payload;

    ALOGE("creating thread");

    uevent_fd = uevent_open_socket(64*1024, true);

    if (uevent_fd < 0) {
        ALOGE("uevent_init: uevent_open_socket failed\n");
        return NULL;
    }

    payload.uevent_fd = uevent_fd;
    payload.usb = (android::hardware::usb::V1_1::implementation::Usb *)param;

    fcntl(uevent_fd, F_SETFL, O_NONBLOCK);

    ev.events = EPOLLIN;
    ev.data.ptr = (void *)uevent_event;

    epoll_fd = epoll_create(64);
    if (epoll_fd == -1) {
        ALOGE("epoll_create failed; errno=%d", errno);
        goto error;
    }

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, uevent_fd, &ev) == -1) {
        ALOGE("epoll_ctl failed; errno=%d", errno);
        goto error;
    }

    while (!destroyThread) {
        struct epoll_event events[64];

        nevents = epoll_wait(epoll_fd, events, 64, -1);
        if (nevents == -1) {
            if (errno == EINTR)
                continue;
            ALOGE("usb epoll_wait failed; errno=%d", errno);
            break;
        }

        for (int n = 0; n < nevents; ++n) {
            if (events[n].data.ptr)
                (*(void (*)(int, struct data *payload))events[n].data.ptr)
                    (events[n].events, &payload);
        }
    }

    ALOGI("exiting worker thread");
error:
    close(uevent_fd);

    if (epoll_fd >= 0)
        close(epoll_fd);

    return NULL;
}

void sighandler(int sig)
{
    if (sig == SIGUSR1) {
        destroyThread = true;
        ALOGI("destroy set");
        return;
    }
    signal(SIGUSR1, sighandler);
}

Return<void> Usb::setCallback(const sp<V1_0::IUsbCallback> &callback) {

    sp<IUsbCallback> callback_V1_1 = IUsbCallback::castFrom(callback);

    if (callback != NULL)
        if (callback_V1_1 == NULL)
            ALOGI("Registering 1.0 callback");

    pthread_mutex_lock(&mLock);
    /*
     * When both the old callback and new callback values are NULL,
     * there is no need to spin off the worker thread.
     * When both the values are not NULL, we would already have a
     * worker thread running, so updating the callback object would
     * be suffice.
     */
    if ((mCallback_1_0 == NULL && callback == NULL) ||
        (mCallback_1_0 != NULL && callback != NULL)) {
        /*
         * Always store as V1_0 callback object. Type cast to V1_1
         * when the callback is actually invoked.
         */
        mCallback_1_0 = callback;
        pthread_mutex_unlock(&mLock);
        return Void();
    }

    mCallback_1_0 = callback;
    ALOGI("registering callback");

    // Kill the worker thread if the new callback is NULL.
    if (mCallback_1_0 == NULL) {
        pthread_mutex_unlock(&mLock);
        if (!pthread_kill(mPoll, SIGUSR1)) {
            pthread_join(mPoll, NULL);
            ALOGI("pthread destroyed");
        }
        return Void();
    }

    destroyThread = false;
    signal(SIGUSR1, sighandler);

    /*
     * Create a background thread if the old callback value is NULL
     * and being updated with a new value.
     */
    if (pthread_create(&mPoll, NULL, work, this)) {
        ALOGE("pthread creation failed %d", errno);
        mCallback_1_0 = NULL;
    }

    pthread_mutex_unlock(&mLock);
    return Void();
}

// Protects *usb assignment
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
Usb *usb;

Usb::Usb()
        : mLock(PTHREAD_MUTEX_INITIALIZER),
          mRoleSwitchLock(PTHREAD_MUTEX_INITIALIZER) {
    pthread_condattr_t attr;
    if (pthread_condattr_init(&attr)) {
        ALOGE("pthread_condattr_init failed: %s", strerror(errno));
        abort();
    }
    if (pthread_condattr_setclock(&attr, CLOCK_MONOTONIC)) {
        ALOGE("pthread_condattr_setclock failed: %s", strerror(errno));
        abort();
    }
    if (pthread_condattr_destroy(&attr)) {
        ALOGE("pthread_condattr_destroy failed: %s", strerror(errno));
        abort();
    }
    pthread_mutex_lock(&lock);
    // Make this a singleton class
    assert(usb == NULL);
    usb = this;
    pthread_mutex_unlock(&lock);
}

/*
 * whitelisting USB device idProduct and idVendor to allow auto suspend.
 */
static bool canProductAutoSuspend(const std::string &deviceIdVendor,
        const std::string &deviceIdProduct) {
    if (deviceIdVendor == ESSENTIAL_USB_VENDOR_ID_STR &&
        deviceIdProduct == ESSENTIAL_USBC_35_ADAPTER_UNPLUGGED_ID_STR) {
        return true;
    }
    return false;
}

static bool canUsbDeviceAutoSuspend(const std::string &devicePath) {
    std::string deviceIdVendor;
    std::string deviceIdProduct;
    readFile(devicePath + "/idVendor", deviceIdVendor);
    readFile(devicePath + "/idProduct", deviceIdProduct);

    // deviceIdVendor and deviceIdProduct will be empty strings if readFile fails
    return canProductAutoSuspend(deviceIdVendor, deviceIdProduct);
}

/*
 * function to consume USB device plugin events (on receiving a
 * USB device path string), and enable autosupend on the USB device if
 * necessary.
 */
void checkUsbDeviceAutoSuspend(const std::string& devicePath) {
    /*
     * Currently we only actively enable devices that should be autosuspended, and leave others
     * to the default.
     */
    if (canUsbDeviceAutoSuspend(devicePath)) {
        ALOGI("auto suspend usb device %s", devicePath.c_str());
        writeFile(devicePath + "/power/control", "auto");
    }
}

/*
 * function to check if attached device is an audio accessory by
 * querying /sys/bus/usb/devices/1-1\:1.0/sound/card1/id
 */
bool isUsbAudioDevice() {
    std::string retVal;
    int32_t sndRead = readFile("/sys/bus/usb/devices/1-1:1.0/sound/card1/id", retVal);
    if (sndRead < 0) {
        ALOGI("sound card not present on usb device");
        return false;
    }
    return true;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace usb
}  // namespace hardware
}  // namespace android
