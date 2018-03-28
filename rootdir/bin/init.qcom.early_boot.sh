#! /vendor/bin/sh

# Copyright (c) 2012-2013,2016 The Linux Foundation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of The Linux Foundation nor
#       the names of its contributors may be used to endorse or promote
#       products derived from this software without specific prior written
#       permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

export PATH=/vendor/bin

log -t BOOT -p i "Essential Mata target '$1', SoC '$soc_hwplatform', HwID '$soc_hwid', SoC ver '$soc_hwver'"

# Set carrier specific properties as per sku
sku=`getprop ro.boot.carrier.sku`
if [ "$sku" == "SOFTBANK" ]; then
    ## system properties for Softbank only
    setprop ro.vendor.ril.dtmf_interval 101
    setprop ro.vendor.ril.dtmf_pause_interval 215
    setprop audio.camerasound.force true
    setprop ro.com.google.clientidbase.am android-softbank-jp
    setprop persist.vendor.radio.reject_cause_21 1
    setprop persist.vendor.radio.reject_cause_603 1
    setprop ro.cdma.home.operator.alpha softbank
    setprop ro.product.model Iron
    setprop ro.product.manufacturer Heroes
fi
# Since an ro.* cannot be set twice, we can just call this directly
setprop ro.product.model PH-1
setprop ro.product.manufacturer "Essential Products"

# End of carrier specific properties

setprop ro.alarm_boot false

# copy GPU frequencies to system property
if [ -f /sys/class/kgsl/kgsl-3d0/gpu_available_frequencies ]; then
    gpu_freq=`cat /sys/class/kgsl/kgsl-3d0/gpu_available_frequencies` 2> /dev/null
    setprop ro.gpu.available_frequencies "$gpu_freq"
fi
