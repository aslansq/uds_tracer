                  PEAK-System GmbH PCAN Linux driver README

LICENSE

	This file comes with the peak-linux-driver-x.y.z sources package
	provided by PEAK-System Technik GmbH, Germany, for free. Refer to the
	files LICENSE.gpl and LICENSE.lgpl for more information on its use and
	the rights granted to you with regard to its redistribution.

	Roughly:

	LICENSE.gpl	defines the rights granted to the files that are saved
			into the "driver" sub-directory.
	LICENSE.lgpl	defines the rights granted to the files that are saved
			into all the other sub-directories.

	with the exception of files that explicitly mention a different license.

BUILD

	In addition to the standard tools that are needed to compile any
	C source files, the following packages (or their equivalent, depending
	on the current Linux distribution) might be mandatory to build one
	or the other component of this package:

	linux-headers-`uname -r`
	libpopt-dev
	g++

	The two last one are only required for building some test applications
	located into the "test" sub-directory.

	Default (aka "chardev") build is done with:

	$ cd peak-linux-driver-x.y.z
	$ make clean all

	This makes everything (that is, driver, libs and test programs) from
	scratch. This is the equivalent of:

	$ cd peak-linux-driver-x.y.z
	$ make -C driver clean
	$ make -C lib clean
	$ make -C test clean
	$ make -C libpcanbasic clean
	$ make -C driver all
	$ make -C lib all
	$ make -C test all
	$ make -C libpcanbasic all

	On the other hand, building the "netdev" version of the driver is done
	with:

	$ cd peak-linux-driver-x.y.z
	$ make netdev

	This is the equivalent of:

	$ cd peak-linux-driver-x.y.z
	$ make -C driver netdev
	$ make -C lib
	$ make -C test
	$ make -C libpcanbasic

	In the same vein, building the RTDM version of the driver, libs and test
	programs for Xenomai 3.x is done with:

	$ cd peak-linux-driver-x.y.z
	$ make xeno

	while building their RTDM version for RTAI 5.x is done with:

	$ cd peak-linux-driver-x.y.z
	$ make rtai

INSTALL

	$ cd peak-linux-driver-x.y.z
	$ sudo make install

	- or -

	# make install

	if the user has root privileges. This is the equivalent of:

	$ cd peak-linux-driver-x.y.z
	$ [sudo] make -C driver install
	$ [sudo] make -C lib install
	$ [sudo] make -C test install
	$ [sudo] make -C libpcanbasic install

UNINSTALL

	$ cd peak-linux-driver-x.y.z
	$ sudo make uninstall

	- or -

	# make uninstall

	if the user has root privileges. This is the equivalent of:

	$ cd peak-linux-driver-x.y.z
	$ [sudo] make -C driver uninstall
	$ [sudo] make -C lib uninstall
	$ [sudo] make -C test uninstall
	$ [sudo] make -C libpcanbasic uninstall
