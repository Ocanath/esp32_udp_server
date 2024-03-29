import socket
import time
import struct 
import numpy as np

hostname = socket.gethostname()
addr=socket.gethostbyname(hostname)
print("Host IP Addr: "+addr)
addrmod = addr
dotidx = len(addrmod)-1
for i in range(len(addrmod)-1,0, -1):
	if(addrmod[i] == '.'):
		dotidx = i
		break
lastoctet = addrmod[dotidx+1:len(addrmod)]
addrmod = addrmod.replace(lastoctet,"255")
print("Using: "+addrmod)
udp_server_addr = (addrmod, 34345)
bufsize = 512
client_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
client_socket.settimeout(0.0) #make non blocking

start_time = time.time()

try:
	barr = bytearray("marco",encoding='utf8')
	client_socket.sendto(barr,udp_server_addr)		
except KeyboardInterrupt:
	print("some fkn error")

found = 0
while(time.time()-start_time < 3):
	try:
		pkt,addr = client_socket.recvfrom(bufsize)
		if(len(pkt) > 0):
			print("received: "+str(pkt)+"from: ",str(addr))
			found = 1
		break
	except:
		pass
if(found == 0):
	print("no response")