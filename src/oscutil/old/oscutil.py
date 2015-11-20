"""
This module provides event support for Osc things.
"""

import time
import socket
import traceback

from traceback import format_exc
from threading import Thread
from osc.oscAPI import *
from osc.OSC import *

# See http://www.faqs.org/rfcs/rfc1055.html
SLIP_END = chr(192)
SLIP_ESC = chr(219)
SLIP_ESC2 = chr(221)

def nextSLIPMsg(data):
	first = data[0]
	if first != SLIP_END:
            raise Exception, "In nextSLIPMsg, data did not begin/end with SLIP_END"
	sofar = 1
	dataleng = len(data)
	# print "Start nextSlipMsg dataleng=",dataleng
	while sofar < dataleng:
		# print "sofar=",sofar
		i = data.find(SLIP_END,sofar)
		if i < 0:
			# print "RETURNING EMPTY message from nextSlipMsg"
			return ("",data)
		# Found a SLIP_END, but we want to skip over escaped SLIP_END's
		if i>0 and data[i-1] == SLIP_ESC:
			sofar = i+1
			# print "SKIPPING escaped END! sofar=",sofar
			continue
		# Found a real SLIP_END, pull of message and return it
		rest = data[i+1:]
		b = data[1:i]
		# print "RETURNING len(b)=",len(b)," len(rest)",len(rest)
		return (b,rest)

	return ("",data)

def encodeSLIP(b):
	b = b.replace(SLIP_ESC, SLIP_ESC + SLIP_ESC2)
	b = b.replace(SLIP_END, SLIP_ESC + SLIP_END)
	# doubled-ended version
        final = SLIP_END + b + SLIP_END
        return final

class BaseEvent:
	def __init__(self):
		self.time = 0.0

class OscEvent(BaseEvent):

	def __init__(self,msg,tm=0.0,source_ip="undefined"):
		BaseEvent.__init__(self)
		self.oscmsg = msg
		self.source_ip = source_ip

	def __str__(self):
		return self.to_xml()

	def to_xml(self):
		return '<event time=%.3f>%s</event>' % (self.time,self.oscmsg)

	def to_osc(self):
		return self.oscmsg

class OscRecipient:
	def __init__(self,host,port,proto="udp",initfunc=None):
		self.osc_host = host
		self.osc_port = int(port)
		self.proto = proto
		self.initfunc = initfunc
		if proto == "udp":
			self.osc_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
		elif proto == "tcp":
			self.connected = False
		else:
			raise RuntimeError, "Unexpected proto value in OscRecipient"

	def close(self):
		self.osc_socket.close()
		self.osc_socket = None
		self.connected = False

	def sendosc(self,oscaddr,oscmsg=[]):
		if self.proto == "udp":
			b = createBinaryMsg(oscaddr,oscmsg)
			self.sendbinary(b)
		elif self.proto == "tcp":
			b = createBinaryMsg(oscaddr,oscmsg)
			b = encodeSLIP(b)
			# b = createBinaryMsg(oscaddr,oscmsg,encoder="SLIP")
			self.sendbinary(b)
		else:
			raise RuntimeError, "Unexpected proto value in OscRecipient.sendosc()"

	def sendto(self,b,addrport):
		raise RuntimeError, "OscRecipient.sendto is obsolete!"

	def sendbinary(self,b):
		if self.proto == "udp":
			self.osc_socket.sendto(b,(self.osc_host,self.osc_port))
		elif self.proto == "tcp":
			if not self.connected:
				self.osc_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
				hostport = (self.osc_host,self.osc_port)
				try:
					self.osc_socket.connect(hostport)
					self.connected = True
					if self.initfunc != None:
						self.initfunc()
				except:
					raise RuntimeError, "Unable to connect to %s:%d - %s" % (self.osc_host,self.osc_port,format_exc())
					return
			self.send_stream(b)
		else:
			raise RuntimeError, "Unexpected proto value in OscRecipient.sendbinary()"

        def send_stream(self,msg):
		msglen = len(msg)
		totalsent = 0
		while totalsent < msglen:
			sent = self.osc_socket.send(msg[totalsent:])
			if sent == 0:
				raise RuntimeError, "socket connection broken"
			totalsent = totalsent + sent
			
