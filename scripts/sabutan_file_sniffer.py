#!/usr/bin/env python
"""
Read and interpret the results of sabutan's built-in file-time tracking
"""

from sys import argv,stdin,stderr,exit
from re  import compile as re_compile

def usage(s=None):
	message = """

usage: cat sabutan_file_log | sabutan_file_sniffer [options]
  (no options yet)"

Typical input is shown below. Usually this is created by a command like
  sabutan query
with
  --debug=reportopenclose,reportfilebytes,reportloadtime
Lines that begin with [ are relevant events. Any other lines are ignored. 

  [FileManager open_file] node1.detbrief.rrr.bf
  [BloomFilter open] 0.000046 secs node1.detbrief.rrr.bf
  [BloomFilter identify_content] read 16 bytes node1.detbrief.rrr.bf
  [BloomFilter identify_content] read 128 bytes node1.detbrief.rrr.bf
  [BloomFilter load-header] 0.000009 secs node1.detbrief.rrr.bf
  [RrrBitVector load-open] 0.000009 secs node1.detbrief.rrr.bf@144
  [RrrBitVector serialized_in] read 108855387 bytes node1.detbrief.rrr.bf@144
  [RrrBitVector load] 0.265745 secs node1.detbrief.rrr.bf@144
  [RrrBitVector load-open] 0.000001 secs node1.detbrief.rrr.bf@108855531
  [RrrBitVector serialized_in] read 7814859 bytes node1.detbrief.rrr.bf@108855531
  [RrrBitVector load] 0.017566 secs node1.detbrief.rrr.bf@108855531
  [FileManager close_file] node1.detbrief.rrr.bf
  [FileManager open_file] node2.detbrief.rrr.bf
  [BloomFilter open] 0.000054 secs node2.detbrief.rrr.bf
  [BloomFilter identify_content] read 16 bytes node2.detbrief.rrr.bf
   ...

CAVEAT: Currently this only interprets file read timing from the query command."""

	if (s == None): exit (message)
	else:           exit ("%s%s" % (s,message))


def main():
	global debug

	# parse the command line

	debug = {}

	for arg in argv[1:]:
		if ("=" in arg):
			argVal = arg.split("=",1)[1]

		if (arg == "--debug"):
			debug["debug"] = True
		elif (arg.startswith("--debug=")):
			for name in argVal.split(","):
				debug[name] = True
		else:
			usage("unrecognized option: %s" % arg)

	# process file events

	isFirst = True
	for (filename,block) in read_blocks(stdin):
		if ("blocks" in debug):
			if (isFirst): isFirst = False
			else:         print
			print "\n".join(block)

		bytesRead = 0
		seconds   = 0.0
		for line in block:
			(sabuClass,action,info) = parse_event(line)
			if ("bytesRead" in info): bytesRead += info["bytesRead"]
			if ("seconds"   in info): seconds   += info["seconds"]

		if (isFirst):
			print "#%s\t%s\t%s" % ("node","bytesRead","seconds")
			isFirst = False
		print "%s\t%d\t%.6f" % (filename,bytesRead,seconds)


def read_blocks(f):
	currentFilename = None
	block = None

	lineNumber = 0
	for line in f:
		lineNumber += 1
		line = line.strip()
		if (not line.startswith("[")): continue

		assert ("]" in line), "bad line at line %d:\n%s" % (lineNumber,line)

		filename = line.split()[-1]
		if ("@" in filename):
			filename = filename.split("@",1)[0]

		if (filename != currentFilename):
			if (currentFilename != None):
				yield (currentFilename,block)
			currentFilename = filename
			block = []

		block += [line]

	if (currentFilename != None):
		yield (currentFilename,block)


def parse_event(line):
	(action,infoText) = line[1:].split("]",1)
	(action,infoText) = (action.strip(),infoText.strip())

	(sabuClass,action) = action.split(None,1)
	(sabuClass,action) = (sabuClass.strip(),action.strip())

	info = {"text": infoText}

	parsed = False
	if (not parsed):
		try:
			info["bytesRead"] = parse_bytes_read(infoText)
			parsed = True
		except ValueError:
			pass
	if (not parsed):
		try:
			info["seconds"] = parse_time(infoText)
			parsed = True
		except ValueError:
			pass

	return (sabuClass,action,info)

bytesReadRe = re_compile("^read (?P<bytes>[0-9]+) bytes")
def parse_bytes_read(info):
	m = bytesReadRe.match(info)
	if (m == None): raise ValueError
	return int(m.group("bytes"))

timeRe = re_compile("^(?P<seconds>[0-9.]+) secs")
def parse_time(info):
	m = timeRe.match(info)
	if (m == None): raise ValueError
	return float(m.group("seconds"))


if __name__ == "__main__": main()
