#!/bin/sh

ifconfig wlan0 192.168.1.45
#/home/pi/video_udp &
python /home/pi/boot/UDPServer.py -d &
python /home/pi/putty_upload/8266_Stat_UpLoad.py -d &
python /home/pi/code/boot/FindPiServer.py -d &
#/home/pi/putty_upload/tcp_server_1 >/home/pi/logs/esp8266.txt 2>&1 &
#/home/pi/putty_upload/tcp_server_d &
#/home/pi/ftp_cmd/run_cmd1.sh >/home/pi/logs/out.file 2>&1 &
#python /home/pi/UDPServer.py -d
#mono  ../files/nat123linux.sh  service
/home/pi/boot/files/startnat123.sh &
