import pygtk
pygtk.require('2.0')
import gtk, os, sys, xml.dom.minidom, subprocess, math

from seatree.gmt.gmtWrapper import GMTWrapper
from seatree.gmt.gmtWrapper import GMTProjection
from seatree.plotter.gmt.gmtPlotter import GMTPlotter

try:
	import seatree.plotter.matPlotLib.matPlotLibPlotter as matPlotLibPlotter
	from seatree.plotter.matPlotLib.matPlotLibPlotter import MatPlotLibPlotter
	import matplotlib
	can_use_pylab = True
except:
	stackTrace = False
	if stackTrace:
		import traceback
		traceback.print_exception(*sys.exc_info())
	can_use_pylab = False

from seatree.modules.module import Module
from seatree.util.scriptRunner import ScriptRunner
from syn2dGUI import Syn2DGUI
from pgmImage import PGMImage
import numpy

class Syn2D(Module):
	
	PLOT_TYPE_GMT = "GMT"
	PLOT_TYPE_PYLAB = "MatPlotLib"
	
	PLOT_MODEL = "Model"
	PLOT_DATA = "Data"
	PLOT_INVERSION = "Inversion"
	PLOT_DIFFERENCE = "Differnece"
	
	def __init__(self):
		'''
		Syn2D - 2D Cartesian Tomography SEATREE module.
		'''
		# short name for the module
		shortName = "Syn2D"
		
		# long, display name for the module
		longName =  "Syn2D - 2D Cartesian Tomography"
		
		# version number
		version = 1.1
		
		# name of the directory that should be created inside of the users
		# home directory, inside of the .seatree folder. this folder should
		# store user-specific configuration files, and the path to this folder
		# can be found in the self.storeDir variable once a module is loaded
		storeName = "syn2d"
		
		# this is the name of the image that should be initially displayed in
		# the plot view. this should just be the image name, and a path. The
		# image must be in the same directory as the module. If you don't have
		# an image, just make it an empty string as below.
		baseImage = ""
		
		# this calls the Module constructor with the above variables
		Module.__init__(self, shortName, longName, version, storeName, baseImage)
		
		self.plotWidth = 400
		
		self.verb = 3
		self.commandString = ""
		self.error = ""
		
		# make model files
		self.xyzFile = ""
		self.pxFile = ""
		
		# inversion files
		self.invertXYZFile = ""
		self.lastInvertPrefix = ""
		
		self.lastType = ""
		self.lastRaysPrefix = ""
		
		if can_use_pylab:
			self.plotType = self.PLOT_TYPE_PYLAB
		else:
			self.plotType = self.PLOT_TYPE_GMT
		
		self.lastPlot = ""
	
	def getPanel(self, mainWindow, accelGroup):
		'''
		This method should return a gtk.Widget to be displayed in the main
		SEATREE window on the left. Usually this will be a gtk.VBox, but any
		displayable gtk.Widget will suffice
		'''
		self.gui = Syn2DGUI(mainWindow, accelGroup, self)
		return self.gui.getPanel()
	
	def setDefaults(self, mainWindow):
		'''
		This is the first method called on an object when a module is loaded.
		
		tmpn -- prefix for temporary files.
		gmtPath -- path to gmt binaries that should be given to the module's GMTPlotter
		mainWindow -- main GUI window
		'''
		
		# load configuration
		self.loadConfFile()
		
		self.mainWindow = mainWindow
		
		tmpn = self.mainWindow.getTempFilePrefix()
		self.gmtPath = self.mainWindow.getGMTPath()
		
		self.computeDir = os.path.dirname(tmpn)
		if not self.computeDir.endswith(os.sep):
			self.computeDir += os.sep
		self.tmpn = tmpn
		
		self.scriptRunner = ScriptRunner(workingDir = self.computeDir)
		
		if (self.isGMT()):
			print "Using GMT plotter!"
			self.setGMTDefaults()
			self.gmtPlotterWidget = GMTPlotter(self, self.mainWindow, 450, 450, self.mainWindow.getConvertPath(), self.gmtPlotter)
			self.gmtPlotterWidget.gmtSettingsPanel.loadDefaults()
			self.matPlotLibPlotter = None
		else:
			print "Using PyLab plotter!"
			self.gmtPlotterWidget = None
			self.matPlotLibPlotter = MatPlotLibPlotter(self, self.mainWindow, 450, 450, startWithImage=False)
			self.matPlotLibPlotter.setColorLimits(-1, 1)
			self.matPlotLibPlotter.setAspectRatioEven(True)
			cm = matplotlib.cm.Spectral
			cm = self.matPlotLibPlotter.reverseColormap(cm)
			self.matPlotLibPlotter.setColorMap(cm)
		
		self.sourcesFile = ""
		self.receiversFile = ""
	
	def canPlotMPL(self):
		return can_use_pylab
	
	def isGMT(self):
		return self.plotType == self.PLOT_TYPE_GMT
	
	def setPlotType(self, type):
		if self.plotType != type:
			# it's a different type
			if type == self.PLOT_TYPE_PYLAB:
				if not self.matPlotLibPlotter:
					self.matPlotLibPlotter = MatPlotLibPlotter(self, self.mainWindow, 450, 450, startWithImage=False)
				self.mainWindow.loadPlotter(self.matPlotLibPlotter)
			else:
				if not self.gmtPlotterWidget:
					self.setGMTDefaults()
					self.gmtPlotterWidget = GMTPlotter(self, self.mainWindow, 450, 450, self.mainWindow.getConvertPath(), self.gmtPlotter)
					self.gmtPlotterWidget.gmtSettingsPanel.loadDefaults()
				self.mainWindow.loadPlotter(self.gmtPlotterWidget)
		self.plotType = type
	
	def getPlotter(self):
		"""
		This method is called at the end of the loading process and returns the Plotter
		object for the module.
		"""
		if (self.isGMT()):
			return self.gmtPlotterWidget
		else:
			return self.matPlotLibPlotter
	
	def cleanup(self):
		"""
		This method will be called when the module is closed or SEATREE is exited.
		It should call the cleanup function of the GMTPlotter and do any other
		necessary cleanup operations.
		"""
		if self.gmtPlotterWidget:
			self.gmtPlotter.cleanup()
	
	def loadConfFile(self):
		doc = xml.dom.minidom.parse(self.seatreePath + os.sep + "conf" + os.sep + "syn2d" + os.sep + "syn2dConf.xml")
		
		# load chkbd path
		chkbdNode = doc.getElementsByTagName("chkbdPath")
		if (chkbdNode and chkbdNode[0].firstChild):
			chkbdpath = chkbdNode[0].firstChild.nodeValue.strip()
			
			if (not chkbdpath):
				chkbdpath = ""
			elif not chkbdpath.endswith(os.sep):
				chkbdpath = chkbdpath + os.sep
		else: chkbdpath = ""
		self.chkbdPath = chkbdpath
		
		# load path to makedata binaries
		makedataBinNode = doc.getElementsByTagName("makedataBinPath")
		if (makedataBinNode and makedataBinNode[0].firstChild):
			makedataBinPath = makedataBinNode[0].firstChild.nodeValue.strip()
			
			if (not makedataBinPath):
				makedataBinPath = ""
			elif not makedataBinPath.endswith(os.sep):
				makedataBinPath = makedataBinPath + os.sep
		else: makedataBinPath = ""
		self.makedataBinPath = makedataBinPath
		
		# load path to makedata binaries
		invertBinNode = doc.getElementsByTagName("invertBinPath")
		if (invertBinNode and invertBinNode[0].firstChild):
			invertBinPath = invertBinNode[0].firstChild.nodeValue.strip()
			
			if (not invertBinPath):
				invertBinPath = ""
			elif not invertBinPath.endswith(os.sep):
				invertBinPath = invertBinPath + os.sep
		else: invertBinPath = ""
		self.invertBinPath = invertBinPath
	
	def runCommand(self, command):
		""" Run Given Command in Shell """
		if (self.verb > 2): print "Command: " + command
		
		self.commandString += command + "\n"
		
		result = self.scriptRunner.runScript(command)
		
		out = result.getStandardOutput()
		err = result.getStandardError()
		ret = result.getReturnValue()
		if (err):
			self.error += err
		if (self.verb > 1 and out): print out
		if (self.verb > 2 and err): print err
		return ret
	
	def updatePlot(self):
		if (self.lastPlot):
			self.gui.setPlotSettingsChanged()
			file = ""
			if (self.lastPlot == self.PLOT_MODEL):
				file = self.plotModel(self.dx)
			elif (self.lastPlot == self.PLOT_DATA):
				file = self.plotData(self.xmax, self.plotReceivers, self.plotSources, self.plotPaths)
			elif (self.lastPlot == self.PLOT_INVERSION):
				file = self.plotInversion(self.xmax, self.dx)
			elif (self.lastPlot == self.PLOT_DIFFERENCE):
				file = self.plotDifference(self.xmax, self.dx, self.diffAbs)
			if (file):
				self.gmtPlotterWidget.displayPlot(file)
	
	def setGMTDefaults(self):
		self.gmtPlotter = GMTWrapper(verb=3,path=self.gmtPath, runDir=self.computeDir)
		self.gmtPlotter.setColormapType("polar")
		self.gmtPlotter.setColormapInvert(True)
		# set plot options
		self.gmtPlotter.setMapProjection(GMTProjection("X","","",7,""))
		self.gmtPlotter.setPlotOffset(0, 1.5)
		self.gmtPlotter.setBoundaryAnnotation("a20f2/a20f2WeSn")
		self.gmtPlotter.setPortraitMode(1)
		# set colorbar options
		self.gmtPlotter.setColorbarHorizonal(1)
		self.gmtPlotter.setColorbarTriangles(1)
		self.gmtPlotter.setColorbarPos(3.5, -0.5)
		self.gmtPlotter.setColorbarSize(5, 0.25)
		self.gmtPlotter.setColorbarInterval(0.25)
		
		self.gridRange = None
		
	def makeCheckerboardModel(self, xtot, dx, size):
		print "Making Checkerboard Model"
		
		
		#
		# generate a square region checkerboard model
		#
		ytot = xtot
		
		# equal parameterization increment in both directions:
		dy = dx
		
		#
		# length (in parameterization pixels) of side of checkerboard elements:
		#
		dcheckx=size
		dchecky=dcheckx
		
		#
		# abs(amplitude of anomalies):
		anoma=1.0
		
		command = self.chkbdPath + "chkbd"
		command += "<<EOF\n"
		command += str(dx) + "\n"
		command += str(dy) + "\n"
		command += str(xtot) + "\n"
		command += str(ytot) + "\n"
		command += str(dchecky) + "\n"
		command += str(dcheckx) + "\n"
		command += str(anoma) + "\n"
		command += "EOF"
		
		self.runCommand(command)
		
		self.xyzFile = self.computeDir + "chkbd.xyz"
		self.pxFile = self.computeDir + "chkbd.px"
		self.lastType = "chkbd"
		
		self.gridRange = None
		self.dx = dx
	
	def getDefaultImage(self):
		return self.seatreePath + os.sep + "data" + os.sep + "syn2d" + os.sep + "image2.pgm"
	
	def makeImageModel(self, xtot, dx, fileName):
		print "Making Image Model"
		image = PGMImage(fileName)
		max = image.getMax()
		
		if not (xtot == image.getWidth() and xtot == image.getHeight()): # if it's not a square that's with/height of xtot
			print "Image size is incorrect. For now must be perfect square"
			print "Expected: " + str(xtot) + "x" + str(xtot)
			print "Encountered: " + str(image.getWidth()) + "x" + str(image.getHeight())
			return # fail
		
		# make the xyz file from the image
		self.xyzFile = self.computeDir + "image.xyz"
		xyzFP = open(self.xyzFile, 'w')
		self.pxFile = self.computeDir + "image.px"
		pxFP = open(self.pxFile, 'w')
		for y in range(image.getHeight()):
			for x in range(image.getWidth()):
				num = image.getPixel(x, y, flip=True)
				z = self.getZ(num, max)
				xyzFP.write(str(x) + "\t" + str(y) + "\t" + str(z) + "\n")
				pixelIndex = x + image.getWidth() * y + 1
				pxFP.write(str(pixelIndex) + " " + str(z) + "\n")
		xyzFP.close()
		pxFP.close()
		self.lastType = "image"
		
		self.gridRange = None
		self.dx = dx
		
		return image.getWidth()
	
	def getZ(self, num, max):
		scaled = float(num) * 2.0 / 255
		return scaled - 1.0
	
	def plotModel(self, dx):
		self.dx = dx
		self.lastPlot = self.PLOT_MODEL
		if (self.isGMT()):
			return self.plotModelGMT(dx)
		else:
			return self.plotModelMPL(dx)
	
	def plotModelToExistingMPL(self):
		self.matPlotLibPlotter.plotXYZFromSquareDataFile(self.xyzFile, title="Input Model", colorBar=True)
	
	def plotModelMPL(self, dx):
		if (self.xyzFile):
			#print "Plotting with MatPlotLib!"
			
			self.matPlotLibPlotter.clearFigure()
			
			self.plotModelToExistingMPL()
			
			self.matPlotLibPlotter.drawFigure()
			
			"""
			self.axis = self.matPlotLibPlotter.getAxis()
			self.axis.clear()
			self.figure = self.matPlotLibPlotter.getFigure()
			
			a = matplotlib.mlab.load(self.xyzFile)     # using pylab function write to numpy array A
			n = int(math.sqrt(a.shape[0]))             # assume square
			x=a[:,0].reshape(n,n)
			y=a[:,1].reshape(n,n)
			z=a[:,2].reshape(n,n)
			
			cmap = matplotlib.cm.jet
			
			image = self.axis.pcolor(x, y, z, cmap=cmap, shading='flat')
			self.axis.set_xlim(0,n-1)
			self.axis.set_ylim(0,n-1)
			self.figure.colorbar(image)
			self.axis.set_title('input model')
			self.matPlotLibPlotter.drawFigure()
			"""
			
			#matPlotLibPlotter.show()
	
	def plotModelToExistingGMT(self, dx):
		self.grdFile = self.tmpn + "model.grd"
		self.gmtPlotter.spatialToNetCDF(dx, "cat " + self.xyzFile, self.grdFile, False, verbose=True)
		self.gmtPlotter.setPlotRange(self.gridRange[0], self.gridRange[1], self.gridRange[2], self.gridRange[3])
		
		cptOut = self.tmpn + "cpt.cpt"
		self.gmtPlotter.makeCPT(-1.0, 1.0, 0.1, cptOut)
		
		self.gmtPlotter.createImageFromGrid(self.grdFile)
	
	def plotModelGMT(self, dx):
		if (self.xyzFile):
			self.gmtPlotter.detectGridRange(dx, self.xyzFile)
			self.gridRange = self.gmtPlotter.getGridRange()
			self.psFile = self.tmpn + "model.ps"
			
			# set colorbar options
			self.gmtPlotter.setColorbarHorizonal(1)
			self.gmtPlotter.setColorbarTriangles(1)
			self.gmtPlotter.setColorbarPos(3.5, -0.5)
			self.gmtPlotter.setColorbarSize(5, 0.25)
			self.gmtPlotter.setColorbarInterval(0.25)
			
			# initialize the PS file
			self.gmtPlotter.initPSFile(self.psFile)
			# plot the GRD file
			self.plotModelToExistingGMT(dx)
			# plot the color scale
			self.gmtPlotter.drawColorbar()
			# modify the bounding box
			self.gmtPlotter.setBoundingBox(30, 30, 610, 650)
			# close the PS file
			self.gmtPlotter.closePSFile()
			return self.psFile
	
	def makeData(self, xtot, dx, ndata, sigma, ipick, station_mode):
		
		# square for now at least
		dy = dx
		ytot = xtot
		
		# output file prefix
		name = "rays." + self.lastType
		
		# minimum acceptable epicentral distance
		deltamin = 10
		
		# make the data
		
		command = self.makedataBinPath + "make_sr"
		command += "<<EOF\n"
		command += str(xtot) + "\n"
		command += str(ytot) + "\n"
		command += str(ndata) + "\n"
		command += str(deltamin) + "\n"
		command += str(station_mode) + "\n"
		command += str(ipick) + "\n"
		command += "EOF"
		
		self.runCommand(command)
		
		# noiseless data
		
		self.sourcesFile = self.computeDir + "sources.txt"
		self.receiversFile = self.computeDir + "receivers.txt"
		self.lastRaysPrefix = name
		
		self.dx = dx
		self.dy = dy
		self.xtot = xtot
		self.ytot = ytot
		self.sigma = sigma
		self.ndata = ndata
		
		if station_mode < 0:
			# this is a custom source/reciever list
			pathsFile = self.computeDir + "paths.txt"
			fp = open(pathsFile, "r")
			ndata = 0
			for line in fp.readlines():
				if len(line) > 0:
					ndata += 1
			fp.close()
			self.ndata = ndata
		
		self.raysShot = False
		
		return self.ndata
	
	def shootRays(self):
		name = self.lastRaysPrefix
		
		# matrix format
		binary = 1
		
		# increment along ray path
		rpinc = 0.05
		
		command = self.makedataBinPath + "shootray_sr"
		command += "<<EOF\n"
		command += str(self.dx) + "\n"
		command += str(self.dy) + "\n"
		command += str(self.xtot) + "\n"
		command += str(self.ytot) + "\n"
		command += name + "\n"
		command += str(binary) + "\n"
		command += str(rpinc) + "\n"
		command += "EOF"
		
		self.runCommand(command)
		
		nfree = int(math.pow((self.xtot / self.dx), 2))
		# seed for random...should be current time?
		seed = -1
		
		#		command = self.makedataBinPath + "noisydatamaker"
