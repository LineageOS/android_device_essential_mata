/*
 * Copyright (C) 2014 STMicroelectronics, Inc.
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef STM_SENSORS_CONFIGURATION_H
#define STM_SENSORS_CONFIGURATION_H

#include <stdint.h>
#include <errno.h>
#include <sys/cdefs.h>
#include <sys/types.h>



__BEGIN_DECLS

#define SENSOR_PROXIMITY_LABEL 			"Time of Flight Proximity Sensor"
#define SENSOR_PROXIMITY_DATANAME		"STM VL53L0 proximity sensor"
//#define PROXIMITY_ENABLE_FILENAME		"/sys/kernel/range/enable_ps_sensor"
//#define PROXIMITY_SET_DELAY_FILENAME	"/sys/kernel/range/set_delay_ms"
#define PROXIMITY_MAX_RANGE				819
#define PROXIMITY_POWER_CONSUMPTION		18
#define PROXIMITY_MIN_DELAY				30000 //in microseconds
#define PROXIMITY_FIFO_RESERVED_COUNT	0
#define PROXIMITY_FIFO_MAX_COUNT		0
#define SENSOR_TYPE_TIME_OF_FLIGHT      (40)
__END_DECLS

#endif  // ANDROID_SENSORS_H
