import xmlschema
import os

class ServSubItem:
	def __init__(self):
		self.nameForProgrammer = ""
		self.hexStartStr = ""
		self.hexEndStr = ""
		self.hexStart = 0
		self.hexEnd = 0
		self.isHexRange = False
		self.name = ""
		self.mnemonic = ""
		self.detail = ""

	def getCapitalizedName(self):
		return self.nameForProgrammer[0].upper() + self.nameForProgrammer[1:]

class ServSubCont:
	def __init__(self):
		self._servSubItems = [] # ServSubItem type
		self._nameForProgCounters = {"reserved" : 0}

	def add(self, servSubItem):
		pName = servSubItem.nameForProgrammer
		
		if pName in self._nameForProgCounters:
			self._nameForProgCounters[pName] = self._nameForProgCounters[pName] + 1
		else:
			self._nameForProgCounters[pName] = 0

		if self._nameForProgCounters[pName] != 0:
			servSubItem.nameForProgrammer = servSubItem.nameForProgrammer + str(self._nameForProgCounters[pName])

		self._servSubItems.append(servSubItem)

	def getNumOfItems(self):
		return len(self._servSubItems)
	
	def getItems(self):
		return self._servSubItems

class ServSubXmlParser:
	def __init__(self, servProgName, servSubXmlFilePath, servSubXsdFilePath):
		self.servProgName = servProgName
		self.servSubXmlFilePath = os.path.abspath(servSubXmlFilePath)
		self.servSubXsdFilePath = os.path.abspath(servSubXsdFilePath)
		self.servSubCont = ServSubCont()

		if not os.path.exists(self.servSubXmlFilePath):
			raise Exception("xml file not exist. " + str(self.servSubXmlFilePath))
			return

		if not os.path.exists(self.servSubXsdFilePath):
			raise Exception("xsd file not exist. " + str(self.servSubXsdFilePath))
			return

		self.__parse()

	def getCont(self):
		return self.servSubCont

	def __parse(self):
		xsd = xmlschema.XMLSchema(self.servSubXsdFilePath)
		if not xsd.is_valid(self.servSubXmlFilePath):
			errors = xsd.validate(self.servSubXmlFilePath, use_defaults=False)
			raise Exception(
				"xml file not according to xsd. " +
				str(self.servSubXmlFilePath) +
				str(self.servSubXsdFilePath) +
				" Errors: " + str(errors)
			)
			return
		
		xml_data = xsd.to_dict(self.servSubXmlFilePath)
		
		for servSubXml in xml_data.get('ServiceSub', []):
			if servSubXml["ServiceNameForProgrammer"] != self.servProgName:
				continue
			nrc = self.__parseItem(servSubXml)
			self.servSubCont.add(nrc)

	def __parseItem(self, servSubXml):
		servSubItem = ServSubItem()
		servSubItem.nameForProgrammer = servSubXml["SubFunctionNameForProgrammer"]
		
		if "to" in servSubXml["Bit6to0"]:
			servSubItem.hexStartStr = servSubXml["Bit6to0"].split("to")[0].replace(" ", "")
			servSubItem.hexEndStr = servSubXml["Bit6to0"].split("to")[1].replace(" ", "")

			try:
				servSubItem.hexStart = int(servSubItem.hexStartStr, 16)
				servSubItem.hexEnd = int(servSubItem.hexEndStr, 16)
			except Exception as e:
				print(e)
				raise ValueError(servSubItem.nameForProgrammer + " hex value is invalid")
		else:
			servSubItem.hexStartStr  = servSubXml["Bit6to0"].replace(" ", "")

			servSubItem.hexEndStr = servSubItem.hexStartStr
			try:
				servSubItem.hexStart = int(servSubItem.hexStartStr, 16)
				servSubItem.hexEnd = servSubItem.hexStart
			except Exception as e:
				print(e)
				raise ValueError(servSubItem.nameForProgrammer + " hex value is invalid")

		if servSubItem.hexStart != servSubItem.hexEnd:
			servSubItem.isHexRange = True

		servSubItem.name = servSubXml["ShortName"]
		servSubItem.mnemonic = servSubXml["Mnemonic"]
		servSubItem.detail = servSubXml["Details"]
		servSubItem.detail = servSubItem.detail.replace('\n', '\\n')
		servSubItem.detail = servSubItem.detail.replace('"', '\\"')

		return servSubItem


