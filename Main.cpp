//-----------------------------------------------------------------------------
//
//	Main.cpp
//
//	OpenZWave to redis bridge.
//
//	Creates an interface in an instance of Redis for:
//  * reading data from a zwave network
//  * receiving zwave network events
//  * sending signals to the zwave network to control devices
//
//	Copyright (c) 2012 Carl Leiby <carl.leiby@gmail.com>
//
//
//	SOFTWARE NOTICE AND LICENSE
//
//	OpenZWave is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Lesser General Public License as published
//	by the Free Software Foundation, either version 3 of the License,
//	or (at your option) any later version.
//
//	OpenZWave is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU Lesser General Public License for more details.
//
//	You should have received a copy of the GNU Lesser General Public License
//	along with OpenZWave.  If not, see <http://www.gnu.org/licenses/>.
//
//-----------------------------------------------------------------------------

#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include "Options.h"
#include "Manager.h"
#include "Driver.h"
#include "Node.h"
#include "Group.h"
#include "Notification.h"
#include "ValueStore.h"
#include "Value.h"
#include "ValueID.h"
#include "ValueBool.h"
#include "Log.h"
#include "Defs.h"
#include "redisclient.h"
#include <boost/assign/list_of.hpp>

using namespace OpenZWave;

static uint32 g_homeId = 0;
static bool   g_initFailed = false;

static list<ValueID>	poll_values;

static pthread_mutex_t g_criticalSection;
static pthread_cond_t  initCond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t initMutex = PTHREAD_MUTEX_INITIALIZER;

string idString(uint8 id) {
	std::stringstream stream;
	stream << std::setfill ('0') << std::setw(2) << std::hex << (int)id;
	return stream.str();
}

string idString(uint16 id) {
	std::stringstream stream;
	stream << std::setfill ('0') << std::setw(4) << std::hex << (int)id;
	return stream.str();
}

string idString(uint32 id) {
	std::stringstream stream;
	stream << std::setfill ('0') << std::setw(8) << std::hex << id;
	return stream.str();
}

string idString(int32 id) {
	std::stringstream stream;
	stream << std::setfill ('0') << std::setw(8) << std::hex << id;
	return stream.str();
}

string idString(uint64 id) {
	std::stringstream stream;
	stream << std::setfill ('0') << std::setw(16) << std::hex << id;
	return stream.str();
}

string nodeString(Notification const* _notification) {
  string key = "";
	key += idString(_notification->GetHomeId());
	key += ":";
	key += idString(_notification->GetNodeId());
	return key;
}

string homeKey(Notification const* _notification) {
	string key = "zw_home:";
	key += idString(_notification->GetHomeId());
	return key;
}

string nodeKey(Notification const* _notification) {
	string key = "zw_node:";
	key += nodeString(_notification);
	return key;
}

string valueKey(Notification const* _notification) {
	string key = "zw_value:";
	key += nodeString(_notification);
	key += ":";
	key += idString(_notification->GetValueID().GetId());
	return key;
}

uint32 parseMessageHomeId(string message) {
	uint32 result;//number which will contain the result
	stringstream convert(message); // stringstream used for the conversion initialized with the contents of Text
    return convert >> std::hex >> result ? result : 0;
}

uint8 parseMessageField(string message, int field) {
	int result;//number which will contain the result
    stringstream convert;
    for ( string::const_iterator i=message.begin(); i!=message.end(); ++i ) {
        if ( *i==':' ) {
		field--;
    	} else if ( field == 0 ) {
		convert << *i;
        } else if ( field < 0 ) {
	 	break;
	}
    }
    return convert >> std::hex >> result ? result : 0;
}

uint8 parseMessageNodeId(string message) {
	return parseMessageField(message, 1);
}

uint8 parseMessageLevel(string message) {
	return parseMessageField(message, 2);
}


string parseMessageString(string message, int field) {
    stringstream convert;
    for ( string::const_iterator i=message.begin(); i!=message.end(); ++i ) {
        if ( *i==':' ) {
			field--;
    	} else if ( field == 0 ) {
			convert << *i;
        } else if ( field < 0 ) {
	 		break;
		}
    }
    return convert.str();
}

