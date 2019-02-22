#! /vendor/bin/sh

echo 1 > /sys/kernel/boot_adsp/boot
echo 1 > /sys/kernel/boot_cdsp/boot
echo 1 > /sys/kernel/boot_slpi/boot
setprop vendor.qcom.devup 1

version=`grep -ao "OEM_IMAGE_VERSION_STRING[ -~]*" \
              /firmware/image/slpi_v2.b04 | \
         sed -e s/OEM_IMAGE_VERSION_STRING=SLPI.version.// -e s/\(.*\).//`
setprop sys.slpi.firmware.version "$version"