class OscRecipientRestartable(OscRecipient):

	def __init__(self,addr,port,proto,initfunc):
		OscRecipient.__init__(self,addr,port,proto=proto,initfunc=initfunc)
		self.initfunc = initfunc

	def sendosc(self, msg_addr, msg_data):
		try: 
			OscRecipient.sendosc(self,msg_addr,msg_data)
		except:
			if self.proto == "udp":
				raise RuntimeError, "Exception: "+format_exc()
			print "Retrying TCP connection to %d@%s" % (self.osc_port,self.osc_host)
			self.close();
			try:
				OscRecipient.sendosc(self,msg_addr,msg_data)
				print "Retry worked!"
			except:
				print "Retry failed..."
				raise RuntimeError, "Exception: "+format_exc()

class OscMonitor:

	def __init__(self,addr,port,proto="udp"):
		if proto != "udp" and proto != "tcp":
			raise RuntimeError, "Unexpected proto in OscMonitor"
		# if port is 0, one is chosen for us
		self.thread = OscThread(addr,port,proto=proto)
		self.thread.start()

	def getport(self):
		return self.thread.getport()

	def time_now(self):
		return time.time()

	def callback(self,f,data=None):
		return self.thread.callback(f,data)
		
class OscThread(Thread):

	def __init__(self,host,port,proto):
		Thread.__init__(self)
		self.host = host
		self.port = int(port)
		self.proto = proto
		self.setDaemon(True)
		self.callback_func = None
		self.callback_data = None

		if proto == "udp":
			self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
			self.sock.bind((self.host,self.port))
		elif proto == "tcp":
			self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
			self.sock.bind((self.host,self.port))
			self.sock.listen(5)
		else:
			raise RuntimeError, "Unexpected proto in OscThread"
		if self.port == 0:
			sockname = self.sock.getsockname()
			self.port = sockname[1]
		# addressManager = OSC.CallbackManager()
		# addressManager.add(self.handle_register,"/registerclient")
		# addressManager.add(self.handle_unregister,"/unregisterclient")

	def getport(self):
		return self.port

	def callback(self,f,data):
		self.callback_func = f
		self.callback_data = data

	def docallback(self,t,fromhost):
		try:
			if self.callback_func != None:
				e = OscEvent(t,source_ip=fromhost)
				self.callback_func(e,self.callback_data)
		except:
			print "Error in callback on OSC message - %s"%(format_exc())

	def run(self):
		print "Listening for OSC on host =",self.host," port =",self.port," proto = ",self.proto
		recvsize = 8192
		while True:
			if self.proto == "udp":
				pair = self.sock.recvfrom(recvsize)
				t = decodeOSC(pair[0])
				fromhost = pair[1][0]
				self.docallback(t,fromhost)
			else:
				conn, fromhost = self.sock.accept()
				# print "ACCEPTED!! fromhost=",fromhost
				# Should probably spawn a thread
				try:
					self.handle_tcp_connection(conn,fromhost,recvsize)
				except:
					print "Error in handle_tcp_connection - %s"%(format_exc())



	def findnextstart(self,buff):
		buffleng = len(buff)
		for n in range(1,buffleng):
			if buff[n]==SLIP_END and buff[n-1]!=SLIP_ESC:
				print "FOUND SLIP_END (next start) at n=",n
				if n < (buffleng-1) and buff[n+1] == SLIP_END:
					buff = buff[n+1:]
				else:
					buff = buff[n:]
				break
		return buff

	def handle_tcp_connection(self,conn,fromhost,recvsize):
		buff = ""
		# print "HANDLE_TCP_CONNECTION fromhost=",fromhost
		while True:
			data = conn.recv(recvsize)
			if len(data) == 0:
				break
			buff += data

			if buff[0] != SLIP_END:
				print "HEY! buffer doesn't begin with END, scanning for non-escaped SLIP_END"
				buff = self.findnextstart(buff)

			# print "Received data len=",len(data)," buff len=",len(buff)
			while len(buff)>2 and buff[0]==SLIP_END:
				(b,buff) = nextSLIPMsg(buff)
				if len(b) == 0:
					break
				b = b.replace(SLIP_ESC+SLIP_END,SLIP_END)
				b = b.replace(SLIP_ESC+SLIP_ESC2,SLIP_ESC)
				# print "Decoding osc len=",len(b)," buff len is now ",len(buff)
				t = decodeOSC(b)
				self.docallback(t,fromhost)
		# print "CLOSED fromhost=",fromhost
		conn.close()

