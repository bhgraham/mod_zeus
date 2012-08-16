# mod_zeus is used to correct the IP addresses in Apache log files that are being recorded as traffic from Rackspace Cloud and other load balancers.



install-mod_zeus.sh 
===================

	Author - Benjamin H. Graham
	Email - bhgraham1@gmail.com
	Copyright - 2012 Administr8 (http://administr8.me/)

Install: bash <(GET a8.lc/modzeus) --install (install locally)

Usage: bash <(GET a8.lc/modzeus) -q (quiet mode)

Usage: bash <(GET a8.lc/modzeus)

VERSION="1.0.0 - 08/15/2012";

Changelog:

1.0.0 - Created modzeus for debian and redhat

TODO:

Add apache1 support as well



mod_zeus
========

License: BSD

Apache module for mod_zeus is an Apache module that can be used to translate the IP that is returned in the X-Forwarded-For header so your log files can correctly display the source IP that the request came from instead of just the Load Balancer IP.


Use this to fix and use the apache logging on a server behind a Rackspace cloud load balancer.


    Author:       Owen Garrett
    Released:  21/03/2011 - 9:10am
    License:     BSD

 

When Zeus load-balances a connection to an Apache server or Apache-based application, the connection appears to originate from the Zeus machine. This can be a problem if the server wishes to perform access control based on the client's IP address, or if it wants to log the true source address of the request.

<img src="http://community.riverbed.com/t5/image/serverpage/image-id/352i25E6049C68F0C85D/image-size/original?v=mpbl-1&px=-1">
 
Zeus' module for Apache detects the 'X-Cluster-Client-Ip' header and updates Apache's calculation of the source IP address to the correct value

Zeus provide a kernel module that may be used to spoof the source IP address of server-side connections so that they appear to originate from the remote client.  As a simpler alternative, this extension consists of an Apache module that works round this issue.
 

How does it work?
By default, Zeus inserts a special X-Cluster-Client-Ip header into each request to identify the true source address of the request. Zeus' Apache module inspects this header and corrects Apache's calculation of the source address. This change is transparent to Apache, and any applications running on or behind Apache.

 
Installation

You will need to compile the Apache module; this is most easily done using the Apache Extension toolset.  You will probably need to run the installation step as root:

#  apxs2 -i -a -c -n 'zeus' apache-2.x/mod_zeus.c

The installation step should copy the mod_zeus.so module, and add the following to your httpd.conf file:

#  apxs2 -i -a -c -n 'zeus' apache-2.x/mod_zeus.c

 
I got an error...
The apxs toolset does not always work with the apache distribution it is bundled with.  The most common problem is that the distro-supplied httpd.conf file is empty, and does not contain any LoadModule directives:
    apxs:Error:Activation failed for custom /etc/apache2/httpd.conf file..
    apxs:Error:At least one `LoadModule' directive already has to exist..

Edit the httpd.conf file and add the following two dummy lines (first line is blank):

    #LoadModule foo mod_fo.so

... then re-run the installer.

If you use a ClearModuleList directive in your Apache httpd.conf file, you will also need to add the hooks for Zeus back in along with the other modules. This can be achieved for the Zeus module by adding the following line:

    AddModule mod_zeus.c

Configuring the module

Add the following two lines to your httpd.conf file:

    ZeusEnable on
    ZeusLoadBalancerIp10.100.1.6810.100.1.69

 The ZeusLoadBalancerIp configuration directive specifies the back-end addresses of the ZXTM machines. The Apache module will only trust the X-Cluster-Client-Ip header in connections which originate from these IP addresses. This means that remote users cannot spoof their source addresses by inserting a false header and accessing the Apache servers directly.

Restart your Apache servers, and monitor your servers' error logs. If you have misconfigured the ZeusLoadBalancerIp value, you will see messages like:

    Ignoring X-Cluster-Client-Ip'204.17.28.130'from non-LoadBalancer machine '10.100.1.31'


The Result
Apache, and applications running on Apache will see the correct source IP address for each request. The access log module will log the correct address when you use %a or %h in your log format string.

The Apache module will add an environment variable named ZEUS_LOAD_BALANCER_IP, which you can inspect in your application or log using %{ZEUS_LOAD_BALANCER_IP}e. This variable identifies the back-end IP address of the Zeus machine that submitted the request.

Download this extension below:
http://community.riverbed.com/rvrb/attachments/rvrb/Extensions/5/1/modzeus-10-1.zip
http://www.zeus.com/sites/default/files/extensions/4/files/modzeus-10-1.zip