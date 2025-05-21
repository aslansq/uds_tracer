from serv import *
import os
from jinja2 import Environment, FileSystemLoader
import sys
import getopt

def collectArgs():
	retArgs = {}
	argLists = sys.argv[1:]
	opts = "ht:o:"
	longOpts = ["help", "template=", "output="]

	try:
		args, vals = getopt.getopt(argLists, opts, longOpts)
		for arg, val in args:
			arg = arg.replace(" ", "")
			if arg in ("-h", "--help"):
				s = \
				"Usage: uds.py -t <template path> -o <output path>" \
				"Arguments:" \
				"  -h, --help         show this help message and exit" \
				"  -t, --template     specify the template path" \
				"  -o, --output       specify the output path"
				print(s)
				sys.exit()
			elif arg in ("-t", "--template"):
				if not os.path.isfile(val):
					print('Error: The template ' + val + ' does not exist.')
					sys.exit(1)
				retArgs["template"] = val
			elif arg in ("-o", "--output"):
				if not os.path.isdir(val):
					print('Error: The output directory ' + val + ' does not exist.')
					sys.exit(1)
				retArgs["output"] = val

	except getopt.GetoptError as e:
		print(e)
		sys.exit(1)

	if not "template" in retArgs:
		print("Error: --template is required.")
	if not "output" in retArgs:
		print("Error: --output is required.")
	return retArgs

args = collectArgs()
templateDirPath = os.path.dirname(args["template"])
templateFileName = os.path.basename(args["template"])
outputDirPath = args["output"]
outputFileName = templateFileName.removesuffix(".template")
outputFilePath = os.path.join(outputDirPath, outputFileName)

servXml = ServXmlParser(
	os.path.join("xml", "serv.xml"),
	os.path.join("xsd", "service.xsd"),
	os.path.join("xml", "serv_sub.xml"),
	os.path.join("xsd", "service_sub.xsd")
)

reqRespXml = ReqRespXmlParser(
	os.path.join("xml", "req_resp.xml"),
	os.path.join("xsd", "req_resp.xsd")
)

reqRespClasses = reqRespXml.getReqRespClasses()
servCont = servXml.getServCont()

def screaming_camel_case(value):
	s = ""
	for i in range(len(value)):
		c = value[i]
		if not c.islower() and i != 0:
			s = s + "_"
		s = s + c.upper()
	return s

def camel_case(value):
	s = ""
	for i in range(len(value)):
		c = value[i]
		if not c.islower() and i != 0:
			s = s + "_"
		s = s + c.lower()
	return s

env = Environment(loader=FileSystemLoader(templateDirPath))
env.filters['screaming_camel'] = screaming_camel_case
env.filters['camel'] = camel_case

template = env.get_template(templateFileName)
rendered_output = template.render(
	servCont=servCont,
	reqRespClasses=reqRespClasses
)
with open(outputFilePath, 'w') as f:
	f.write(rendered_output)

print("Uds.py generated successfully.")