class ServItem:
	def __init__(self):
		self.nameForProgrammer = ""
		self.hexStr = ""
		self.hex = 0
		self.shortName = ""
		self.defaultSess = ""
		self.nonDefaultSess = ""
		self.servSubCont = None # ServSubCont type

	def getCapitalizedName(self):
		return self.nameForProgrammer[0].upper() + self.nameForProgrammer[1:]
	
	def isContEmpty(self):
		if self.servSubCont is None:
			return True

		if self.servSubCont.getNumOfItems() == 0:
			return True

		return False

class ServCont:
	def __init__(self):
		self._servItems = [] # ServItem type
		self._nameForProgCounters = {"reserved" : 0}

	def add(self, servItem):
		pName = servItem.nameForProgrammer

		if pName in self._nameForProgCounters:
			self._nameForProgCounters[pName] = self._nameForProgCounters[pName] + 1
		else:
			self._nameForProgCounters[pName] = 0
		
		if self._nameForProgCounters[pName] != 0:
			servItem.nameForProgrammer = servItem.nameForProgrammer + str(self._nameForProgCounters[pName])

		self._servItems.append(servItem)

	def getNumOfItems(self):
		return len(self._servItems)
	
	def getItems(self):
		return self._servItems
	
class ReqRespItem:
	def __init__(self):
		self.nameForProgrammer = ""
		self.reqRespType = ""
		self.idx = ""
		self.dataSize = ""
		self.detail = ""
		self.isSizeKnown = False
		self.isStdDataSize = False
		self.dataType = ""
	def getCapitalizedName(self):
		return self.nameForProgrammer[0].upper() + self.nameForProgrammer[1:]
	
	def isByte(self):
		if self.dataType == "uint8_t":
			return True
		return False

class ReqRespClass:
	def __init__(self):
		self.memClassName = ""
		self.reqRespItems = [] # ReqRespItem type

	def getName(self):
		return self.memClassName + self.reqRespItems[0].reqRespType

class ReqRespXmlParser:
	def __init__(self, reqRespXmlFilePath, reqRespXsdFilePath):
		self.reqRespXmlFilePath = os.path.abspath(reqRespXmlFilePath)
		self.reqRespXsdFilePath = os.path.abspath(reqRespXsdFilePath)
		self.reqRespClasses = []
		self.classNames = []

		if not os.path.exists(self.reqRespXmlFilePath):
			raise Exception("xml file not exist. " + str(self.reqRespXmlFilePath))
			return

		if not os.path.exists(self.reqRespXsdFilePath):
			raise Exception("xsd file not exist. " + str(self.reqRespXsdFilePath))
			return

		self.__parse()
	
	def getReqRespClasses(self):
		return self.reqRespClasses
	
	def __parse(self):
		xsd = xmlschema.XMLSchema(self.reqRespXsdFilePath)
		if not xsd.is_valid(self.reqRespXmlFilePath):
			errors = xsd.validate(self.reqRespXmlFilePath, use_defaults=False)
			raise Exception(
				"xml file not according to xsd. " +
				str(self.reqRespXmlFilePath) +
				str(self.reqRespXsdFilePath) +
				" Errors: " + str(errors)
			)
			return
		
		xml_data = xsd.to_dict(self.reqRespXmlFilePath)

		self.__parseItems(xml_data)


	def __parseItems(self, xmlData):
		for reqResItem in xmlData.get('ReqRespItem', []):
			className = reqResItem["MemClassName"]
			if className not in self.classNames:
				reqRespClass = ReqRespClass()
				reqRespClass.memClassName = className
				self.classNames.append(className)
				self.reqRespClasses.append(reqRespClass)

		for reqResItem in xmlData['ReqRespItem']:
			reqResItemObj = ReqRespItem()
			reqResItemObj.nameForProgrammer = reqResItem["NameForProgrammer"]
			reqResItemObj.reqRespType = reqResItem["ReqResp"]
			reqResItemObj.idx = int(reqResItem["DataByte"])
			reqResItemObj.idx = str(reqResItemObj.idx - 1)
			reqResItemObj.dataSize = reqResItem["Size"]

			try:
				intDataSize = int(reqResItemObj.dataSize)
				reqResItemObj.isSizeKnown = True
				if intDataSize == 1 or intDataSize == 2 or intDataSize == 4 or intDataSize == 8:
					reqResItemObj.isStdDataSize = True
				
				if intDataSize == 1:
					reqResItemObj.dataType = "uint8_t"
				elif intDataSize == 2:
					reqResItemObj.dataType = "uint16_t"
				elif intDataSize == 4:
					reqResItemObj.dataType = "uint32_t"
				elif intDataSize == 8:
					reqResItemObj.dataType = "uint64_t"
			except ValueError:
				reqResItemObj.dataSize = ""

			if "Details" in reqResItem:
				reqResItemObj.detail = reqResItem["Details"]
			else:
				reqResItemObj.detail = ""
			reqResItemObj.detail = reqResItemObj.detail.replace('\n', '\\n')
			reqResItemObj.detail = reqResItemObj.detail.replace('"', '\\"')

			for reqRespClass in self.reqRespClasses:
				if reqRespClass.memClassName == reqResItem["MemClassName"]:
					reqRespClass.reqRespItems.append(reqResItemObj)
					break

