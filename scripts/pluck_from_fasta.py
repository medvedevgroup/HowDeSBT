#!/usr/bin/env python
"""
Pluck a known subset of the sequences in a FASTA file.
"""

from sys import argv,stdin,stdout,stderr,exit


def usage(s=None):
	message = """
usage: cat fasta_file | pluck_from_fasta [<sequence_names_file>] [options]
  <sequence_names_file>  file listing the names of sequences of interest, one-
                         per-line
  --sequence[s]=<names>  comma-separated list of sequence names"""

	if (s == None): exit (message)
	else:           exit ("%s\n%s" % (s,message))


def main():

	# parse the command line

	seqNamesFilename = None
	seqNamesGiven    = []

	for arg in argv[1:]:
		if ("=" in arg):
			argVal = arg.split("=",1)[1]

		if (arg.startswith("--sequence=")) or (arg.startswith("--sequences=")):
			newNames = []
			for name in argVal.split(","):
				assert (name != ""), "empty name not allowed"
				if (name not in seqNamesGiven) and (name not in newNames):
					newNames += [name]
			seqNamesGiven += newNames
		elif (seqNamesFilename == None):
			seqNamesFilename = arg
		else:
			usage("unrecognized option: %s" % arg)

	if (seqNamesFilename == None) and (seqNamesGiven == []):
		usage("you have to tell me what sequences to pluck")

	# read the sequence names

	seqNames = set(seqNamesGiven)

	if (seqNamesFilename != None):
		lineNumber = 0
		for line in file(seqNamesFilename):
			lineNumber += 1
			line = line.strip()
			if (line == ""): continue
			seqNames.add(line)

	# process the fasta file, printing each desired sequence or collecting it
	# for later sort

	copying = False

	lineNumber = 0
	for line in stdin:
		lineNumber += 1
		line = line.strip()

		if (line.startswith(">")):
			name = line[1:].split()[0]
			copying = (name in seqNames)
			if (copying): print line
		elif (copying):
			print line


if __name__ == "__main__": main()
