# Android fstab file.
# The filesystem that contains the filesystem checker binary (typically /system) cannot
# specify MF_CHECK, and must come before any filesystems that do specify MF_CHECK

#TODO: Add 'check' as fs_mgr_flags with data partition.
# Currently we dont have e2fsck compiled. So fs check would failed.

# A/B fstab.qcom variant
#<src>                                   <mnt_point>        <type> <mnt_flags and options>                          <fs_mgr_flags>
/dev/block/zram0                         none               swap   defaults                                         zramsize=536870912,max_comp_streams=8

/dev/block/bootdevice/by-name/system     /                  ext4   ro,barrier=1                                     wait,slotselect,verify
/dev/block/bootdevice/by-name/userdata   /data              ext4   noatime,nosuid,nodev,barrier=1,noauto_da_alloc   latemount,wait,check,formattable,fileencryption=ice,quota

/dev/block/bootdevice/by-name/bluetooth  /bt_firmware       vfat   ro,shortname=lower,uid=1002,gid=3002,dmask=227,fmask=337,context=u:object_r:bt_firmware_file:s0 wait,slotselect
/dev/block/bootdevice/by-name/dsp        /dsp               ext4   ro,nosuid,nodev,barrier=1,context=u:object_r:adsprpcd_file:s0 wait,slotselect
/dev/block/bootdevice/by-name/misc       /misc              emmc   defaults                                         defaults
/dev/block/bootdevice/by-name/modem      /firmware          vfat   ro,shortname=lower,uid=1000,gid=1000,dmask=222,fmask=333,context=u:object_r:firmware_file:s0 wait,slotselect
/dev/block/bootdevice/by-name/persist    /persist           ext4   noatime,nosuid,nodev,barrier=1                   wait

/devices/*/xhci-hcd.0.auto*              auto               auto   defaults                                         voldmanaged=usb:auto
/devices/*/0000:01:00.0*                 auto               auto   defaults                                         voldmanaged=usb:auto
