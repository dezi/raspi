##########################################################################
#
# Dezi's Raspberry Pi mock ups.
#

nix:
	@echo "Please give target..."

all: mockup update daemons

daemons: proftpd

##########################################################################
#
# Mock up bash + vi
#

~/.bashrc.orig:
	touch ~/.bashrc
	cp ~/.bashrc ~/.bashrc.orig
	sed -i "s/#alias ll='ls -l'/alias ll='ls -Al'/" ~/.bashrc

mockup.bash: ~/.bashrc.orig

~/.vimrc.orig:
	touch ~/.vimrc
	cp ~/.vimrc ~/.vimrc.orig
	echo "set tabstop=4" >> ~/.vimrc

mockup.vi: ~/.vimrc.orig

mockup:	mockup.bash mockup.vi

##########################################################################
#
# Update and upgrade system
#

update:
	sudo apt-get update -y
	sudo apt-get upgrade -y

##########################################################################
#
# ProFTPD
#

/etc/proftpd/proftpd.conf:
	sudo apt-get install -y proftpd

proftpd: /etc/proftpd/proftpd.conf
	
