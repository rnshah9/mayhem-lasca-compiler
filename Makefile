PANDOC = pandoc
IFORMAT = markdown
FLAGS = --standalone --toc --highlight-style pygments

CC = gcc
LLC = llc-3.5

OPTS = -no-user-package-db -package-db .cabal-sandbox/*-packages.conf.d

all: medium

medium.nl: newlang
	stack exec gencode 10000 > medium.nl

medium.ll: medium.nl
	time stack exec nemish -- medium.nl +RTS -ssterr > medium.ll

medium.s: medium.ll
	time $(LLC) -O2 medium.ll

medium: medium.s
	time $(CC) -fPIC medium.s -o medium

newlang:
	stack build

compile_example: %.s
	$(CC) -fPIC *.s -o compiled


.PHONY: clean

clean:
	rm -f compiled *.ll *.js medium*