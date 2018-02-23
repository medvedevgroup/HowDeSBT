#!/usr/bin/env python
"""
Read and interpret the results of sabutan's built-in file-time tracking
"""

from sys import argv,stdin,stderr,exit

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
   ..."""

	if (s == None): exit (message)
	else:           exit ("%s%s" % (s,message))


def main():

	# parse the command line

	if (len(argv) != 1):
		usage("give me no arguments")

	# process file events

	isFirst = True
	for (filename,block) in file_blocks(stdin):
		if (isFirst): isFirst = False
		else:         print
		print "\n".join(block)


def file_blocks(f):
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


if __name__ == "__main__": main()
