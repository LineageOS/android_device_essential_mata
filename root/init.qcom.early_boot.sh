#!/system/bin/sh
# Copyright (c) 2012-2013,2016-2017 The Linux Foundation. All rights reserved.
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

export PATH=/system/bin

# Set platform variables
if [ -f /sys/devices/soc0/hw_platform ]; then
    soc_hwplatform=`cat /sys/devices/soc0/hw_platform` 2> /dev/null
else
    soc_hwplatform=`cat /sys/devices/system/soc/soc0/hw_platform` 2> /dev/null
fi
if [ -f /sys/devices/soc0/soc_id ]; then
    soc_hwid=`cat /sys/devices/soc0/soc_id` 2> /dev/null
else
    soc_hwid=`cat /sys/devices/system/soc/soc0/id` 2> /dev/null
fi
if [ -f /sys/devices/soc0/platform_version ]; then
    soc_hwver=`cat /sys/devices/soc0/platform_version` 2> /dev/null
else
    soc_hwver=`cat /sys/devices/system/soc/soc0/platform_version` 2> /dev/null
fi

if [ -f /sys/class/graphics/fb0/virtual_size ]; then
    res=`cat /sys/class/graphics/fb0/virtual_size` 2> /dev/null
    fb_width=${res%,*}
fi

log -t BOOT -p i "Essential Mata, SoC '$soc_hwplatform', HwID '$soc_hwid', SoC ver '$soc_hwver'"

function set_density_by_fb_mata() {
    #put default density based on width
    if [ -z $fb_width ]; then
        setprop ro.sf.lcd_density 320
    else
        if [ $fb_width -ge 1312 ]; then
            # Our real DPI is 504, set it to 480 to use a
            # predefined value
            setprop ro.sf.lcd_density 480 # For Mata
        fi
    fi
}
target=`getprop ro.board.platform`
project=`getprop ro.boot.device`

set_density_by_fb_mata
setprop persist.sys.force_sw_gles 0

# Setup display nodes & permissions
# HDMI can be fb1 or fb2
# Loop through the sysfs nodes and determine
# the HDMI(dtv panel)

function set_perms() {
    #Usage set_perms <filename> <ownership> <permission>
    chown -h $2 $1
    chmod $3 $1
}

function setHDMIPermission() {
   file=/sys/class/graphics/fb$1
   dev_file=/dev/graphics/fb$1
   dev_gfx_hdmi=/devices/virtual/switch/hdmi

   set_perms $file/hpd system.graphics 0664
   set_perms $file/res_info system.graphics 0664
   set_perms $file/vendor_name system.graphics 0664
   set_perms $file/product_description system.graphics 0664
   set_perms $file/video_mode system.graphics 0664
   set_perms $file/format_3d system.graphics 0664
   set_perms $file/s3d_mode system.graphics 0664
   set_perms $file/dynamic_fps system.graphics 0664
   set_perms $file/msm_fb_dfps_mode system.graphics 0664
   set_perms $file/hdr_stream system.graphics 0664
   set_perms $file/cec/enable system.graphics 0664
   set_perms $file/cec/logical_addr system.graphics 0664
   set_perms $file/cec/rd_msg system.graphics 0664
   set_perms $file/pa system.graphics 0664
   set_perms $file/cec/wr_msg system.graphics 0600
   set_perms $file/hdcp/tp system.graphics 0664
   set_perms $file/hdmi_audio_cb audioserver.audio 0600
   ln -s $dev_file $dev_gfx_hdmi
}

# check for HDMI connection
for fb_cnt in 0 1 2 3
do
    file=/sys/class/graphics/fb$fb_cnt/msm_fb_panel_info
    if [ -f "$file" ]
    then
      cat $file | while read line; do
        case "$line" in
            *"is_pluggable"*)
             case "$line" in
                  *"1"*)
                  setHDMIPermission $fb_cnt
             esac
        esac
      done
    fi
done



# check for mdp caps
file=/sys/class/graphics/fb0/mdp/caps
if [ -f "$file" ]
then
    setprop debug.gralloc.enable_fb_ubwc 0
    setprop debug.gralloc.gfx_ubwc_disable 1
    cat $file | while read line; do
      case "$line" in
                *"ubwc"*)
                setprop debug.gralloc.enable_fb_ubwc 1
                setprop debug.gralloc.gfx_ubwc_disable 0
            esac
    done
fi

file=/sys/class/graphics/fb0
if [ -d "$file" ]
then
        set_perms $file/idle_time system.graphics 0664
        set_perms $file/dynamic_fps system.graphics 0664
        set_perms $file/dyn_pu system.graphics 0664
        set_perms $file/modes system.graphics 0664
        set_perms $file/mode system.graphics 0664
        set_perms $file/msm_cmd_autorefresh_en system.graphics 0664
fi

# set lineptr permissions for all displays
for fb_cnt in 0 1 2 3
do
    file=/sys/class/graphics/fb$fb_cnt
    if [ -f "$file/lineptr_value" ]; then
        set_perms $file/lineptr_value system.graphics 0664
    fi
    if [ -f "$file/msm_fb_persist_mode" ]; then
        set_perms $file/msm_fb_persist_mode system.graphics 0664
    fi
done

boot_reason=`cat /proc/sys/kernel/boot_reason`
reboot_reason=`getprop ro.boot.alarmboot`
power_off_alarm_file=`cat /persist/alarm/powerOffAlarmSet`
if [ "$boot_reason" = "3" ] || [ "$reboot_reason" = "true" ]; then
    if [ "$power_off_alarm_file" = "1" ]
    then
        setprop ro.alarm_boot true
        setprop debug.sf.nobootanimation 1
    fi
else
    setprop ro.alarm_boot false
fi

# copy GPU frequencies to system property
if [ -f /sys/class/kgsl/kgsl-3d0/gpu_available_frequencies ]; then
    gpu_freq=`cat /sys/class/kgsl/kgsl-3d0/gpu_available_frequencies` 2> /dev/null
    setprop ro.gpu.available_frequencies "$gpu_freq"
fi