//-----------------------------------------------------------------------------
// <OnNotification>
// Callback that is triggered when a value, group or node changes
//-----------------------------------------------------------------------------
void OnNotification
(
	Notification const* _notification,
	void* _context
)
{
	// Must do this inside a critical section to avoid conflicts with the main thread
	pthread_mutex_lock( &g_criticalSection );
	redis::client & c = *(redis::client *)_context;

	switch( _notification->GetType() )
	{
		case Notification::Type_ValueAdded:
		{
			string key = valueKey(_notification);
			c.hset( key.c_str(), "label", Manager::Get()->GetValueLabel(_notification->GetValueID()));
			c.hset( key.c_str(), "units", Manager::Get()->GetValueUnits(_notification->GetValueID()));
			c.hset( key.c_str(), "help", Manager::Get()->GetValueHelp(_notification->GetValueID()));
			c.hset( key.c_str(), "min", idString(Manager::Get()->GetValueMin(_notification->GetValueID())));
			c.hset( key.c_str(), "max", idString(Manager::Get()->GetValueMax(_notification->GetValueID())));
			c.hset( key.c_str(), "genre", Value::GetGenreNameFromEnum(_notification->GetValueID().GetGenre()));
			c.hset( key.c_str(), "type", Value::GetTypeNameFromEnum(_notification->GetValueID().GetType()));
			c.hset( key.c_str(), "readOnly", Manager::Get()->IsValueReadOnly(_notification->GetValueID()) ? "true" : "false" );
			c.hset( key.c_str(), "writeOnly", Manager::Get()->IsValueWriteOnly(_notification->GetValueID()) ? "true" : "false" );
			c.hset( key.c_str(), "set", Manager::Get()->IsValueSet(_notification->GetValueID()) ? "true" : "false" );
			c.hset( key.c_str(), "commandClassId", idString(_notification->GetValueID().GetCommandClassId()));
			string str;
			if( Manager::Get()->GetValueAsString( _notification->GetValueID(), &str ) ) {
				c.hset( key.c_str(), "initial_value", str);
				c.publish("zw_value_add", key);
				c.hset( nodeKey(_notification), "v_"+Manager::Get()->GetValueLabel(_notification->GetValueID()), str );
			}

			if( _notification->GetValueID().GetCommandClassId() == COMMAND_CLASS_BASIC ) {
				// Add the new value to our poll list
				// check that it's not a controller
				if ( Manager::Get()->GetNodeBasic(_notification->GetHomeId(), _notification->GetNodeId()) > 0x02) {
					poll_values.push_back( _notification->GetValueID() );
				}
			}
			break;
		}

		case Notification::Type_ValueRemoved:
		{
			string key = valueKey(_notification);
			c.del( key.c_str() );
			c.publish("zw_value_delete", key);

			// Remove the value from our poll list
			for( list<ValueID>::iterator it = poll_values.begin(); it != poll_values.end(); ++it ) {
				if( (*it) == _notification->GetValueID() ) {
					poll_values.erase( it );
					break;
				}
			}
			break;
		}

		case Notification::Type_ValueChanged:
		{
			string key = valueKey(_notification);
			string str;
			if( Manager::Get()->GetValueAsString( _notification->GetValueID(), &str ) ) {
				c.hset( key.c_str(), "updated_value", str);
				c.publish("zw_value_update", key);
				c.hset( nodeKey(_notification), "v_"+Manager::Get()->GetValueLabel(_notification->GetValueID()), str );
			}
			break;
		}

		case Notification::Type_Group:
		{
			break;
		}

		case Notification::Type_NodeAdded:
		{
			string key = nodeKey(_notification);
			c.hset(key, "type", Manager::Get()->GetNodeType(_notification->GetHomeId(), _notification->GetNodeId()));
			c.hset(key, "mfgName", Manager::Get()->GetNodeManufacturerName(_notification->GetHomeId(), _notification->GetNodeId()));
			c.hset(key, "prodName", Manager::Get()->GetNodeProductName(_notification->GetHomeId(), _notification->GetNodeId()));
			c.hset(key, "nodeName", Manager::Get()->GetNodeName(_notification->GetHomeId(), _notification->GetNodeId()));
			c.hset(key, "nodeLocation", Manager::Get()->GetNodeLocation(_notification->GetHomeId(), _notification->GetNodeId()));
			c.hset(key, "nodeBasic", idString(Manager::Get()->GetNodeBasic(_notification->GetHomeId(), _notification->GetNodeId())));
			c.hset(key, "nodeGeneric", idString(Manager::Get()->GetNodeGeneric(_notification->GetHomeId(), _notification->GetNodeId())));
			c.hset(key, "mfgId", Manager::Get()->GetNodeManufacturerId(_notification->GetHomeId(), _notification->GetNodeId()));
			c.hset(key, "prodType", Manager::Get()->GetNodeProductType(_notification->GetHomeId(), _notification->GetNodeId()));
			c.hset(key, "prodId", Manager::Get()->GetNodeProductId(_notification->GetHomeId(), _notification->GetNodeId()));
			c.hset(key, "prodName", Manager::Get()->GetNodeProductName(_notification->GetHomeId(), _notification->GetNodeId()));
			c.hset(key, "isRoutingDevice", Manager::Get()->IsNodeRoutingDevice(_notification->GetHomeId(), _notification->GetNodeId()) ? "true" : "false");
			c.hset(key, "isListeningDevice", Manager::Get()->IsNodeListeningDevice(_notification->GetHomeId(), _notification->GetNodeId()) ? "true" : "false");
			c.hset(key, "isFrequentListeningDevice", Manager::Get()->IsNodeFrequentListeningDevice(_notification->GetHomeId(), _notification->GetNodeId()) ? "true" : "false");
			c.hset(key, "isBeamingDevice", Manager::Get()->IsNodeBeamingDevice(_notification->GetHomeId(), _notification->GetNodeId()) ? "true" : "false");
			c.hset(key, "isSecurityDevice", Manager::Get()->IsNodeSecurityDevice(_notification->GetHomeId(), _notification->GetNodeId()) ? "true" : "false");
			c.hset(key, "isAwake", Manager::Get()->IsNodeAwake(_notification->GetHomeId(), _notification->GetNodeId()) ? "true" : "false");
			c.hset(key, "isFailed", Manager::Get()->IsNodeFailed(_notification->GetHomeId(), _notification->GetNodeId()) ? "true" : "false");
			c.hset(key, "value", idString(_notification->GetByte()));
			c.publish("zw_node_add", key);
			break;
		}

		case Notification::Type_NodeRemoved:
		{
			string key = nodeKey(_notification);
			c.del( key );
			c.publish("zw_node_delete", key);
			// Remove the node from our list
			for( list<ValueID>::iterator it = poll_values.begin(); it != poll_values.end(); ++it ) {
				if( (*it).GetHomeId() == _notification->GetHomeId() &&
					(*it).GetNodeId() == _notification->GetNodeId() ) {
			    	poll_values.erase( it );
				}
			}
			break;
		}
		case Notification::Type_NodeEvent:
		{
			// We have received an event from the node, caused by a
			// basic_set or hail message.
			string key = nodeKey(_notification);
			c.hset(key, "value", idString(_notification->GetByte()));
			c.publish("zw_node_update", key);
			break;
		}

		case Notification::Type_PollingDisabled:
		{
			break;
		}

		case Notification::Type_PollingEnabled:
		{
			break;
		}

		case Notification::Type_DriverReady:
		{
			g_homeId = _notification->GetHomeId();
			break;
		}

		case Notification::Type_DriverFailed:
		{
			g_initFailed = true;
			pthread_cond_broadcast(&initCond);
			break;
		}

		case Notification::Type_AwakeNodesQueried:
		case Notification::Type_AllNodesQueried:
		{
			pthread_cond_broadcast(&initCond);
			break;
		}

		case Notification::Type_NodeNaming:
		{
			string key = nodeKey(_notification);
			c.hset(key, "nodeName", Manager::Get()->GetNodeName(_notification->GetHomeId(), _notification->GetNodeId()));
			c.hset(key, "nodeLocation", Manager::Get()->GetNodeLocation(_notification->GetHomeId(), _notification->GetNodeId()));
			c.publish("zw_node_named", key);
			break;
		}
		case Notification::Type_Notification:
		{
			string key = nodeKey(_notification);
			std::cout << "notification: " << key << endl;
			break;
		}
		case Notification::Type_DriverReset:
		case Notification::Type_NodeProtocolInfo:
		case Notification::Type_NodeQueriesComplete:
		default:
		{
		}
	}

	pthread_mutex_unlock( &g_criticalSection );
}

