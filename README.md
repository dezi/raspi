raspian usefull stuff.
==============================================

Do github add authentification for rw access:
---------------------------------------------
ssh-keygen -t rsa -C dezi@kappa-mm.de
cat /home/pi/.ssh/id_rsa.pub

raspbmc usefull stuff.
==============================================

Add raspi-config to apt-get:
----------------------------
sudo nano /etc/apt/sources.list
deb http://mirrordirector.raspbian.org/raspbian/ wheezy main contrib non-free rpi
sudo nano /etc/apt/sources.list.d/raspi.list
deb http://archive.raspberrypi.org/debian/ wheezy main

Change keyboard layout to german:
---------------------------------
insserv /etc/init.d/mountkernfs.sh -d
sudo dpkg-reconfigure locales
sudo dpkg-reconfigure console-setup
sudo dpkg-reconfigure keyboard-configuration

sudo service keyboard-setup start
sudo vi /etc/default/keyboard
XKBLAYOUT="de"


http://raspberrypi.stackexchange.com/questions/797/how-can-i-use-a-bluetooth-mouse-and-keyboard/938#938