class ServXmlParser:
	def __init__(self, servXmlFilePath, servXsdFilePath, servSubXmlFilePath, servSubXsdFilePath):
		self.servSubXmlFilePath = os.path.abspath(servSubXmlFilePath)
		self.servSubXsdFilePath = os.path.abspath(servSubXsdFilePath)
		self.servXmlFilePath = os.path.abspath(servXmlFilePath)
		self.servXsdFilePath = os.path.abspath(servXsdFilePath)
		self.servCont = ServCont()

		if not os.path.exists(self.servXmlFilePath):
			raise Exception("xml file not exist. " + str(self.servXmlFilePath))
			return

		if not os.path.exists(self.servXsdFilePath):
			raise Exception("xsd file not exist. " + str(self.servXsdFilePath))
			return
		
		if not os.path.exists(self.servSubXmlFilePath):
			raise Exception("xml file not exist. " + str(self.servSubXmlFilePath))
			return
		if not os.path.exists(self.servSubXsdFilePath):
			raise Exception("xsd file not exist. " + str(self.servSubXsdFilePath))
			return

		self.__parse()
	
	def getServCont(self):
		return self.servCont
	
	def __parse(self):
		xsd = xmlschema.XMLSchema(self.servXsdFilePath)
		if not xsd.is_valid(self.servXmlFilePath):
			errors = xsd.validate(self.servXmlFilePath, use_defaults=False)
			raise Exception(
				"xml file not according to xsd. " +
				str(self.servXmlFilePath) +
				str(self.servXsdFilePath) +
				" Errors: " + str(errors)
			)
			return
		
		xml_data = xsd.to_dict(self.servXmlFilePath)

		for servXml in xml_data.get('Service', []):
			serv = self.__parseItem(servXml)
			self.servCont.add(serv)

	def __parseItem(self, servXml):
		servItem = ServItem()
		servItem.nameForProgrammer = servXml["NameForProgrammer"]
		servItem.shortName = servXml["ShortName"]
		servItem.hexStr = servXml["Hex"]

		try:
			servItem.hex = int(servItem.hexStr, 16)
		except Exception as e:
			print(e)
			raise ValueError(servItem.nameForProgrammer + " hex cannot be converted")
		
		if servItem.hex > 255:
			raise ValueError(servItem.nameForProgrammer + " hex out of bound")
		
		servItem.defaultSess = servXml["DefaultSession"].replace(' ', '_')
		servItem.nonDefaultSess = servXml["NonDefaultSession"].replace(' ', '_')

		servSubXmlParser = ServSubXmlParser(servItem.nameForProgrammer , self.servSubXmlFilePath, self.servSubXsdFilePath)

		servItem.servSubCont = servSubXmlParser.getCont()

		return servItem

