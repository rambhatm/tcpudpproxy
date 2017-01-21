#ifndef PROXY_H
#define PROXY_H

#include <string>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
/*
    The proxy class. The proxy can be tcp, udp or both. The parameters needed to create a new proxy
    are shown in the constructor class.

    methods:

    proxy::start() - Starts an initialized proxy. Will spawn a thread for the proxy and perform
    async IO on the local port specified for TCP/UDP traffic. This call doesnt block.

    proxy::shutdown() - Stops the proxy. This call blocks till all IO has stopped and proxy thread is deinited.

*/
class proxy {
public:
	typedef unsigned short port_type;
	typedef std::string host_type;

	proxy(port_type local_port, host_type remote_host, port_type remote_port, std::string protocol);
	void start();
	void shutdown();
	~proxy();

private:
	/*
		The next 5 values are config values stored for the proxy that are read from the ptree
	*/
	port_type       local_port;
	port_type       remote_port;
	host_type       remote_host;
	bool			tcp_enabled;
	bool			udp_enabled;

	boost::asio::io_service ios;		//Boost ASIO io interface

	void*			tcp_proxy;	//Points to the tcp proxy if connection is tcp or both
	void*			udp_proxy;	//points to the udp proxy if connection is udp or both

	/*
		Each proxy object spawns a thread that handles both TCP and UDP traffic
		asyncronously. The calling thread can continue executing after calling start()
	*/
	boost::thread*	proxy_thread;
	void			thread_func();
};
#endif