//-----------------------------------------------------------------------------
// <main>
// Create the driver and then wait
//-----------------------------------------------------------------------------
int main( int argc, char* argv[] )
{
    boost::shared_ptr<redis::client> shared_c;
    shared_c = boost::shared_ptr<redis::client>( new redis::client("localhost") );
	redis::client & c = *shared_c;

	pthread_mutexattr_t mutexattr;

	pthread_mutexattr_init ( &mutexattr );
	pthread_mutexattr_settype( &mutexattr, PTHREAD_MUTEX_RECURSIVE );
	pthread_mutex_init( &g_criticalSection, &mutexattr );
	pthread_mutexattr_destroy( &mutexattr );

	pthread_mutex_lock( &initMutex );

	// Create the OpenZWave Manager.
	// The first argument is the path to the config files (where the manufacturer_specific.xml file is located
	// The second argument is the path for saved Z-Wave network state and the log file.  If you leave it NULL
	// the log file will appear in the program's working directory.
	Options::Create( getenv ("OPEN_ZWAVE_CONFIG"), "", "" );
	Options::Get()->AddOptionInt( "SaveLogLevel", LogLevel_Error );
	Options::Get()->AddOptionInt( "QueueLogLevel", LogLevel_Error );
	Options::Get()->AddOptionInt( "DumpTrigger", LogLevel_Error );
	Options::Get()->AddOptionInt( "PollInterval", 1500 );
	Options::Get()->AddOptionBool( "IntervalBetweenPolls", true );
	Options::Get()->AddOptionBool( "ValidateValueChanges", true);
	Options::Get()->Lock();

	Manager::Create();

	// Add a callback handler to the manager.  The second argument is a context that
	// is passed to the OnNotification method.  If the OnNotification is a method of
	// a class, the context would usually be a pointer to that class object, to
	// avoid the need for the notification handler to be a static.
	Manager::Get()->AddWatcher( OnNotification, &c );

	// Add a Z-Wave Driver
	// Modify this line to set the correct serial port for your PC interface.

	string port = "/dev/ttyUSB0";
	if ( argc > 1 )
	{
		port = argv[1];
	}
	if( strcasecmp( port.c_str(), "usb" ) == 0 )
	{
		Manager::Get()->AddDriver( "HID Controller", Driver::ControllerInterface_Hid );
	}
	else
	{
		Manager::Get()->AddDriver( port );
	}
	c.set("port", port);
	cout << "port: " << c.get("port");

	// Now we just wait for either the AwakeNodesQueried or AllNodesQueried notification,
	// then write out the config file.
	// In a normal app, we would be handling notifications and building a UI for the user.
	pthread_cond_wait( &initCond, &initMutex );

	// Since the configuration file contains command class information that is only
	// known after the nodes on the network are queried, wait until all of the nodes
	// on the network have been queried (at least the "listening" ones) before
	// writing the configuration file.  (Maybe write again after sleeping nodes have
	// been queried as well.)
	if( !g_initFailed )
	{

		Manager::Get()->WriteConfig( g_homeId );

		// The section below demonstrates setting up polling for a variable.  In this simple
		// example, it has been hardwired to poll COMMAND_CLASS_BASIC on the each node that
		// supports this setting.
		pthread_mutex_lock( &g_criticalSection );

		for( list<ValueID>::iterator it2 = poll_values.begin(); it2 != poll_values.end(); ++it2 ) {
			Manager::Get()->EnablePoll( (*it2), 2 );
		}
		pthread_mutex_unlock( &g_criticalSection );

		// set up another redis connection.  A second connection is required to handle the incoming
		// subscription signals.  You can publish or set values over a connection once it's used for
		// subscribing.
		boost::shared_ptr<redis::client> shared_listen_c;
		shared_listen_c = boost::shared_ptr<redis::client>( new redis::client("localhost") );
		redis::client & listen_c = *shared_listen_c;

	 	static std::vector<std::string> channels = boost::assign::list_of("zw_set_node_name")("zw_set_node_location")
	 		("zw_control")("zw_turn_on_node")("zw_turn_off_node")("zw_set_node_level");
		struct my_subscriber : redis::client::subscriber {
			void subscribe(redis::client& client, const std::string& channel, int subscriptions) {
				std::cout << "Subscribed to #"<< channel << " (" << subscriptions << " subscriptions)" << std::endl;
			}
    			void message(redis::client& client, const std::string& channel, const std::string& msg) {
		      		std::cout << channel << " :: " << msg << std::endl;
		      		uint32 homeId = parseMessageHomeId(msg);
		      		uint8 nodeId = parseMessageNodeId(msg);
		      		if (channel == "zw_turn_on_node") {
		      			std::cout << "Turning on home:"<<idString(homeId)<<" node:"<<idString(nodeId)<<endl;
		      			Manager::Get()->SetNodeOn( homeId, nodeId );
		      		} else if (channel == "zw_turn_off_node") {
		      			std::cout << "Turning off home:"<<idString(homeId)<<" node:"<<idString(nodeId)<<endl;
		      			Manager::Get()->SetNodeOff( homeId, nodeId );
		      		} else if (channel == "zw_set_node_level") {
		      			uint8 level = parseMessageLevel(msg);
		      			std::cout << "Setting level home:"<<idString(homeId)<<" node:"<<idString(nodeId)<<" to:"<<idString(level)<<endl;
		      			Manager::Get()->SetNodeLevel( homeId, nodeId, level );
		      		} else if (channel == "zw_set_node_name") {
		      			string name = parseMessageString(msg, 2);
		      			std::cout << "Setting name home:"<<idString(homeId)<<" node:"<<idString(nodeId)<<" to:"<<name<<endl;
		      			Manager::Get()->SetNodeName( homeId, nodeId, name );
		      		} else if (channel == "zw_set_node_location") {
		      			string location = parseMessageString(msg, 2);
		      			std::cout << "Setting location home:"<<idString(homeId)<<" node:"<<idString(nodeId)<<" to:"<<location<<endl;
		      			Manager::Get()->SetNodeLocation( homeId, nodeId, location );
		      		} else if (channel == "zw_control") {
						if (msg == "exit") {
							client.unsubscribe(channels);
						}
		      		}
    			}
			void unsubscribe(redis::client& client, const std::string& channel, int subscriptions) {
		      		std::cout << "Unsubscribed from #" << channel << " (" << subscriptions << ") subscriptions" << std::endl;
  		  	}
	  	};

	  	my_subscriber subscriber;
		listen_c.subscribe(channels, subscriber);

		Driver::DriverData data;
		Manager::Get()->GetDriverStatistics( g_homeId, &data );
		printf("SOF: %d ACK Waiting: %d Read Aborts: %d Bad Checksums: %d\n", data.m_SOFCnt, data.m_ACKWaiting, data.m_readAborts, data.m_badChecksum);
		printf("Reads: %d Writes: %d CAN: %d NAK: %d ACK: %d Out of Frame: %d\n", data.m_readCnt, data.m_writeCnt, data.m_CANCnt, data.m_NAKCnt, data.m_ACKCnt, data.m_OOFCnt);
		printf("Dropped: %d Retries: %d\n", data.m_dropped, data.m_retries);
	}

	// program exit (clean up)
	if( strcasecmp( port.c_str(), "usb" ) == 0 )
	{
		Manager::Get()->RemoveDriver( "HID Controller" );
	}
	else
	{
		Manager::Get()->RemoveDriver( port );
	}
	Manager::Get()->RemoveWatcher( OnNotification, NULL );
	Manager::Destroy();
	Options::Destroy();
	pthread_mutex_destroy( &g_criticalSection );
	return 0;
}
