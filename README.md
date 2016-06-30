# Stan challenge answer with Apache2 and YAJL2

This is an example of Apache module with sream (SAX-style) JSON parser
using [YAJL library](https://lloyd.github.io/yajl/).

## Installation

The following packages are needed to be installed to build it on Ubuntu:
make
apache2
apache2-dev
libyajl-dev

Then just execute
```
make install
```
to have the mod_stan module compiled and installed into Apache modules directory.


## Configuration

You need to enable mod_headers using the following command:
```
sudo a2enmod headers
```

then adjust ServerName parameter in stan.conf file and copy it into
/etc/apache2/sites/available directory.

Now you can enable new web-site and restart Apache server:
```
sudo a2ensite stan
sudo service apache2 restart
```
Now the new web site is ready to answer requests. 

How to send request.json to the server
--------------------------------------
```
curl -vv -H "Content-Type: application/json" --data @request.json http://server.name/
```