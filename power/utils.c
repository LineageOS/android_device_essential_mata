/*
 * Copyright (c) 2012-2013,2015-2016, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * *    * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
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
#define LOG_NIDEBUG 0

#include <dlfcn.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"
#include "list.h"
#include "hint-data.h"
#include "power-common.h"

#define LOG_TAG "QCOM PowerHAL"
#include <utils/Log.h>

static void *qcopt_handle;
static int (*perf_lock_acq)(unsigned long handle, int duration,
    int list[], int numArgs);
static int (*perf_lock_rel)(unsigned long handle);
static struct list_node active_hint_list_head;

static void *get_qcopt_handle()
{
    char qcopt_lib_path[PATH_MAX] = {0};
    void *handle = NULL;

    dlerror();

    if (property_get("ro.vendor.extension_library", qcopt_lib_path,
                NULL)) {
        handle = dlopen(qcopt_lib_path, RTLD_NOW);
        if (!handle) {
            ALOGE("Unable to open %s: %s\n", qcopt_lib_path,
                    dlerror());
        }
    }

    return handle;
}

static void __attribute__ ((constructor)) initialize(void)
{
    qcopt_handle = get_qcopt_handle();

    if (!qcopt_handle) {
        ALOGE("Failed to get qcopt handle.\n");
    } else {
        /*
         * qc-opt handle obtained. Get the perflock acquire/release
         * function pointers.
         */
        perf_lock_acq = dlsym(qcopt_handle, "perf_lock_acq");

        if (!perf_lock_acq) {
            ALOGE("Unable to get perf_lock_acq function handle.\n");
        }

        perf_lock_rel = dlsym(qcopt_handle, "perf_lock_rel");

        if (!perf_lock_rel) {
            ALOGE("Unable to get perf_lock_rel function handle.\n");
        }
    }
}

static void __attribute__ ((destructor)) cleanup(void)
{
    if (qcopt_handle) {
        if (dlclose(qcopt_handle))
            ALOGE("Error occurred while closing qc-opt library.");
    }
}

int sysfs_read(char *path, char *s, int num_bytes)
{
    char buf[80];
    int count;
    int ret = 0;
    int fd = open(path, O_RDONLY);

    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error opening %s: %s\n", path, buf);

        return -1;
    }

    if ((count = read(fd, s, num_bytes - 1)) < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", path, buf);

        ret = -1;
    } else {
        s[count] = '\0';
    }

    close(fd);

    return ret;
}

int sysfs_write(char *path, char *s)
{
    char buf[80];
    int len;
    int ret = 0;
    int fd = open(path, O_WRONLY);

    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error opening %s: %s\n", path, buf);
        return -1 ;
    }

    len = write(fd, s, strlen(s));
    if (len < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", path, buf);

        ret = -1;
    }

    close(fd);

    return ret;
}

int get_scaling_governor(char governor[], int size)
{
    if (sysfs_read(SCALING_GOVERNOR_PATH, governor,
                size) == -1) {
        // Can't obtain the scaling governor. Return.
        return -1;
    } else {
        // Strip newline at the end.
        int len = strlen(governor);

        len--;

        while (len >= 0 && (governor[len] == '\n' || governor[len] == '\r'))
            governor[len--] = '\0';
    }

    return 0;
}

int is_interactive_governor(char* governor) {
   if (strncmp(governor, INTERACTIVE_GOVERNOR, (strlen(INTERACTIVE_GOVERNOR)+1)) == 0)
      return 1;
   return 0;
}

void interaction(int duration, int num_args, int opt_list[])
{
#ifdef INTERACTION_BOOST
    static int lock_handle = 0;

    if (duration < 0 || num_args < 1 || opt_list[0] == 0)
        return;

    if (qcopt_handle) {
        if (perf_lock_acq) {
            lock_handle = perf_lock_acq(lock_handle, duration, opt_list, num_args);
            if (lock_handle == -1)
                ALOGE("Failed to acquire lock.");
        }
    }
#endif
}

int interaction_with_handle(int lock_handle, int duration, int num_args, int opt_list[])
{
#ifdef INTERACTION_BOOST
    if (duration < 0 || num_args < 1 || opt_list[0] == 0)
        return 0;

    if (qcopt_handle) {
        if (perf_lock_acq) {
            lock_handle = perf_lock_acq(lock_handle, duration, opt_list, num_args);
            if (lock_handle == -1)
                ALOGE("Failed to acquire lock.");
        }
    }
    return lock_handle;
#else
    return 0;
#endif
}

void release_request(int lock_handle) {
    if (qcopt_handle && perf_lock_rel)
        perf_lock_rel(lock_handle);
}

void perform_hint_action(int hint_id, int resource_values[], int num_resources)
{
    if (qcopt_handle) {
        struct hint_data temp_hint_data = {
            .hint_id = hint_id
        };
        struct list_node *found_node = find_node(&active_hint_list_head,
                                                 &temp_hint_data);
        if (found_node) {
            ALOGE("hint ID %d already active", hint_id);
            return;
        }
        if (perf_lock_acq) {
            /* Acquire an indefinite lock for the requested resources. */
            int lock_handle = perf_lock_acq(0, 0, resource_values,
                    num_resources);

            if (lock_handle == -1) {
                ALOGE("%s: Failed to acquire lock.", __func__);
            } else {
                /* Add this handle to our internal hint-list. */
                struct hint_data *new_hint =
                    (struct hint_data *)malloc(sizeof(struct hint_data));

                if (new_hint) {
                    if (!active_hint_list_head.compare) {
                        active_hint_list_head.compare =
                            (int (*)(void *, void *))hint_compare;
                        active_hint_list_head.dump = (void (*)(void *))hint_dump;
                    }

                    new_hint->hint_id = hint_id;
                    new_hint->perflock_handle = lock_handle;

                    if (add_list_node(&active_hint_list_head, new_hint) == NULL) {
                        free(new_hint);
                        /* Can't keep track of this lock. Release it. */
                        if (perf_lock_rel)
                            perf_lock_rel(lock_handle);

                        ALOGE("Failed to process hint.");
                    }
                } else {
                    /* Can't keep track of this lock. Release it. */
                    if (perf_lock_rel)
                        perf_lock_rel(lock_handle);

                    ALOGE("Failed to process hint.");
                }
            }
        }
    }
}

void undo_hint_action(int hint_id)
{
    if (qcopt_handle) {
        if (perf_lock_rel) {
            /* Get hint-data associated with this hint-id */
            struct list_node *found_node;
            struct hint_data temp_hint_data = {
                .hint_id = hint_id
            };

            found_node = find_node(&active_hint_list_head,
                    &temp_hint_data);

            if (found_node) {
                /* Release this lock. */
                struct hint_data *found_hint_data =
                    (struct hint_data *)(found_node->data);

                if (found_hint_data) {
                    if (perf_lock_rel(found_hint_data->perflock_handle) == -1)
                        ALOGE("Perflock release failed: %d", hint_id);
                }

                if (found_node->data) {
                    /* We can free the hint-data for this node. */
                    free(found_node->data);
                }

                remove_list_node(&active_hint_list_head, found_node);
                ALOGV("Undo of hint ID %d succeeded", hint_id);
            } else {
                ALOGE("Invalid hint ID: %d", hint_id);
            }
        }
    }
}

/*
 * Used to release initial lock holding
 * two cores online when the display is on
 */
void undo_initial_hint_action()
{
    if (qcopt_handle) {
        if (perf_lock_rel) {
            perf_lock_rel(1);
        }
    }
}
