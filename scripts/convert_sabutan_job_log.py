#!/usr/bin/env python
"""
Read the log output from a sabutan test job, and extract the useful information.
"""

from sys import argv,stdin,stdout,stderr,exit
from re  import compile


def usage(s=None):
	message = """
usage: cat job_log_file | convert_sabutan_job_log [options]
  --sabutan                    the log file is from a sabutan test job
                               (this is the default)
  --allsome                    the log file is from a bloomtree-allsome test job
  --split-sbt                  the log file is from a split-sbt test job
  --showas=HMS                 report times as hours, minutes, seconds
  --showas=HM                  report times as hours, minutes
  --showas=hours[.<digits>]    report times in units of hours
  --showas=minutes[.<digits>]  report times in units of minutes
  --showas=seconds[.<digits>]  report times in units of seconds"""

	if (s == None): exit (message)
	else:           exit ("%s\n%s" % (s,message))


def main():

	# parse the command line

	logType = "sabutan"
	showAs  = "HMS"

	for arg in argv[1:]:
		if ("=" in arg):
			argVal = arg.split("=",1)[1]

		if (arg == "--sabutan"):
			logType = "sabutan"
		elif (arg in ["--allsome","--bloomtree-allsome","--bt-allsome"]):
			logType = "allsome"
		elif (arg in ["--split-sbt","--splitsbt"]):
			logType = "split-sbt"
		elif (arg == "--showas=HMS"):
			showAs = "HMS"
		elif (arg == "--showas=HM"):
			showAs = "HM"
		elif (arg == "--showas=MS"):
			showAs = "MS"
		elif (arg == "--showas=hours"):
			showAs = "hours"
		elif (arg == "--showas=minutes"):
			showAs = "minutes"
		elif (arg == "--showas=seconds"):
			showAs = "seconds"
		elif (arg.startswith("--showas=hours.")):	#  .. decimal point)
			showAs = ("hours",arg.split(".",1)[1])
		elif (arg.startswith("--showas=minutes.")):
			showAs = ("minutes",arg.split(".",1)[1])
		elif (arg.startswith("--showas=seconds.")):
			showAs = ("seconds",arg.split(".",1)[1])
		else:
			usage("unrecognized option: %s" % arg)

	# convert the log file

	if   (logType == "sabutan"):   converter = SabutanLogReader(showAs=showAs)
	elif (logType == "allsome"):   converter = AllSomeLogReader(showAs=showAs)
	elif (logType == "split-sbt"): converter = SplitSbtLogReader(showAs=showAs)

	converter.read_file(stdin)

	print converter

# SabutanLogReader--
#
# typical log file:
#   topologyId = detbrief.winnowed.rrr
#   batch = ten.3
#   specificity = 0.9
#   
#   real	0m57.174s
#   user	0m11.520s
#   sys	0m33.848s
#   
#   real	0m41.874s
#   user	0m9.200s
#   sys	0m32.672s

class SabutanLogReader(object):

	def __init__(self,showAs="HMS"):
		self.showAs = showAs

	def read_file(self,f):
		self.topologyId  = None
		self.batch       = None
		self.specificity = None
		self.runs        = []
		currentRun       = None

		for line in f:
			line = line.strip()

			if (line.startswith("topologyId")):
				line = line.split(None,2)
				assert (line[1] == "=")
				if (self.topologyId != None): assert (line[2] == self.topologyId)
				else:                         self.topologyId = line[2]
				continue

			if (line.startswith("batch")):
				line = line.split(None,2)
				assert (line[1] == "=")
				if (self.batch != None): assert (line[2] == self.batch)
				else:                    self.batch = line[2]
				continue

			if (line.startswith("specificity")):
				line = line.split(None,2)
				assert (line[1] == "=")
				if (self.specificity != None): assert (line[2] == self.specificity)
				else:                          self.specificity = line[2]
				continue

			if (line.startswith("real")):
				line = line.split(None,1)
				currentRun = {"real" : time_to_seconds(line[1])}
				self.runs += [currentRun]
				continue

			if (line.startswith("user")):
				assert (currentRun != None) and ("user" not in currentRun)
				line = line.split(None,1)
				currentRun["user"] = time_to_seconds(line[1])
				continue

			if (line.startswith("sys")):
				assert (currentRun != None) and ("sys" not in currentRun)
				line = line.split(None,1)
				currentRun["sys"] = time_to_seconds(line[1])
				continue

	def __str__(self):
		s = []
		s += [self.topologyId  if (self.topologyId  != None) else "NA"]
		s += [self.batch       if (self.batch       != None) else "NA"]
		s += [self.specificity if (self.specificity != None) else "NA"]

		for run in self.runs:
			s += [seconds_to_time(run["real"],showAs=self.showAs) if ("real" in run) else "NA"]
			s += [seconds_to_time(run["user"],showAs=self.showAs) if ("user" in run) else "NA"]
			s += [seconds_to_time(run["sys"], showAs=self.showAs) if ("sys"  in run) else "NA"]

		return "\t".join(s)


