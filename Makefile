#  codesphinx: count all of those less than flattering words while coding.
#  Copyright (C) 2013 Evan Bezeredi <bezeredi.dev@gmail.com>
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#
#!
#  Test makefile for codesphinx.
#

PROG = bin/codesphinx

all:
	gcc -Wall -g -o $(PROG) src/main.c \
		-DMODELDIR=\"`pkg-config --variable=modeldir pocketsphinx`\" \
		`pkg-config --cflags --libs pocketsphinx sphinxbase`

run:
	export LD_LIBRARY_PATH=/usr/local/lib/
	$(PROG) -hmm src/lib/pocketsphinx-0.8/model/hmm/en_US/hub4wsj_sc_8k \
		-lm res/codesphinx.lm \
		-dict res/codesphinx.dic

clean:
	rm -f $(PROG)

