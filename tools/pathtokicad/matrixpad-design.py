#!/usr/bin/python

from pathtokicad import *

fill_paths = [

	(FRONT_SILK,   "matrixpad/design.path"),
]

segment_paths = [
]

pads = [
]

print_module("matrixpad_design", fill_paths, segment_paths, pads)
 
