/*
 * Copyright (C) 2014 STMicroelectronics, Inc.
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

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>
#include <cutils/log.h>
#include <linux/input.h>

#include "configuration.h"
#include "ProximitySensor.h"

//#define FETCH_FULL_EVENT_BEFORE_RETURN 1

/*****************************************************************************/

ProximitySensor::ProximitySensor()
    : SensorBase(NULL, SENSOR_PROXIMITY_DATANAME),
      mEnabled(0),
	  mBatchEnabled(0),
      mInputReader(4),
      mHasPendingEvent(false)
{
    //Added for debug
    ALOGE("ProximitySensor::ProximitySensor gets called!");
	
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = ID_P;
    mPendingEvent.type = SENSOR_TYPE_TIME_OF_FLIGHT;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));

    if (data_fd) {
        strcpy(input_sysfs_path, "/sys/class/input/");
        strcat(input_sysfs_path, "input");
        strcat(input_sysfs_path, input_name+5);
        //strcat(input_sysfs_path, "/device/");
        input_sysfs_path_len = strlen(input_sysfs_path);
		ALOGE("ProximitySensor: input_name:%s, input_sysfs_path:%s",input_name,input_sysfs_path);
        enable(0, 1);
    }
//LOGE("ProximitySensor, data_fd:0x%x, input_sysfs_path:%s\n", data_fd, input_sysfs_path);
}

ProximitySensor::~ProximitySensor() {
    if (mEnabled) {
        enable(0, 0);
    }
}

int ProximitySensor::setInitialState() {
    struct input_absinfo absinfo;
    if (!ioctl(data_fd, EVIOCGABS(EVENT_TYPE_PROXIMITY), &absinfo) ) {
        mHasPendingEvent = true;
	      mPendingEvent.distance = absinfo.value;
    } else {
    }

    return 0;
}

int ProximitySensor::enable(int32_t, int en) {

    //Added for debug
    ALOGE("ProximitySensor::enable function called for stmvl53L0!");

    char buf[2];
    int err;

    int flags = en ? 1 : 0;
    if (flags != mEnabled) {
        int fd;
        strcpy(&input_sysfs_path[input_sysfs_path_len], "/enable_ps_sensor");
        fd = open(input_sysfs_path, O_RDWR);
ALOGE("ProximitySensor::enable filename:%s\n",input_sysfs_path);
		//fd = open(PROXIMITY_ENABLE_FILENAME, O_RDWR);
        if (fd >= 0) {
            buf[1] = 0;
            if (flags) {
                buf[0] = '1';
            } else {
                buf[0] = '0';
            }
            err = write(fd, buf, sizeof(buf));
            close(fd);
            mEnabled = flags;
            setInitialState();
       LOGE("ProximitySensor::enable enabled VL53L0! Error code(2 should indicate success): %d flag %d", err, flags);
            return 0;
        }
        LOGE("ProximitySensor::enable: Failed to open file descriptor for enabling VL53L0");
        LOGE("ProximitySensor::enable: Attempted to open file %s. Error: %d", input_sysfs_path, fd);
        return -1;
    }
    return 0;
}

bool ProximitySensor::hasPendingEvents() const {
    return mHasPendingEvent;
}

int ProximitySensor::batch(int handle, int flags, int64_t period_ns, int64_t timeout)
{
   	int err;

 	if (period_ns > 0)
	{
        int fd;
        strcpy(&input_sysfs_path[input_sysfs_path_len], "/set_delay_ms");
        fd = open(input_sysfs_path, O_RDWR);
		//fd = open(PROXIMITY_SET_DELAY_FILENAME, O_RDWR);
        if (fd >= 0) {
		   	char buf[80];
			sprintf(buf,"%lld", period_ns / 1000000);
            err = write(fd, buf, strlen(buf)+1);
            close(fd);
	    	//Added for debug
	    	LOGE("ProximitySensor::batch return: %d", err);
            return 0;
        }
        LOGE("ProximitySensor::batch: Failed to open file descriptor for set delay time");
        return -1;
    }
    return 0;

}

