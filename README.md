zwave-redis-bridge
==================

Bridge zwave data to redis' keystore and events and commands to redis' pub/sub channels

The open-zwave library is written in c++ and provides access to a USB zwave controller.  This library bridges
that acces over to an instance of a redis database.  Once the bridge is established you can interface with
your zwave network using any redis client.  For my purposes I wanted to present the state of my zwave
network via a sinatra web app written in Ruby.  This was trivial given this bridge.

dependencies
------------
This software is depends on the open-zwave library located at: [the open-zwave google code site](http://code.google.com/p/open-zwave/) and it's based off of the simple example provided in that code.

```Bash
svn checkout http://open-zwave.googlecode.com/svn/trunk/ open-zwave
```

In order to interface with Redis, I used the redis-cplusplus-client.  I've forked a version of that project in
order to get pub/sub implementation but leaving out some other modifications making it easier to build on a
raspberry-pi.

The redis client depends on the boost libraries.  You'll need to get them too.
On Raspbian

```Bash
sudo apt-get install libboost-dev
sudo apt-get install libboost-thread1.49-dev
```

My fork of the redis-client is on github [here](http://github.com/carlism/redis-cplusplus-client).  I forked it to
pull in some pub/sub functionality and made sure it still build on Raspian

clone the repo locally and build the library

```Bash
git clone git://github.com/carlism/redis-cplusplus-client.git
cd redis-cplusplus-client/
make
cd ..
```

building
--------
Pull a copy of this repo

```Bash
git clone git://github.com/carlism/zwave-redis-bridge.git
```

Once you've got these two code bases checked out you'll need to edit the make file here to make sure the
relative paths are correct and this should build.

```Bash
cd zwave-redis-bridge
make
```

What does it do?
================

On start up
-----------
1. makes a connection to the local redis instance and flushes the db clean
2. sets up communications with the zwave usb controller
3. registers itself as the listener that should receive all of the zwave events
4. waits for one of the two zwave events to occur: AwakeNodesQueried or AllNodesQueried
5. dumps the known config so far out to the zw xml files
6. tells the usb controller to enable polling on all the basic values on each node
7. makes a second redis connection and establishes a set of subscriptions to channels for processing incoming commands

Command Channels Supported
--------------------------

Channel              | Messsage Format    | Function
-------              | ---------------    | --------
zw_turn_on_node      | home:node          | switches on the device on the node specified
zw_turn_off_node     | home:node          | switches off the device on the node specified
zw_set_node_level    | home:node:level    | sets the level of the node (used for dimmer switches and the like)
zw_set_node_name     | home:node:name     | sets the name value for the node
zw_set_node_location | home:node:location | sets the location value for the node

ZWave Notification Events Handled
---------------------------------

zw_home:home
zw_node:home:node
zw_value:home:node:value

Notification Events
-------------------

Channel          | Message
-------          | -------
zw_node_add      | zw_node:home:node
zw_node_delete   | zw_node:home:node
zw_node_update   | zw_node:home:node
zw_node_named    | zw_node:home:node
zw_value_add     | zw_value:home:node:value
zw_value_delete  | zw_value:home:node:value
zw_value_update  | zw_value:home:node:value

