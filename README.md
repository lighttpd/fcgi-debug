Description
-----------

**Homepage**: http://redmine.lighttpd.net/projects/fcgi-debug/wiki

fcgi-debug helps you to trace what happens with your fastcgi programs without having to strace them;
it just sits between webserver and your fastcgi program and forwards all content while analysing it.


Usage
-----

Example usage (assuming the binary is in build/fcgi-debug, starts a fastcgi server listening on port 1026):

<pre>
spawn-fcgi -C 0 -p 1026 -n -- build/fcgi-debug /usr/bin/php5-cgi
</pre>


Build dependencies
------------------

 * [glib2 >= 2.12](https://git.gnome.org/browse/glib)
 * [libev](http://software.schmorp.de/pkg/libev.html)
 * cmake

For running you probably want [spawn-fcgi](http://redmine.lighttpd.net/projects/spawn-fcgi/wiki).


Build
-----

<pre>
git clone git://git.lighttpd.net/fcgi-debug.git
cd fcgi-debug
(mkdir build && cd build && cmake .. && make)
</pre>
