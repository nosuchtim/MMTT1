# This renames debug.txt to a timestamped name, and creates a fresh debug.txt

import time
import os
import socket
from subprocess import call

hostname = socket.gethostname()

os.chdir("c:/local/manifold/bin/ffglplugins")

fname = hostname + "_" + time.strftime("debug_20%y_%m_%d_%H_%M_%S.txt")

try:
	os.rename("debug.txt",fname)
	call(["c:/python27/python.exe","c:/local/manifold/bin/mailhome.py",fname])
except:
	pass
try:
	f = open("debug.txt","w")
	f.close()
except:
	pass