int ProximitySensor::setDelay(int32_t handle, int64_t delay_ns)
{
    LOGV("ProximitySensor::setDelay called!");
    int fd;
    strcpy(&input_sysfs_path[input_sysfs_path_len], "pollrate_ms");
    fd = open(input_sysfs_path, O_RDWR);
    if (fd >= 0) {
        char buf[80];
        sprintf(buf, "%lld", delay_ns / 1000000);
        write(fd, buf, strlen(buf)+1);
        close(fd);
		//Added for debug
		LOGV("ProximitySensor::setDelay is successful!");
        return 0;
    } else {
}
    LOGV("ProximitySensor::setDelay failed!");
    return -1;
}

int ProximitySensor::readEvents(sensors_event_t* data, int count)
{

    //Added for debug
    if (count < 1)
        return -EINVAL;

    if (mHasPendingEvent) {
        mHasPendingEvent = false;
        mPendingEvent.timestamp = getTimestamp();
        *data = mPendingEvent;
        return mEnabled ? 1 : 0;
    }

    ssize_t n = mInputReader.fill(data_fd);
    if (n < 0){
        LOGE("ProximitySensor::readEvents: Did not successfully read an event, error: %d", n);
        return n;
    }

    int numEventReceived = 0;
    input_event const* event;
	static float dmax=0;
	static float errorcode = 0;
#if FETCH_FULL_EVENT_BEFORE_RETURN
again:
#endif
    LOGV("ProximitySensor::readEvents: At readEvent loop, probably was able to read at least one event successfully");
    while (count && mInputReader.readEvent(&event)) {
        int type = event->type;
		LOGE("event type:0x%x, code:0x%x, value:%u\n",
			event->type, event->code, event->value);

        if (type == EV_ABS) {

			
			if (event->code == ABS_GAS) { //Flush Complete event
			
				mPendingEvent.version = META_DATA_VERSION;
				mPendingEvent.sensor = ID_P;
				mPendingEvent.type = SENSOR_TYPE_META_DATA;
				mPendingEvent.meta_data.what = META_DATA_FLUSH_COMPLETE;
				LOGE("Send META_DATA_FLUSH_COMPLETE event\n");
				
			} else {
				if (mPendingEvent.type != SENSOR_TYPE_TIME_OF_FLIGHT) { 
					//Revert back to normal Event from a Meta Event
					//Update the mPendingEvent data structure for normal event
					mPendingEvent.version = sizeof(sensors_event_t);
		    		mPendingEvent.sensor = ID_P;
		    		mPendingEvent.type = SENSOR_TYPE_TIME_OF_FLIGHT;
		    		memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));
		    		
				} 

	            if (event->code == EVENT_TYPE_PROXIMITY) {
	               if (event->value != -1) {
	                    // FIXME: not sure why we're getting -1 sometimes
	                    //mPendingEvent.distance = indexToValue(event->value);
			    		//mPendingEvent.distance = event->value;
#ifdef ORIGINAL_HAL
			    		mPendingEvent.data[0] = event->value;
#endif
			    		//LOGE("%s: p sensor data (%d)",__func__,event->value);
	                }
	            } 
#ifdef ORIGINAL_HAL
			    else if (event->code == ABS_HAT0X)
					mPendingEvent.data[1] = event->value;
			    else if (event->code == ABS_HAT0Y)
					mPendingEvent.data[2] = event->value;
			    else if (event->code == ABS_HAT1X)
					mPendingEvent.data[3] = event->value;
			    else if (event->code == ABS_HAT1Y) {
					mPendingEvent.data[4] = event->value;
	        	}
			    else if (event->code == ABS_HAT2X)
					mPendingEvent.data[5] = event->value;
			    else if (event->code == ABS_HAT2Y)
					mPendingEvent.data[6] = event->value;
			    else if (event->code == ABS_HAT3X)
					mPendingEvent.data[7] = event->value;
			    else if (event->code == ABS_HAT3Y) {
					mPendingEvent.data[8] = event->value;
	        	}
#else
				else if (event->code == ABS_HAT0X)
					mPendingEvent.data[0] = event->value;
				else if (event->code == ABS_HAT0Y)
					mPendingEvent.data[1] = event->value;
			    else if (event->code == ABS_HAT1X)
					mPendingEvent.data[2] = event->value;
				else if (event->code == ABS_HAT1Y) {
					errorcode = event->value;
	        	}

				#if 1  /* New CODE : Indexes 3,4,5 are updated in the section below */
			    else if (event->code == ABS_HAT2X)
					mPendingEvent.data[6] = event->value;
			    else if (event->code == ABS_HAT2Y)
					mPendingEvent.data[7] = event->value;
			    else if (event->code == ABS_HAT3X)
					mPendingEvent.data[8] = event->value;
				else if (event->code == ABS_WHEEL) 
					mPendingEvent.data[9] = event->value;	
				else if (event->code == ABS_PRESSURE) 
					mPendingEvent.data[10] = event->value;	
				
				#endif /* End of New CODE */
				
				else if (event->code == ABS_HAT3Y) {
					dmax = event->value;

				}
#endif
        	}
        } 

		else if (type == EV_SYN) {
			if (event->code == SYN_DROPPED) {
				//Kernel input sub-system informed there was a buffer overrun
				//Ignore this event, and look for valid events with information
				LOGE("PS sensor : Input device Buffer Overrun\n");
				mInputReader.next();
				continue; //Do not send this event to application
			}
#ifndef ORIGINAL_HAL
LOGE("errorcode: %f,dmax: %f\n", errorcode, dmax);
			if (errorcode == 0) {
				mPendingEvent.data[3] = 1;
				mPendingEvent.data[4] = 0;
				mPendingEvent.data[5] = 0;
			} else if (errorcode <= 4) {
				mPendingEvent.data[3] = 2;
				mPendingEvent.data[4] = dmax;
				mPendingEvent.data[5] = -1;
			} else {
				mPendingEvent.data[3] = 0;
				mPendingEvent.data[4] = 0;
				mPendingEvent.data[5] = -1;
			}
			mPendingEvent.data[11] = errorcode;
#endif
            mPendingEvent.timestamp = timevalToNano(event->time);
            if (mEnabled) {
                *data++ = mPendingEvent;
                numEventReceived++;
                count--;
	        //Added for debug
		ALOGV("ProximitySensor: gets one ps sync event!");
            }
        } else {
            ALOGV("ProximitySensor: unknown event (type=%d, code=%d)",
                    type, event->code);
        }
        mInputReader.next();
    }
