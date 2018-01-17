#!/usr/bin/env python
"""
Read and interpret the results of sabutan's built-in memory allocation tracking
"""

from sys import argv,stdin,stderr,exit

def usage(s=None):
	message = """

usage: cat sabutan_memory_log | sabutan_memory_sniffer [options]
  --stop=<text>    (cumulative) if a line contains the specified text, we
                   stop and report whatever objects are active.  Note that
                   <text> will usually have to be surrounded by "quotes"

Typical input is shown below.  Lines that begin with @+ are allocators, those
with @- are deallocators.  Other lines are ignored.  The @+ or @- prefix is
followed by the address of the object.

	@+0x7f9b18e00910 constructor BloomFilter(BloomFilter:"leaf6.bf")
	@+0x7f9b18e009a0 malloc bf file header for "leaf6.bf"
	about to construct BloomFilter for leaf6.bf content 0
	@+0x7f9b18e00a10 constructor BloomFilter(BloomFilter:"leaf6.bf")
	about to construct BitVector for leaf6.bf content 0
	creating bit_vector type 1 at offset 112 in "leaf6.bf"
	@+0x7f9b18e00aa0 constructor BitVector(BitVector:"leaf6.bf":112)
	@-0x7f9b18e009a0 free bf file header for "leaf6.bf"
	@-0x7f9b18e00a10 destructor BloomFilter(BloomFilter:"leaf6.bf")
	@+0x7f9b18e00850 creating bits for BitVector(BitVector:"leaf6.bf":112 ...
	@+0x7f9b18e00b40 constructor BloomFilter(BloomFilter:"leaf6.detbrief.bf")
	@-0x7f9b18e00910 destructor BloomFilter(BloomFilter:"leaf6.bf")
	@+0x7f9b18e00870 creating bits for BitVector(BitVector:"" 0x7f9b18e00a20)
	@+0x7f9b18e00a20 constructor BitVector(BitVector:"")
	@+0x7f9b18e00910 malloc bf file header for BloomFilter(...
	Saving leaf6.detbrief.bf
	@-0x7f9b18e00910 free bf file header for BloomFilter(...
	 ..."""

	if (s == None): exit (message)
	else:           exit ("%s%s" % (s,message))


def main():
	global addrToHistory

	# parse the command line

	stopText = None

	for arg in argv[1:]:
		if ("=" in arg):
			argVal = arg.split("=",1)[1]

		if (arg.startswith("--stop=")):
			if (stopText == None): stopText =  [argVal]
			else:                  stopText += [argVal]
		elif (arg.startswith("--")):
			usage("unrecognized option: %s" % arg)
		else:
			usage("unrecognized argument: %s" % arg)

	# process allocation records

	addrToHistory = {}
	addrToActive  = {}

	lineNumber = 0
	for line in stdin:
		lineNumber += 1
		line = line.strip()

		if (stopText != None):
			stop = False
			for text in stopText:
				if (text not in line): continue
				stop = True
				break
			if (stop):
				print >>stderr, "stopping at line %d, for \"%s\"" % (lineNumber,text)
				break

		if (not (line.startswith("@"))): continue

		errorReport = "I don't understand (line %d):\n%s" % (lineNumber,line)

		if (line.startswith("@+")):
			# allocation

			(addr,text) = line.split(None,1)
			addr = addr[2:]

			if (addr in addrToActive):
				if (not repeated_contructor(lineNumber,addr,text)):
					(_,_,prevText) = addrToHistory[addr][-1]
					print ("problem: %s is already active"
					     + "\n  [line %d] %s"
					     + "\n  [line %d] %s") \
					     % (addr,addrToActive[addr],prevText,lineNumber,text)

			if (addr not in addrToHistory): addrToHistory[addr] = []
			addrToHistory[addr] += [(lineNumber,"+",text)]
			addrToActive [addr] =  lineNumber

		elif (line.startswith("@-")):
			# deallocation

			(addr,text) = line.split(None,1)
			addr = addr[2:]

			if (addr not in addrToActive):
				if (not repeated_destructor(lineNumber,addr,text)):
					print ("problem: %s isn't active"
					     + "\n  [line %d] %s") \
					     % (addr,lineNumber,text)

			if (addr not in addrToHistory): addrToHistory[addr] = []
			addrToHistory[addr] += [(lineNumber,"-",text)]
			if (addr in addrToActive): del addrToActive[addr]

		else:
			assert (False), errorReport

	# report any 'leaks'

	leaks = [(addrToActive[addr],addr) for addr in addrToActive]
	leaks.sort()

	for (lineNumber,addr) in leaks:
		(_,_,text) = addrToHistory[addr][-1]
		print "possible leak: %s [line %d] %s" \
		    % (addr,lineNumber,text)


def repeated_contructor(lineNumber,addr,text):
	# detect cases like this:
	#   @+0x7fefffc037c0 constructor BitVector ...
	#   @+0x7fefffc037c0 constructor RawBitVector ...

	if (not (text.startswith("constructor"))): return False
	(prevLineNumber,_,prevText) = addrToHistory[addr][-1]
	if (not (prevText.startswith("constructor"))): return False
	if (lineNumber != prevLineNumber+1): return False
	return True


def repeated_destructor(lineNumber,addr,text):
	# detect cases like this:
	#   @-0x7ffc31c04320 destructor DeterminedFilter ...
	#   @-0x7ffc31c04320 destructor BloomFilter ...

	if (not (text.startswith("destructor"))): return False
	(prevLineNumber,_,prevText) = addrToHistory[addr][-1]
	if (not (prevText.startswith("destructor"))): return False
	if (lineNumber != prevLineNumber+1): return False
	return True


if __name__ == "__main__": main()
