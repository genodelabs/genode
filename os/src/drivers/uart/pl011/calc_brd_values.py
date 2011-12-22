#!/usr/bin/python

from math import floor

clock = 24000000
bauds = [ 9600, 14400, 19200, 38400, 115200 ]

print "Clock   Desired   IBRD  FBRD    Real"
print " MHz   baud rate              baud rate"
for baud in bauds:
	div   = clock / 16.0 / baud
	ibrd  = int(floor(div))
	fbrd  = int(floor((div - ibrd) * 64 + 0.5))
	baud_ = clock / 16.0 / (ibrd + fbrd / 64.0)

	print "% 4d   % 8d   % 4d  % 3d  % 10.2f" % (clock / 1000000, baud, ibrd, fbrd, baud_)
