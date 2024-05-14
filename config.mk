###############################################################################
# CUTOMIZE THIS FILE TO SUIT YOUR NEEDS FOR THE BUILD PROCESS                 #
# This file is included into the following files (refer to them for more      #
# information on the build process):                                          #
#  - Makefile                                                                 #
###############################################################################

# The version number of the box library ($(VMAJ).$(VMIN).$(VREV))
VMAJ = 1
VMIN = 1
VREV = 0
VDEB = 1

# the RTS library
LIB_SMXZLOG = -lsmxzlog-1.0

# the include path of the smxzlog header files
INC_SMXZLOG = -I/usr/include/smx/libsmxzlog-1.0

# the name of the library
LIBNAME = smxrts
