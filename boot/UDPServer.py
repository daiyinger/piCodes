#!/usr/bin/python
# UDP Echo Server -  udpserver.py
# code by www.cppblog.com/jerryma
import socket, traceback
import time

host = ''
port = 10001
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind((host, port))
addr = ("192.168.1.100",4097)
while True:
        message, address = s.recvfrom(8192)
 	if addr == address:
		continue
	else:
        	fp = open("/home/pi/logs.txt",'a')
        	curTime = time.strftime("%Y-%m-%d %H:%M:%S ", time.localtime(time.time()))
        	fp.write(curTime)
		fp.write("Got data from:"+str(address)+"\n")
        	fp.write(message.decode()+"\n");
        	fp.close()
        #print ("Got data from", address, ": ", message)
        #s.sendto(message, address)
