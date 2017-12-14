#!/system/bin/sh
export PATH=$PATH:/system/bin/sh:/sbin:/vendor/bin:/system/sbin:/system/bin:/system/xbin
if [ "$1" == "off" ]; then
	hbtp_daemon tool sendCommand enableGripRejection 0
else
	hbtp_daemon tool sendCommand enableGripRejection 1
fi
