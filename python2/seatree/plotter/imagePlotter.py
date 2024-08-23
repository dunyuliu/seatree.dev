import pygtk
pygtk.require('2.0')
import gtk, os, sys

from plotter import Plotter
import shutil

class ImagePlotter(Plotter):
	
	def __init__(self, module, mainWindow, imageWidth, imageHeight, saveType=["png", "PNG Image"], bgColor="white"):
		"""
		Constructor for an Image Plotter. This type of plotter just displays simple image plots
		
		module - the module using this plotter
		mainWindow - the MainWindow displaying this plotter
		imageWidth - the minimum width of the image
		imageHeight - the minimum height of the image
		saveType - the save type for this image (all images that are shown with this plotter should
					be of this type). Default is a PNG image.
		bgColor - the name of the background color for the image widget (default is "white")
		"""
		
		# call the superclass constructor
		Plotter.__init__(self, module, mainWindow)
		
		self.imageWidth = imageWidth
		self.imageHeight = imageHeight
		self.saveType = saveType
		self.bgColor = bgColor
		
		if (self.module.baseimage):
			self.imageFile = self.module.directory + os.sep + self.module.baseimage
		else:
			self.imageFile = self.module.seatreePath + os.sep + "img" + os.sep + "seatree.jpg"
		
	def getMainWidget(self):
		# return the Image Event Box for this image plotter
		
		self.image = gtk.Image()
		self.image.show()
		
		self.imageBuffer = 7
		self.imageEB = gtk.EventBox()
		self.imageEB.add(self.image)
		self.imageEB.modify_bg(gtk.STATE_NORMAL, gtk.gdk.color_parse(self.bgColor))
		self.imageEB.set_size_request(self.imageWidth + self.imageBuffer, self.imageHeight + self.imageBuffer)
		
		self.baseImage = self.imageFile
		
		self.displayImage(self.imageFile, default=True)
		
		self.imageEB.show_all()
		
		return self.imageEB
	
	def getBottomPanel(self):
		# no bottom panel for default image plotter
		return None
	
	def getSaveTypes(self):
		saveTypes = []
		saveTypes.append(self.saveType)
		return saveTypes
	
	def savePlot(self, typeExtension, fileName):
		# don't check for save types since we only have 1
		if (self.imageFile):
			shutil.copyfile(self.imageFile, fileName)
			return True
		else:
			return False
	
	def displayImage(self, imageFile, default=False):
		"""
		This method will display an image file
		
		imageFile - the file name of the image to be displayed
		"""
		self.imageFile = imageFile
		self.image.set_from_file(self.imageFile)
		
		if not default:
			self.mainWindow.setSaveActive(True)
		