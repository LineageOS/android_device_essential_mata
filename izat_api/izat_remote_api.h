/*=============================================================================
  Copyright (c) 2016, The Linux Foundation. All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above
  copyright notice, this list of conditions and the following
    disclaimer in the documentation and/or other materials provided
    with the distribution.
  * Neither the name of The Linux Foundation nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
  ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
  OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  =============================================================================*/

#ifndef __IZAT_REMOTE_APIS_H__
#define __IZAT_REMOTE_APIS_H__

#include <gps_extended_c.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*locationUpdateCb)(UlpLocation *location,
                                 GpsLocationExtended *locExtended,
                                 void* clientData);

/* registers a client callback for listening to location updates
   locationCb - location callback function pointer implemented by client
   clientData - an opaque data pointer from client. This pointer will be
                provided back when the locationCb() callbacak is called.
                This can be used by client to store application context
                or statemachine etc. This parameter can be NULL.
   return: an opaque pointer that serves as a request handle. This handle
           os tp be fed to theunregisterLocationUpdater() call.
*/
void* registerLocationUpdater(locationUpdateCb locationCb, void* clientData);

/* unregisters the client callback
   locationUpdaterHandle - the opaque pointer from the return of
                           registerLocationUpdater()
*/
void unregisterLocationUpdater(void* locationUpdaterHandle);

#ifdef __cplusplus
} // extern "C"
#endif

#endif //__IZAT_REMOTE_APIS_H__
