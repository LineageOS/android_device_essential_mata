# Android fstab file.
# The filesystem that contains the filesystem checker binary (typically /system) cannot
# specify MF_CHECK, and must come before any filesystems that do specify MF_CHECK

#<src>                                                  <mnt_point>        <type> <mnt_flags and options>                          <fs_mgr_flags>
/dev/block/platform/soc/1da4000.ufshc/by-name/system    /                  ext4   ro,barrier=1                                     wait,slotselect,verify
/dev/block/platform/soc/1da4000.ufshc/by-name/userdata  /data              ext4   noatime,nosuid,nodev,barrier=1,noauto_da_alloc   latemount,wait,check,formattable,fileencryption=ice,quota
/dev/block/platform/soc/1da4000.ufshc/by-name/misc      /misc              emmc   defaults                                         defaults
/dev/block/platform/soc/1da4000.ufshc/by-name/modem     /vendor/firmware_mnt vfat ro,shortname=lower,uid=1000,gid=1000,dmask=222,fmask=333,context=u:object_r:firmware_file:s0 wait,slotselect
/dev/block/platform/soc/1da4000.ufshc/by-name/dsp       /vendor/lib/dsp    ext4   ro,nosuid,nodev,barrier=1,context=u:object_r:adsprpcd_file:s0 wait,slotselect
/devices/*/xhci-hcd.0.auto*                             auto               auto   defaults                                         voldmanaged=usb:auto
/devices/*/0000:01:00.0*                                auto               auto   defaults                                         voldmanaged=usb:auto
/dev/block/zram0                                        none               swap   defaults                                         zramsize=536870912,max_comp_streams=8
