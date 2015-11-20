from nosuch.midiutil import *
from nosuch.midifile import *
from nosuch.oscutil import *
from nosuch.midiosc import *
from traceback import format_exc
from time import sleep
import sys

def mycallback(ev,d):
	print "mycallback=",ev,d
	for m in ev.oscmsg:
		if len(m) > 3 and ( m[2] == "alive" or m[2] == "fseq" ) :
			# print "Received ALIVE: ",m
			pass
		else:
			print "Received OSC msg: ",m

if __name__ == '__main__':

	if len(sys.argv) != 2:
		print "Usage: osclisten {port@addr}"
		sys.exit(1)

	input_name = sys.argv[1]
	port = re.compile(".*@").search(input_name).group()[:-1]
	host = re.compile("@.*").search(input_name).group()[1:]

	oscmon = OscMonitor(host,port)
	oscmon.setcallback(mycallback,"")

	sleep(10000)