#		command += "<<EOF\n"
#		command += name + ".xxx" + "\n"
#		command += name + ".ind" + "\n"
#		command += name + ".pnt" + "\n"
#		command += self.pxFile + "\n"
#		command += name + ".rhs" + "\n"
#		command += str(nfree) + "\n"
#		command += str(ndata) + "\n"
#		command += "0" + "\n"
#		command += "-1" + "\n"
#		command += "EOF"
#		self.runCommand(command)
		
		# noisy data
		
		command = self.makedataBinPath + "noisydatamaker"
		command += "<<EOF\n"
		command += name + ".xxx" + "\n"
		command += name + ".ind" + "\n"
		command += name + ".pnt" + "\n"
		command += self.pxFile + "\n"
		command += name + ".rhs" + "\n"
		command += str(nfree) + "\n"
		command += str(self.ndata) + "\n"
		command += str(self.sigma) + "\n"
		command += str(seed) + "\n"
		command += "EOF"
		self.runCommand(command)
		
		self.raysShot = True
	
	def setDataFiles(self, sources="", receivers=""):
		self.sourcesFile = sources
		self.receiversFile = receivers
	
	def getDataFiles(self):
		return (self.sourcesFile, self.receiversFile)
	
	def plotData(self, xmax, plotReceivers, plotSources, plotPaths, plotModel):
		self.xmax = xmax
		self.plotReceivers = plotReceivers
		self.plotSources = plotSources
		self.plotPaths = plotPaths
		self.lastPlot = self.PLOT_DATA
		if (self.isGMT()):
			return self.plotDataGMT(xmax, plotReceivers, plotSources, plotPaths, plotModel)
		else:
			return self.plotDataMPL(xmax, plotReceivers, plotSources, plotPaths, plotModel)
	
	def loadXYFile(self, file):
		return self.matPlotLibPlotter.loadXYFile(file)
	
	def plotDataMPL(self, xmax, plotReceivers, plotSources, plotPaths, plotModel):
		self.matPlotLibPlotter.clearFigure()
		
		label = ""
		
		if (plotModel):
			self.plotModelToExistingMPL()
		
		if (plotPaths):
			# plot the paths
			self.plotPathsMPL()
		
		if (plotSources):
			# plot the receivers
			x, y = self.plotSourcesMPL()
			
			if label:
				label += ", "
			label += "Sources: " + str(len(x))
		
		if (plotReceivers):
			# plot the receivers
			x, y = self.plotReceiversMPL()
			
			if label:
				label += ", "
			label += "Receivers: " + str(len(x))
		
		if label:
			self.matPlotLibPlotter.addTextLabel(0.05, 0.03, label, fontsize=16)
		
		self.matPlotLibPlotter.limitAxis(0, 99, 0, 99)
		
		self.matPlotLibPlotter.drawFigure()
	
	def plotSourcesMPL(self):
		x, y = self.matPlotLibPlotter.loadXYFile(self.computeDir + "sources.txt")
		type = matPlotLibPlotter.PENTAGRAM
		self.matPlotLibPlotter.plotScatterData(x, y, type=type, color="r", colorMap=None, colorBar=False, setAsImage=False, size=60)
		return x, y
	
	def plotReceiversMPL(self):
		x, y = self.matPlotLibPlotter.loadXYFile(self.computeDir + "receivers.txt")
		type = matPlotLibPlotter.TRIANGLE_DOWN
		self.matPlotLibPlotter.plotScatterData(x, y, type=type, color="b", colorMap=None, colorBar=False, setAsImage=False, size=60)
		return x, y
	
	def plotPathsMPL(self):
		fp = open(self.computeDir + "paths.txt")
		
		polys = []
		for line in fp.readlines():
			if len(line) > 0:
				pnts = line.split()
				p = []
				for pnt in pnts:
					p.append(float(pnt)) 
				polys.append(((p[0], p[1]), (p[2], p[3])))
		fp.close()
		
		for poly in polys:
			self.matPlotLibPlotter.plotPolygon(poly, arrows=False)
		
		label = "Paths: " + str(len(polys))
	
	def plotDataGMT(self, xmax, plotReceivers, plotSources, plotPaths, plotModel):
		# file to plot to
		self.psFile = self.tmpn + "rays.ps"
		
		if not (self.gridRange):
			self.gmtPlotter.detectGridRange(self.dx, self.xyzFile)
			self.gridRange = self.gmtPlotter.getGridRange()
			self.gmtPlotter.setPlotRange(self.gridRange[0], self.gridRange[1], self.gridRange[2], self.gridRange[3])
		
		if plotModel:
			self.gmtPlotter.initPSFile(self.psFile, xOff=0, yOff=1.5)
			self.plotModelToExistingGMT(self.dx)
		else:
			# initialize the PS file with a basemap
			self.gmtPlotter.initPSFile(self.psFile, xOff=0, yOff=1.5, basemap=True)
		
		if (plotPaths):
			self.plotPathsGMT()
		
		if (plotSources):
			# plot the sources
			self.plotSourcesGMT()
		
		if (plotReceivers):
			# plot the receivers
			self.plotReceiversGMT()
		
		# modify the bounding box
		self.gmtPlotter.setBoundingBox(30, 0, 610, 700)
		#self.gmtPlotter.setModifyBoudingBox(False)
		# close it
		self.gmtPlotter.closePSFile()
		return self.psFile
	
	def plotSourcesGMT(self):
		self.gmtPlotter.plotXY("sources.txt", colorName="red", plotSymbols=True, symbol="a", symbolSize=0.2)
	
	def plotReceiversGMT(self):
		self.gmtPlotter.plotXY("receivers.txt", colorName="blue", plotSymbols=True, symbol="i", symbolSize=0.25)
	
	def plotPathsGMT(self):
		# reformat the paths
		gmtPaths = "paths_gmt.txt"
		inFile = open(self.computeDir + "paths.txt", "r")
		outFile = open(self.computeDir + gmtPaths, "w")
		
		for line in inFile.readlines():
			if len(line) > 0:
				split = line.split()
				outFile.write("  " + str(float(split[0])) + " " + str(float(split[1])) + "\n")
				outFile.write("  " + str(float(split[2])) + " " + str(float(split[3])) + "\n")
				outFile.write(">" + "\n")
		
		inFile.close()
		outFile.close()
		
		# plot the paths
		self.gmtPlotter.plotPolygon(gmtPaths, 0.25, 0, 255, 0)
	
	def invert(self, xtot, dx, ndata, damp):
		
		self.differenceXYZ = None
		
		if not self.raysShot:
			self.shootRays()
		
		# square for now at least
		dy = dx
		ytot = xtot
		
		outfile = self.lastRaysPrefix + "." + str(damp)
		self.lastInvertPrefix = outfile
		command = self.invertBinPath + "invray"
		command += "<<EOF\n"
		command += '"' + self.lastRaysPrefix + '"' + "\n"
		command += '"' + self.lastRaysPrefix + ".rhs" + '"' + "\n"
		command += str(ndata) + "\n"
		command += str(dx) + "\n"
		command += str(dy) + "\n"
		command += str(xtot) + "\n"
		command += str(ytot) + "\n"
		command += '"' + outfile + '"' + "\n"
		command += str(damp) + "\n"
		command += "EOF"
		if (self.runCommand(command) == 0):
			self.invertXYZFile = outfile + ".xyz"
			
			fp = open(self.computeDir + "solstat.log", "r")
			
			line = fp.readlines()[0]
			
			line = line.strip()
			
			valStrs = line.split()
			
			norm = float(valStrs[0])
			vr = float(valStrs[1]) * 100.0
			
			#norm = int(norm * 100 + 0.5)
			#norm = float(norm) / 100.0
			norm = self.round(norm, 2)
			vr = self.round(vr, 2)
			
			self.invertLabel = "norm = " + str(norm) + ", VR = " + str(vr) + " %"
			
			return True
	
	def round(self, num, digits):
		multiple = float(math.pow(10, digits))
		num = num * multiple
		num = int(num + 0.5)
		num = float(num) / multiple
		return num
	
	def plotInversion(self, xmax, dx, plotSources=False, plotReceivers=False, plotPaths=False):
		self.xmax = xmax
		self.dx = dx
		self.lastPlot = self.PLOT_INVERSION
		if (self.isGMT()):
			return self.plotInversionGMT(xmax, dx, plotSources, plotReceivers, plotPaths)
		else:
			return self.plotInversionMPL(xmax, dx, plotSources, plotReceivers, plotPaths)
	
	def plotInversionGMT(self, xmax, dx, plotSources, plotReceivers, plotPaths):
		# files to plot to/with
		self.psFile = self.tmpn + self.lastInvertPrefix + ".ps"
		self.grdFile = self.tmpn + self.lastInvertPrefix + ".grd"
		
		cptOut = self.tmpn + "cpt.cpt"
		
		if not (self.gridRange):
			self.gmtPlotter.detectGridRange(self.dx, self.xyzFile)
			self.gridRange = self.gmtPlotter.getGridRange()
			self.gmtPlotter.setPlotRange(self.gridRange[0], self.gridRange[1], self.gridRange[2], self.gridRange[3])
			
		self.gmtPlotter.makeCPT(-1.0, 1.0, 0.1, cptOut)
		
		# set colorbar options
		self.gmtPlotter.setColorbarHorizonal(1)
		self.gmtPlotter.setColorbarTriangles(1)
		self.gmtPlotter.setColorbarPos(3.5, -0.5)
		self.gmtPlotter.setColorbarSize(5, 0.25)
		self.gmtPlotter.setColorbarInterval(0.25)
		
		# convert to a GRD file
		self.gmtPlotter.setGridRange(0, xmax, 0, xmax)
		self.gmtPlotter.setNoDataValue(0)
		self.gmtPlotter.setForcePixelRegistration(True)
		self.gmtPlotter.spatialToNetCDF(dx, "cat " + self.invertXYZFile, self.grdFile, False, verbose=True)
		self.gmtPlotter.setForcePixelRegistration(False)
		
		# initialize the PS file
		self.gmtPlotter.initPSFile(self.psFile)
		# plot the GRD file
		self.gmtPlotter.createImageFromGrid(self.grdFile)
		if plotPaths:
			self.plotPathsGMT()
		if plotSources:
			self.plotSourcesGMT()
		if plotReceivers:
			self.plotReceiversGMT()
		# plot the color scale
		self.gmtPlotter.drawColorbar()
		# modify the bounding box
		self.gmtPlotter.setBoundingBox(30, 30, 610, 650)
		# close the PS file
		self.gmtPlotter.closePSFile()
		return self.psFile
	
	def plotInversionMPL(self, xmax, dx, plotSources, plotReceivers, plotPaths):
		self.matPlotLibPlotter.clearFigure()
		
		self.matPlotLibPlotter.plotXYZFromSquareDataFile(self.computeDir + self.invertXYZFile, title="Inversion", colorBar=True)
		
		if plotPaths:
			self.plotPathsMPL()
		if plotSources:
			self.plotSourcesMPL()
		if plotReceivers:
			self.plotReceiversMPL()
		
		self.matPlotLibPlotter.addTextLabel(0.05, 0.03, self.invertLabel, fontsize=16)
		
		self.matPlotLibPlotter.limitAxis(0, 99, 0, 99)
		
		self.matPlotLibPlotter.drawFigure()
	
	def plotDifference(self, xmax, dx, absVal=False, plotSources=False, plotReceivers=False, plotPaths=False):
		self.diffAbs = absVal
		self.xmax = xmax
		self.dx = dx
		self.lastPlot = self.PLOT_DIFFERENCE
		if (self.isGMT()):
			return self.plotDifferenceGMT(xmax, dx, plotSources, plotReceivers, plotPaths, absVal=absVal)
		else:
			return self.plotDifferenceMPL(xmax, dx, plotSources, plotReceivers, plotPaths, absVal=absVal)
	
	def getDifferenceArray(self, forGMT=False, absVal=False):
		orig = matplotlib.mlab.load(self.xyzFile)
		inv = matplotlib.mlab.load(self.computeDir + self.invertXYZFile)
		a = numpy.empty((len(orig), 3), dtype=orig.dtype)
		for i in range(len(orig)):
			origVal = orig[i]
			invVal = inv[i]
			if forGMT:
				a[i][0] = invVal[0]
				a[i][1] = invVal[1]
			else:
				a[i][0] = origVal[0]
				a[i][1] = origVal[1]
			if absVal:
				a[i][2] = abs(origVal[2] - invVal[2])
			else:
				a[i][2] = origVal[2] - invVal[2]
		return a
	
	def plotDifferenceGMT(self, xmax, dx, plotSources, plotReceivers, plotPaths, absVal=False):
		if not self.differenceXYZ:
			self.differenceXYZ = self.computeDir + "inv_diff.xyz"
			a = self.getDifferenceArray(forGMT=True, absVal=absVal);
			fp = open(self.differenceXYZ, "w")
			
			for pt in a:
				fp.write(str(pt[0]) + "\t" + str(pt[1]) + "\t" + str(pt[2]) + "\n")
			
			fp.close()
		
		# files to plot to/with
		self.psFile = self.tmpn + self.lastInvertPrefix + "_diff_" + ".ps"
		self.grdFile = self.tmpn + self.lastInvertPrefix + "_diff_" + ".grd"
		
		cptOut = self.tmpn + "cpt.cpt"
		
		if not (self.gridRange):
			self.gmtPlotter.detectGridRange(self.dx, self.xyzFile)
			self.gridRange = self.gmtPlotter.getGridRange()
			self.gmtPlotter.setPlotRange(self.gridRange[0], self.gridRange[1], self.gridRange[2], self.gridRange[3])
			
		self.gmtPlotter.makeCPT(-1.0, 1.0, 0.1, cptOut)
		
		# set colorbar options
		self.gmtPlotter.setColorbarHorizonal(1)
		self.gmtPlotter.setColorbarTriangles(1)
		self.gmtPlotter.setColorbarPos(3.5, -0.5)
		self.gmtPlotter.setColorbarSize(5, 0.25)
		self.gmtPlotter.setColorbarInterval(0.25)
		
		# convert to a GRD file
		self.gmtPlotter.setGridRange(0, xmax, 0, xmax)
		self.gmtPlotter.setNoDataValue(0)
		self.gmtPlotter.setForcePixelRegistration(True)
		self.gmtPlotter.spatialToNetCDF(dx, "cat " + self.differenceXYZ, self.grdFile, False, verbose=True)
		self.gmtPlotter.setForcePixelRegistration(False)
		
		# initialize the PS file
		self.gmtPlotter.initPSFile(self.psFile)
		# plot the GRD file
		self.gmtPlotter.createImageFromGrid(self.grdFile)
		if plotPaths:
			self.plotPathsGMT()
		if plotSources:
			self.plotSourcesGMT()
		if plotReceivers:
			self.plotReceiversGMT()
		# plot the color scale
		self.gmtPlotter.drawColorbar()
		# modify the bounding box
		self.gmtPlotter.setBoundingBox(30, 30, 610, 650)
		# close the PS file
		self.gmtPlotter.closePSFile()
		return self.psFile
	
	def plotDifferenceMPL(self, xmax, dx, plotSources, plotReceivers, plotPaths, absVal=False):
		self.matPlotLibPlotter.clearFigure()
		
		a = self.getDifferenceArray(absVal=absVal)
		
		n = int(math.sqrt(a.shape[0])) # determine square size
		m = n
		# determine geometry
		xmin, xmax = min(a[:,0]), max(a[:,0])
		ymin, ymax = min(a[:,1]), max(a[:,1])
		ranges = [ xmin , xmax, ymin, ymax ];
		
		# assign three columns to vectors
		x=a[:,0].reshape(n,m)
		y=a[:,1].reshape(n,m)
		z=a[:,2].reshape(n,m)
		
		self.matPlotLibPlotter.plotXYZData(x, y, z, title="Inversion", colorBar=True, range=ranges)
		
		if plotPaths:
			self.plotPathsMPL()
		if plotSources:
			self.plotSourcesMPL()
		if plotReceivers:
			self.plotReceiversMPL()
		
		self.matPlotLibPlotter.limitAxis(0, 99, 0, 99)
		
		self.matPlotLibPlotter.addTextLabel(0.05, 0.03, self.invertLabel, fontsize=16)
		
		#if absVal:
		#	self.matPlotLibPlotter.setColorLimits(0, 1)
		#else:
		#	self.matPlotLibPlotter.setColorLimits(-1, 1)
		
		self.matPlotLibPlotter.drawFigure()
	
	def getOutput(self):
		return self.commandString

	def clearOutput(self):
		self.commandString = ""
	
	def getWorkingDir(self):
		return self.computeDir