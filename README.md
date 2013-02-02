zwave-redis-bridge
==================

Bridge zwave data to redis' keystore and events and commands to redis' pub/sub channels


The open-zwave library is written in c++ and provides access to a USB zwave controller.  This library bridges
that acces over to an instance of a redis database.  Once the bridge is established you can interface with
your zwave network using any redis client.  For my purposes I wanted to present the state of my zwave
network via a sinatra web app written in Ruby.  This was trivial given this bridge.

This software is depends on the open-zwave library located at: http://code.google.com/p/open-zwave/ and it's based off of the simple example provided in that code.

In order to interface with Redis, I used the redis-cplusplus-client.  I've forked a version of that project in
order to get pub/sub implementation but leaving out some other modifications making it easier to build on a
raspberry-pi.

My fork is here: http://github.com/carlism/redis-cplusplus-client

The redis client depends on the boost libraries.  You'll need to get them too.

Once you've got these two code bases checked out you'll need to edit the make file here to make sure thier
relative paths are correct and this should build.
