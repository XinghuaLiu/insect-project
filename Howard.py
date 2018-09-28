#!/usr/bin/python
from __future__ import print_function
import sys,os
from ctypes import *

# Our C library
cstuff=CDLL("/home/ema/Downloads/cameraTeo/libcstuff.so")

# XCLIB's so, taken out of the xcap files
epix=CDLL("/usr/local/xcap/program/libkxclib_x86_64.so")
epix.pxd_SILICONVIDEO_getMinMaxFramePeriod.restype=c_double
epix.pxd_SILICONVIDEO_getFramePeriod.restype=c_double
epix.pxd_SILICONVIDEO_getPixelClock.restype=c_double

# Look for arguments on command line; otherwise prompt user
try:
    delay=int(sys.argv[1])
    nLines=int(sys.argv[2])
    nSeconds=float(sys.argv[3])
    linePos=int(sys.argv[4])
except:
    delay=input("How many microseconds would you like to pass between each frame? ")
    nSeconds=input("How many seconds would you like to record? ")
    nLines=input("How many pixel lines would you like to record? ")
    linePos=input("Central line height? (max 480) ")

# Fix linePos so that it's not under 0 or over 640.
# Also move it. Dr. Ene wants this program to ask the user for the "central line" (if you want lines 0, 1, 2, 3, 4, you should enter 2). XCLIB cares only about the topmost line (line 0 in that example).
linePos=min([max([linePos-nLines/2,0]),480])

# Every time we use an XCLIB function, we check for successful execution.
def checkEpixError(fun,retval,xdim=0,nLines=0):
    if (fun!='readushort' and retval!=0) or (fun=='readushort' and retval!=xdim*nLines):
        print(fun+' returned an erroneous value of '+str(retval))
        print(epix.pxd_mesgFault(0x01))


# The camera thinks it can take pictures faster than it actually can.
# A conversion factor is required, otherwise the data is junk.
# The conversion factors for 1, 2, 4, 6, and 8 lines were experimentally derived to be the following.
# If more than 8 lines are used, this will limit the speed of the camera more than it actually should. So if the user ends up using more than 8 lines and requiring high speeds, this can be revisited.
def minFramePeriod(aoiheight):
    if aoiheight==8:
        k=1.16*1.5
# I do not know why the following case is included. As far as I can tell, the camera cannot do 1 single line, only a minimum of 2?
    elif aoiheight==1:
        k=1.36*1.5
    elif aoiheight==6:
        k=1.185*1.5
    elif aoiheight==4:
        k=1.26*1.5
    elif aoiheight==2:
        k=1.36*1.5
    else:
        k=1.75

# Ask the camera what the fastest frame rate achievable is.
    x=c_double(epix.pxd_SILICONVIDEO_getMinMaxFramePeriod(0x1,c_double(0)))
# Multiply this frame-rate by our experimental constant, k.
    x=c_double(k*x.value)
# Ask the camera what the closest achievable frame rate to x*k is.
    x=c_double(epix.pxd_SILICONVIDEO_getMinMaxFramePeriod(0x1,x))
# We now have the minimum achievable frame rate for a given resolution.

# Check what frame rate the user actually needs.
    msdelay=c_double(float(delay)/1000)
# Get the closest achievable frame rate.
    msdelay=c_double(epix.pxd_SILICONVIDEO_getMinMaxFramePeriod(0x1,msdelay))

# Return either the user's desired frame rate, or the minimum achievable.
    return max([x,msdelay],key = lambda x: x.value)

# Shorthand, due to the length of the function.
def _setResAndTime(aoitop,aoiwidth,aoiheight,scandir,clock,frame):
    checkEpixError('setResolutionAndTiming',epix.pxd_SILICONVIDEO_setResolutionAndTiming(0x1,0,0x0101,0,aoitop,aoiwidth,aoiheight,scandir,10,0,0,clock,frame,c_double(0),c_double(0),c_double(0)))

# Set resolution and timing to certain settings.
# This is cannibalized left-over of earlier code. It was once used to set the resolution and timing to certain settings. It is now used only to calculate an appropriate frame rate, as detailed in minFramePeriod.
def setResAndTim(aoitop,aoiwidth,aoiheight):
## ('L'<<8)|'T' = 19540
## ('L'<<8)|'B' = 19522
## ('R'<<8)|'T' = 21076
## ('R'<<8)|'B' = 21058
    scandir=19540
    aoiwidth=epix.pxd_SILICONVIDEO_getMinMaxAoiWidth(0x01,aoiwidth)
    aoiheight=epix.pxd_SILICONVIDEO_getMinMaxAoiHeight(0x01,aoiheight)
    _setResAndTime(aoitop,aoiwidth,aoiheight,scandir,c_double(70),c_double(0))
    return minFramePeriod(aoiheight)

# The function of this is basically explained in minFramePeriod.
def fixDelay():
    global delay
    checkEpixError('PIXCIopen',epix.pxd_PIXCIopen("","",""))
    delay=setResAndTim(linePos,640,nLines)
    checkEpixError('PIXCIclose',epix.pxd_PIXCIclose("","NSTC",""))

if __name__=='__main__':
# Fix the user's requested frame rate.
    fixDelay()
# If the frame rate changes, so does the number of images that can be taken within a certain time-span.
    nImages=int(10**3*nSeconds/delay.value)

# Although not required, there should be some code here to fix the user's requested resolution. For example, the camera cannot handle 3 or 5 lines, only 2, 4, or 6. This should REALLY be handled here instead of later in the code.

# Call upon the C code.
    cstuff.work(nLines,nImages,delay,linePos)

# Delete all temporary image files.
    os.system("rm datapart*")