#if 0
    LOGE("HAL: distance: %f cm tv_sec: %f tv_usec: %f " 
      "distance: %fmm error_code: %x signalRate_mcps: %f " 
      "rtnAmbRate(kcps): %f rtnConvTime: %f dmax_sq: %f", 
      mPendingEvent.data[0], mPendingEvent.data[1], mPendingEvent.data[2], mPendingEvent.data[3], 
      mPendingEvent.data[4], mPendingEvent.data[5], mPendingEvent.data[6],mPendingEvent.data[7], 
      mPendingEvent.data[8]);
#endif

#if FETCH_FULL_EVENT_BEFORE_RETURN
    /* if we didn't read a complete event, see if we can fill and
       try again instead of returning with nothing and redoing poll. */
    if (numEventReceived == 0 && mEnabled == 1) {
        n = mInputReader.fill(data_fd);
        if (n)
            goto again;
    }
#endif

    return numEventReceived;
}

int ProximitySensor::flush(int32_t handle)
{
	
    int fd = -1;
	int err = -1;
	LOGE("ProximitySensor::flush called\n");

	
    strcpy(&input_sysfs_path[input_sysfs_path_len], "/do_flush");
    fd = open(input_sysfs_path, O_RDWR);
    if (fd >= 0) {
	   	char buf[80];
		sprintf(buf,"%d", 1);
        err = write(fd, buf, strlen(buf)+1);
        close(fd);
    	//Added for debug
    	LOGE("ProximitySensor::flush return: %d\n", err);
        return 0;
    }
    LOGE("ProximitySensor::batch: Failed to open file descriptor for set delay time");
    return -1;	

}
