#!/bin/sh
MMC_STATUS=`cat /proc/jz/mmc`
UDC_STATUS=`cat /proc/jz/udc`
USB_STORAGE_STATUS=`/sbin/lsmod | grep "g_file_storage"`
MMC_DEVICE_NAME=`/sbin/mmc_fat_check`
echo "hello"  > /tmp/cwjia

	  	if [ -n "$USB_STORAGE_STATUS" ] ;then	
			if [ "$ACTION" = "add" ];then
				/sbin/rmmod g_file_storage
				/sbin/rmmod jz4740_udc
				usleep 100000
	  			/sbin/insmod /lib/modules/jz4730_udc.o udc_debug=0
	  			/sbin/insmod /lib/modules/g_file_storage.o file=/dev/ssfdc1,/dev/mmcblk0
			else
				/sbin/rmmod g_file_storage
				/sbin/rmmod jz4740_udc
				usleep 100000
	  			/sbin/insmod /lib/modules/jz4730_udc.o udc_debug=0
	  			/sbin/insmod /lib/modules/g_file_storage.o file=/dev/ssfdc1
			fi
		else
			if [ "$ACTION" = "add" ];then
				MOUNT_STATUS=`cat /proc/mounts | grep "mmc"`
				if [ -z "$MOUNT_STATUS" ] ;then	
					umount -f /mnt/mmc
				        if [ -n "$MMC_DEVICE_NAME" ] ;then
                                           mount -t vfat -o sync,codepage=936,iocharset=cp936 $MMC_DEVICE_NAME /mnt/mmc	
                                        fi  
				else
					PID_FILE=`fuser -m /mnt/mmc`
					if [ -n "$PID_FILE" ] ;then	
						fuser -k -m /mnt/mmc
						usleep 200000
						
						number=0
						while (test $number -lt 10)
						do
							umount -f /mnt/mmc
							usleep 200000
							MOUNT_STATUS=`cat /proc/mounts | grep "mmc"`
							if [ -n "$MOUNT_STATUS" ] ;then
								number=`expr $number + 1`
							else
								number=11
							fi
						done
					else
						umount -f /mnt/mmc
					fi
					usleep 100000
					if [ -n "$MMC_DEVICE_NAME" ];then
                                          mount -t vfat -o sync,codepage=936,iocharset=cp936 $MMC_DEVICE_NAME /mnt/mmc	                              
                                        fi
				fi
			else
				if [ $MMC_STATUS = "REMOVE" ];then
					MOUNT_STATUS=`cat /proc/mounts | grep "mmc"`
					if [ -n "$MOUNT_STATUS" ] ;then
					  PID_FILE=`fuser -m /mnt/mmc`
					  if [ -n "$PID_FILE" ] ;then	
							fuser -k -m /mnt/mmc
							usleep 200000
							
							number=0
							while (test $number -lt 10)
							do
								umount -f /mnt/mmc
								usleep 300000
								MOUNT_STATUS=`cat /proc/mounts | grep "mmc"`
								if [ -n "$MOUNT_STATUS" ] ;then
									number=`expr $number + 1`
								else
									number=11
								fi
							done
						else
							umount -f /mnt/mmc
						fi
					fi
				fi
			fi
		fi
		;;