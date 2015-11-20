#
# This script takes a Params.list file and generates all the .h files
# for Region and Sprite parameters.  This allows new parameters to be added
# just by editing the Params.list file and re-running this script.
# Originally the parameters were represented in generalized lists,
# but looking them up at execution time was expensive (as determined
# by profilng), so now the parameters are all explicit members
# of the structures.

import sys
import os
import re

def generate(listfile):

	try:
		f = open(listfile)
	except:
		print("Unable to open "+listfile)
		sys.exit(1)

	out_ro_declare = open("RegionOverrides_declare.h","w")
	out_ro_get = open("RegionOverrides_get.h","w")
	out_ro_init = open("RegionOverrides_init.h","w")
	out_ro_list = open("RegionOverrides_list.h","w")
	out_ro_set = open("RegionOverrides_set.h","w")
	out_rp_declare = open("RegionParams_declare.h","w")
	out_rp_get = open("RegionParams_get.h","w")
	out_rp_increment = open("RegionParams_increment.h","w")
	out_rp_init = open("RegionParams_init.h","w")
	out_rp_issprite = open("RegionParams_issprite.h","w")
	out_rp_list = open("RegionParams_list.h","w")
	out_rp_set = open("RegionParams_set.h","w")
	out_rp_toggle = open("RegionParams_toggle.h","w")

	out_co_declare = open("ChannelOverrides_declare.h","w")
	out_co_get = open("ChannelOverrides_get.h","w")
	out_co_init = open("ChannelOverrides_init.h","w")
	out_co_list = open("ChannelOverrides_list.h","w")
	out_co_set = open("ChannelOverrides_set.h","w")
	out_cp_declare = open("ChannelParams_declare.h","w")
	out_cp_get = open("ChannelParams_get.h","w")
	out_cp_increment = open("ChannelParams_increment.h","w")
	out_cp_init = open("ChannelParams_init.h","w")
	out_cp_issprite = open("ChannelParams_issprite.h","w")
	out_cp_list = open("ChannelParams_list.h","w")
	out_cp_set = open("ChannelParams_set.h","w")
	out_cp_toggle = open("ChannelParams_toggle.h","w")

	out_ch_reset = open("Channel_reset.h","w")
	out_rg_reset = open("Region_reset.h","w")

	out_sp_declare = open("SpriteParams_declare.h","w")
	out_sp_init = open("SpriteParams_init.h","w")

	lines = f.readlines()

	for ln in lines:
		if len(ln) > 0 and ln[0] == '#':
			continue
		vals = ln.split(None,6)
		(name,typ,mn,mx,paramtype,init,comment) = vals
		
		types={"bool":"BOOL","int":"INT","double":"DBL","string":"STR"}
		rtypes={"bool":"bool","int":"int","double":"double","string":"std::string"}
		captype = types[typ]
		realtype = rtypes[typ]

		is_region_param = (paramtype == "region" or paramtype == "sprite")
		is_channel_param = (paramtype == "channel" or paramtype == "sprite")

		if is_region_param:
			out_ro_declare.write("bool %s;\n"%name)
			out_ro_get.write("GET_BOOL(%s);\n"%name)
			out_ro_init.write("OVERRIDE_INIT(%s);\n"%name)
			out_ro_list.write("\"%s\",\n"%name)
			out_ro_set.write("SET_OVERRIDE(%s);\n"%name)

			out_rp_declare.write("%s %s;\n"%(realtype,name))
			out_rp_get.write("GET_%s_PARAM(%s);\n"%(captype,name))

			out_rp_init.write("%s = %s;\n"%(name,init))
			out_rp_list.write("\"%s\",\n"%name)
			out_rp_set.write("SET_%s_PARAM(%s);\n"%(captype,name))

			out_rg_reset.write("RESET_PARAM(%s);\n"%name)

			if typ == "bool":
				out_rp_increment.write("INC_%s_PARAM(%s);\n"%(captype,name))
				out_rp_toggle.write("TOGGLE_PARAM(%s);\n"%name)
			elif typ == "int" or typ == "double":
				out_rp_increment.write("INC_%s_PARAM(%s,%s,%s);\n"%(captype,name,mn,mx))
			elif typ == "string":
				if mn == "None":
					out_rp_increment.write("INC_NO_PARAM(%s);\n"%(name))
				else:
					# The mn value is the Types array
					out_rp_increment.write("INC_%s_PARAM(%s,%s);\n"%(captype,name,mn))
			else:
				print("Unrecognized paramtype: %s" % typ)

		if is_channel_param:
			out_co_declare.write("bool %s;\n"%name)
			out_co_get.write("GET_BOOL(%s);\n"%name)
			out_co_init.write("OVERRIDE_INIT(%s);\n"%name)
			out_co_list.write("\"%s\",\n"%name)
			out_co_set.write("SET_OVERRIDE(%s);\n"%name)

			out_cp_declare.write("%s %s;\n"%(realtype,name))
			out_cp_get.write("GET_%s_PARAM(%s);\n"%(captype,name))

			out_cp_init.write("%s = %s;\n"%(name,init))
			out_cp_list.write("\"%s\",\n"%name)
			out_cp_set.write("SET_%s_PARAM(%s);\n"%(captype,name))

			out_ch_reset.write("RESET_PARAM(%s);\n"%name)

			if typ == "bool":
				out_cp_increment.write("INC_%s_PARAM(%s);\n"%(captype,name))
				out_cp_toggle.write("TOGGLE_PARAM(%s);\n"%name)

			elif typ == "int" or typ == "double":
				out_cp_increment.write("INC_%s_PARAM(%s,%s,%s);\n"%(captype,name,mn,mx))
			elif typ == "string":
				if mn == "None":
					out_cp_increment.write("INC_NO_PARAM(%s);\n"%(name))
				else:
					# The mn value is the Types array
					out_cp_increment.write("INC_%s_PARAM(%s,%s);\n"%(captype,name,mn))
			else:
				print("Unrecognized paramtype: %s" % typ)

		if paramtype == "sprite":
			out_sp_declare.write("%s %s;\n"%(realtype,name))
			out_sp_init.write("INIT_PARAM(%s);\n"%name)

			out_rp_issprite.write("IS_SPRITE_PARAM(%s);\n"%name)
			out_cp_issprite.write("IS_SPRITE_PARAM(%s);\n"%name)

	out_ro_declare.close()
	out_ro_get.close()
	out_ro_init.close()
	out_ro_list.close()
	out_ro_set.close()
	out_rp_declare.close()
	out_rp_get.close()
	out_rp_increment.close()
	out_rp_init.close()
	out_rp_issprite.close()
	out_rp_list.close()
	out_rp_set.close()
	out_rp_toggle.close()
	out_rg_reset.close()
	out_sp_declare.close()
	out_sp_init.close()

	out_co_declare.close()
	out_co_get.close()
	out_co_init.close()
	out_co_list.close()
	out_co_set.close()
	out_cp_declare.close()
	out_cp_get.close()
	out_cp_increment.close()
	out_cp_init.close()
	out_cp_issprite.close()
	out_cp_list.close()
	out_cp_set.close()
	out_cp_toggle.close()
	out_ch_reset.close()

if __name__ == "__main__":

	if len(sys.argv) < 2:
		print("Usage: %s [dir]" % sys.argv[0])
		sys.exit(1)
	dir = sys.argv[1]
	os.chdir(dir)
	fname = "Params.list"
	print("Processing %s in directory %s" % (fname,dir))
	generate(fname)