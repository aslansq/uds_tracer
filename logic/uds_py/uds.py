from serv import *
import os
from jinja2 import Environment, FileSystemLoader

env = Environment(loader=FileSystemLoader('templates/cpp'))

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

template = env.get_template('uds_def.h.template')
rendered_output = template.render(
	servCont=servCont,
	reqRespClasses=reqRespClasses
)
with open('output/uds_def.h', 'w') as f:
	f.write(rendered_output)

template = env.get_template('uds_def.cpp.template')
rendered_output = template.render(
	servCont=servCont,
	reqRespClasses=reqRespClasses
)
with open('output/uds_def.cpp', 'w') as f:
	f.write(rendered_output)

print("Uds.py generated successfully.")