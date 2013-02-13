##########################################################################
#
# Dezi's linux mock ups.
#

nix:
	@echo "Please give target..."

bashvi:
	make -f Makefile.bashvi

raspberrypi:
	make -f Makefile.raspberrypi

centos:
	make -f Makefile.centos

local:
	make -f Makefile.local

dezi:
	make -f Makefile.dezi

spon:
	make -f Makefile.spon
