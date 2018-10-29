#!/usr/bin/env python
"""
Compare two runs of the query command in howdesbt (or in bloomtree-allsome),
and report any differences.
"""

from sys import argv,stdin,stderr,exit

def usage(s=None):
	message = """
usage: [cat query1log] | compare_query_results [query1log] query2log [options]
  query[1|2]log       (required) output from the command "howdesbt query"; if
                      query1log isn't on the command line, it is read from
                      stdin
  --fasta=<filename>  map sequences in the log(s) to names; this is useful
                      for output from bloomtree-allsome

We compare two runs of the query command in howdesbt (or in bloomtree-allsome),
and report any differences.

Typical output from howdesbt:
  *query1 3
  # 57 nodes examined
  SRR393763.bf
  SRR393767.bf
  SRR962601.bf
  *query2 8
  # 135 nodes examined
  SRR1047874.bf
  SRR1047873.bf
  SRR921944.bf
  SRR921945.bf
  SRR921935.bf
  SRR921934.bf
  SRR791045.bf
  SRR791044.bf
  *query3 0
  # 15 nodes examined
   ...

Bloomtree-allsome produces similar output, except the query names are replaced
by the query sequences."""

	if (s == None): exit (message)
	else:           exit ("%s\n%s" % (s,message))


def main():
	global debug

	# parse the command line

	logFilename1  = None
	logFilename2  = None
	fastaFilename = None
	debug         = []

	for arg in argv[1:]:
		if ("=" in arg):
			argVal = arg.split("=",1)[1]

		if (arg.startswith("--fasta=")):
			fastaFilename = argVal
		elif (arg == "--debug"):
			debug += ["debug"]
		elif (arg.startswith("--debug=")):
			debug += argVal.split(",")
		elif (arg.startswith("--")):
			usage("unrecognized option: %s" % arg)
		elif (logFilename1 == None):
			logFilename1 = arg
		elif (logFilename2 == None):
			logFilename2 = arg
		else:
			usage("unrecognized option: %s (more than two log files?)" % arg)

	if (logFilename1 == None):
		usage("you have two give me two log files")

	if (logFilename2 == None):
		(logFilename1,logFilename2) = (None,logFilename1)

	# read the name mapping, if there is one

	sequenceToName = None

	if (fastaFilename != None):
		f = file(fastaFilename)
		sequenceToName = {}
		for (lineNum,name,seq) in read_fasta(f,fastaFilename):
			if (seq in sequenceToName):
				(name1,lineNum1) = sequenceToName[seq]
				assert (False), \
				       "%s contains duplicated sequence: \"%s\" (line %d) and \"%s\" (line %d)" \
				     % (fastaFilename,name1,lineNum1,name,lineNum)
			sequenceToName[seq] = (name,lineNum)
		f.close()

		for seq in sequenceToName:
			(name,_) = sequenceToName[seq]
			sequenceToName[seq] = name

	# read the two log files

	if (logFilename1 == None):
		log1 = read_query_log(stdin,sequenceToName=sequenceToName)
	else:
		f = file(logFilename1)
		log1 = read_query_log(f,logFilename1,sequenceToName=sequenceToName)
		f.close()

	f = file(logFilename2)
	log2 = read_query_log(f,logFilename2,sequenceToName=sequenceToName)
	f.close()

	if ("input" in debug):
		dump_query_log(stderr,log1,logFilename1)
		dump_query_log(stderr,log2,logFilename2)

	# compare logs

	allNames =  [name for name in log1]
	allNames += [name for name in log2 if (name not in log1)]
	allNames.sort()

	print "#%s\t%s\t%s\t%s" % ("query","inboth","in1only","in2only")

	for name in allNames:
		q1 = q2 = None
		if (name in log1): q1 = log1[name]
		if (name in log2): q2 = log2[name]

		if (q1 == None):
			print "%s\t%d\t%d\t%d" % (name,0,0,len(q2.hits))
		elif (q2 == None):
			print "%s\t%d\t%d\t%d" % (name,0,len(q1.hits),0)
		else:
			inBoth = [x for x in q1.hits if (x in q2.hits)]
			print "%s\t%d\t%d\t%d" \
			    % (name,len(inBoth),len(q1.hits)-len(inBoth),len(q2.hits)-len(inBoth))


