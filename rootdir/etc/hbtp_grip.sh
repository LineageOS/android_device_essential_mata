#!/system/bin/sh

if [ "$1" == "off" ]; then
	/system/vendor/bin/hbtp_daemon tool sendCommand enableGripRejection 0
else
	/system/vendor/bin/hbtp_daemon tool sendCommand enableGripRejection 1
fi
