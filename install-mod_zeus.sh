#!/bin/bash
# Author - Benjamin H. Graham
# Email - bhgraham1@gmail.com
# Copyright - 2012 Administr8 (http://administr8.me/)

DESC="Apache mod_zeus installer.";
SCRIPT="modzeus";

# Install: bash <(GET a8.lc/modzeus) --install
# Usage: bash <(GET a8.lc/modzeus) -q
# Usage: bash <(GET a8.lc/modzeus) -p <loadbalancer ip address>

VERSION="1.0.2 - 08/16/2012";

# Changelog:
# 1.0.0 - Created modzeus for debian and redhat
# 1.0.1 - tested on cent and debian, fixed github url
# 1.0.2 - optional secure load balancer config

# TODO:
# Add apache1 support

# source Administr8 common functions
GET=GET; hash GET 2>/dev/null || { GET="curl -s"; }
x=$(mktemp); $GET a8.lc/a8rc > $x; source $x;

if [ -f /etc/redhat-release ]; then
    a8distro="redhat";
fi;
if [ -f /etc/debian_version ]; then
    a8distro="debian";
fi;

if [[ ${!#} != "-q" ]]; then
	a8info "Downloading current mod_zeus.c from Github";
fi;
wget -q --no-check-certificate https://raw.github.com/bhgraham/mod_zeus/master/apache-2.x/mod_zeus.c

a8info "Installing for apache2 on $a8distro";

if [[ $a8distro == "redhat" ]]; then
	if [[ ${!#} != "-q" ]]; then
		a8info "Downloading the httpd-devel package with yum."
	fi;
	yum -q -y install httpd-devel.x86_64 gcc

	if [[ ${!#} != "-q" ]]; then
		a8info "Compiling mod_zeus.c..."
	fi;
	apxs -c -i mod_zeus.c

	if [[ ${!#} != "-q" ]]; then
		a8info "Adding module configuration to Apache2."
	fi;
	echo "LoadModule zeus_module modules/mod_zeus.so" > /etc/httpd/conf.d/zeus.conf
	echo "ZeusEnable On" >> /etc/httpd/conf.d/zeus.conf

	if [[ $1 == "-p" ]]; then
		echo "ZeusLoadBalancerIP $2" >> /etc/httpd/conf.d/zeus.conf
	else
		echo "ZeusLoadBalancerIP *" >> /etc/httpd/conf.d/zeus.conf
	fi;
	if [[ ${!#} != "-q" ]]; then
		a8info "Restarting Apache (httpd)..."
	fi;
	service httpd graceful;

	if [[ ${!#} != "-q" ]]; then
		a8info "Done."
	fi;

elif [[ $a8distro == "debian" ]]; then
	if [[ ${!#} != "-q" ]]; then
		a8info "Downloading the apache2-prefork-dev package with apt-get..."
	fi;
	apt-get -y -q install apache2-prefork-dev;

	if [[ ${!#} != "-q" ]]; then
		a8info "Compiling mod_zeus.c..."
	fi;
	apxs2 -iac mod_zeus.c;

	if [[ ${!#} != "-q" ]]; then
		a8info "Adding module configuration to Apache."
	fi;
	echo "LoadModule zeus_module modules/mod_zeus.so" > /etc/apache2/mods-enabled/zeus.conf;
	echo "ZeusEnable On" >> /etc/apache2/mods-enabled/zeus.conf;
	echo "ZeusLoadBalancerIP *" >> /etc/apache2/mods-enabled/zeus.conf;
#INSERT
#	echo "ZeusLoadBalancerIP 10.100.3.23 10.100.3.24" >> /etc/apache2/mods-enabled/zeus.conf;
# these should be the ips for the load balancer

	if [[ ${!#} != "-q" ]]; then
		a8info "Restarting Apache..."
	fi;
	service apache2 graceful;

	if [[ ${!#} != "-q" ]]; then
		a8info "Done."
	fi;
else
	a8fail "No supported distribution found"
fi;

rm mod_zeus.c;