# read_fasta--

def read_fasta(f,filename):
	seqLineNum = seqName = seqNucs = None

	lineNum = 0
	for line in f:
		lineNum += 1
		line = line.strip()
		if (line.startswith(">")):
			if (seqName != None):
				yield (seqLineNum,seqName,"".join(seqNucs))
			seqLineNum = lineNum
			seqName    = line[1:].strip()
			seqNucs    = []
		elif (seqName == None):
			assert (False), "first sequence in %s has no header" % filename
		else:
			seqNucs += [line]

	if (seqName != None):
		yield (seqLineNum,seqName,"".join(seqNucs))


# read_query_log--

class QueryResults: pass

def read_query_log(f,filename=None,sequenceToName=None):
	if (filename == None): filename = "stdin"

	if (sequenceToName != None):
		sequenceNames = [sequenceToName[seq] for seq in sequenceToName]

	nameToResults = {}
	q = None

	lineNum = 0
	for line in f:
		lineNum += 1
		line = line.strip()
		if (line == ""):
			continue

		if (line[0] == "*"):
			if (q != None):
				assert (len(q.hits) == q.hitCount), \
				       "hit list disagrees with hit count for \"%s\" (line %d in %s)" \
				     % (q.name,filename,q.lineNum)  
				nameToResults[q.name] = q

			q = QueryResults()
			q.lineNum = lineNum
			(name,hitCount) = line[1:].split(None,1)

			if (sequenceToName == None):
				q.name = name
			elif (name in sequenceToName):
				q.name = sequenceToName[name]
			elif (name in sequenceNames):
				q.name = name
			else:
				assert (False), \
				       "unknown query sequence in %s (line %d):\n%s" \
				     % (filename,lineNum,name)

			try:
				q.hitCount = int(hitCount)
				if (q.hitCount < 0): raise ValueError
			except ValueError:
				assert (False), \
				       "bad hit count in %s (line %d): %s" \
				     % (filename,lineNum,hitCount)

			q.hits = []
			q.nodesExamined = None
			continue

		if (line[0] != "#"):
			if ("/" in line):
				line = line.split("/")[-1]
			if (".bf" in line):
				ix= line.find(".bf")
				line = line[:ix]
			elif (".bv" in line):
				ix= line.find(".bv")
				line = line[:ix]
			q.hits += [line]
			continue

		if (q == None):  # ignore comments before first query
			continue

		nodesExamined = None
		if (line.endswith("nodes examined")):
			nodesExamined = line[1:-len("nodes examined")]
		elif (line.endswith("nodes visited")):
			nodesExamined = line[1:-len("nodes visited")]

		if (nodesExamined != None):
			try:
				q.nodesExamined = int(nodesExamined)
				if (q.nodesExamined < 0): raise ValueError
			except ValueError:
				assert (False), \
				       "bad nodes examined in %s (line %d):\n%s" \
				     % (filename,lineNum,line)
			continue

		assert (False), \
		       "unrecognized line in %s (line %d):\n%s" \
		     % (filename,lineNum,line)

	if (q != None):
		assert (len(q.hits) == q.hitCount), \
		       "hit list disagrees with hit count for \"%s\" (line %d in %s)" \
		     % (q.name,filename,q.lineNum)  
		nameToResults[q.name] = q

	return nameToResults


# dump_query_log--

def dump_query_log(f,log,filename=None):
	if (filename == None): filename = "stdin"

	print >>f, "log for %s" % filename
	for name in log:
		q = log[name]
		print "%s %s %s" \
		    % (q.name,q.nodesExamined,",".join(q.hits))

	print >>f


if __name__ == "__main__": main()
