
Installing the bfs PI Device

1) Installing the PI operating system.

The easiest way to do the imaging is to use the install the basic R-PI
imager programmer, which is available among most of the normal distributions.
The package is the rpi-imager, and you install

	% brew install install rpi-imager

2) Installing the operating system

Next, you want to install the operating system.  This is fairly straightforward
process, where you simply select the operating system and media and it will
create the new interface for you.

	OS: Raspberry PI OS (32-bit)
	Storage: something like generic, 64GB...

3) Boot and login - by default, already at PI

	Default login: pi raspberry

Note: there is a 3 minute "setup" phase that is part of the distro.  Nothing
much of note rather than the obvious.

4) Setup ssh (prepare to go headless), disable auto-login

	a) Enable the sshd process.  

	Preferences -> Raspberry PI Configuration -> Interfaces -> ssh (enable)

	b) Disable the login

	Preferences -> Raspberry PI Configuration -> System -> auto-login (disable)

5) Create the new users, give sudo access

	a) Create user "sudo adduser bfs"
	b) Add bfs as suder "sudo addusers bfs sudo"

6) Fix a static IP address (note that you have to figure this out for your
network)

	a) Add the following information to the /etc/dhcpcd.conf

	interface eth0
	static ip_address=192.168.0.4/24    
	static routers=193.168.0.254
	static domain_name_servers=192.168.0.254 8.8.8.8 

Notes: https://www.raspberrypi.org/documentation/configuration/tcpip/

7) Reboot, login as "bfs"

	a) "sudo reboot" or user interface to reboot
	b) login as the bfs user

8) Update everything (install sometimes is out of date)

	a) "sudo apt-get update"
	b) "sudo apt-get upgrade outdated"

9) Install/mount the external filesystem (ext4)

	c) % sudo mkdir /mnt/bfs-external
	b) % sudo blkid
		- this will tell you the device to mount
		- record the UUID (see below)
	d) Test the mount, 
		% sudo mount /dev/sda /mnt/bfs-external
		% ls -lt /mnt/bfs-external
		% sudo umount /mnt/bfs-external
	e) Add the mount to the boot process, edit fstab, add the line
		UUID=XXX /mnt/bfs-external	ext4    defaults 0       1
	f) Reboot, check to see the mount worked.

10) Create a .ssh key

	% ssh-keygen -t rsa -b 4096

11) Edit the user startup file, add BFS_HOME

	% vim ~/.bashrc
	(add)
	export BFS_HOME=~/bfs

** IF NOT MASTER DEVICE **

	% ssh-copy-id bfs@192.168.1.205

NOTE: the IP should be the master device ID


** IF MASTER DEVICE **

10) Copy the SSH to all of the client devices

	% ssh-copy-id bfs@192.168.1.206
	...
	% ssh-copy-id bfs@192.168.1.207
	...
	% ssh-copy-id bfs@192.168.1.208
	...

11) Connecting to VPN (as needed)

	a) install the vpn software
	% sudo apt-get install openconnect openvpn

	b) connect to the VPN service
	% sudo openconnect -u pdm12 -b vpn.cse.psu.edu/SIISADMIN

12) Login to gitlab, add SSH key to the account

	https://gitlab-siis.cse.psu.edu/

	Profile->SSH Keys

13) Make the distribution directory, clone the bfs git repository

	% cd ~
	% mkdir -p bfs
	% clone git@gitlab-siis.cse.psu.edu:secure_storage/bfs.git

12) Get the necessary libraries 

libgcrypt-dev
libffi-dev
libfuse3-3 
libfuse3-dev
\
Other notes:

A) Formatting new external drives for ext4 (this is done for you already)

	+ Run a command to see the drive information
 		% sudo lsblk -o UUID,NAME,FSTYPE,SIZE,MOUNTPOINT,LABEL,MODEL
	+ Repartition the drive
		% sudo fdisk /sda
		d - delete the existing partition
		n - create new partition, accept defaults (primary, max
size)
		w - write the new partition
	
	+ Figure out the drive information and format the drive
		% sudo mkfs -t ext4 /dev/sda1