# AllSomeLogReader--

class AllSomeLogReader(object):

	def __init__(self):
		pass

	def read_file(self,f):
		raise ValueError


# SplitSbtLogReader--

class SplitSbtLogReader(object):

	def __init__(self):
		pass

	def read_file(self,f):
		raise ValueError


# time_to_seconds--
#  Convert a time like "5m32.672s" to 332.672

timeFmtRe1 = compile("^(?P<days>[0-9]+)d(?P<hours>[0-9]+)h(?P<minutes>[0-9]+)m$")
timeFmtRe2 = compile("^(?P<hours>[0-9]+)h(?P<minutes>[0-9]+)m(?P<seconds>[0-9.]+)s$")
timeFmtRe3 = compile("^(?P<minutes>[0-9]+)m(?P<seconds>[0-9.]+)s$")
timeFmtRe4 = compile("^(?P<hours>[0-9]+)h(?P<minutes>[0-9]+)m$")
timeFmtRe5 = compile("^(?P<minutes>[0-9]+)m$")

def time_to_seconds(s):
	s = s.replace("\t"," ").strip()

	m = timeFmtRe1.match(s)
	if (m != None):
		days = int(m.group("days"))
		hrs  = int(m.group("hours"))
		mins = int(m.group("minutes"))
		return 86400*days + 3600*hrs + 60*mins

	m = timeFmtRe2.match(s)
	if (m != None):
		hrs  = int(m.group("hours"))
		mins = int(m.group("minutes"))
		secs = float(m.group("seconds"))
		return 3600*hrs + 60*mins + secs

	m = timeFmtRe3.match(s)
	if (m != None):
		mins = int(m.group("minutes"))
		secs = float(m.group("seconds"))
		return 60*mins + secs

	m = timeFmtRe4.match(s)
	if (m != None):
		hrs  = int(m.group("hours"))
		mins = int(m.group("minutes"))
		return 3600*hrs + 60*mins

	m = timeFmtRe5.match(s)
	if (m != None):
		mins = int(m.group("minutes"))
		return 60*mins

	raise ValueError


# seconds_to_time--

def seconds_to_time(seconds,showAs="HMS"):

	showFmt = None
	if (type(showAs) == tuple):
		(showAs,showFmt) = showAs
		if (showFmt == ""): showFmt = "2"

	showSeconds = (showAs in ["HMS","MS","seconds"]) or (showFmt != None)

	if (not showSeconds):
		seconds = int((seconds + 30) / 60) * 60

	if (showAs == "days"):
		if (showFmt == None):
			days = (seconds + 12*60*60) / (24*60*60)
			time = "%d" % days
		else:
			showFmt = "%%.%sf" % showFmt
			days = seconds / (24*60*60.0)
			time = showFmt % days
		return time

	if (showAs == "hours"):
		if (showFmt == None):
			hours = (seconds + 30*60) / (60*60)
			time = "%d" % hours
		else:
			showFmt = "%%.%sf" % showFmt
			hours = seconds / (60*60.0)
			time = showFmt % hours
		return time

	if (showAs == "minutes"):
		if (showFmt == None):
			minutes = (seconds + 30) / 60
			time = "%d" % minutes
		else:
			showFmt = "%%.%sf" % showFmt
			minutes = seconds / 60.0
			time = showFmt % minutes
		return time

	if (showAs == "seconds"):
		if (showFmt == None):
			time = "%d" % round(seconds)
		else:
			showFmt = "%%.%sf" % showFmt
			time = showFmt % seconds
		return time

	time = []
	if (showAs == "HMS") or (showAs == "HM"):
		if (seconds >= 3600):
			hours = int(seconds/3600)
			time += ["%dh" % hours]
			seconds -= 3600 * hours
	elif (showAs == "MS"):
		pass
	else: # if (showAs == "DHMS"):
		if (seconds >= 86400):
			days = int(seconds/86400)
			time += ["%dd" % days]
			seconds -= 86400 * days
		if ((seconds >= 3600) or (time != [])):
			hours = int(seconds/3600)
			time += ["%02dh" % hours]
			seconds -= 3600 * hours

	if (seconds >= 60) or (time != []) or (not showSeconds):
		minutes = int(seconds/60)
		time += ["%02dm" % minutes]
		seconds -= 60 * minutes
	if (showSeconds):
		time += ["%06.3fs" % seconds]
	time = "".join(time)
	if (time.startswith("0")): time = time[1:]
	return time


if __name__ == "__main__": main()
