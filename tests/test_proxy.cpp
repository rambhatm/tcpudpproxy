#include "proxy.h"
#include <iostream>
#include <string>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>

int main(int argc, char* argv[])
{
	if (argc != 5) {
		std::cerr << "usage: test_proxy <local host ip> <local port> <forward host ip> <forward port>" << std::endl;
		return 1;
	}
	const unsigned short local_port = static_cast<unsigned short>(::atoi(argv[2]));
	const unsigned short forward_port = static_cast<unsigned short>(::atoi(argv[4]));
	const std::string local_host = argv[1];
	const std::string forward_host = argv[3];

	try {
        proxy* parr[5];
        proxy p(local_port, forward_host, forward_port, "both");
        proxy p2(local_port+1, forward_host, forward_port+1, "udp");
		for (int i = 0; i < 5; i++) {
			parr[i] = new proxy(local_port + i , forward_host, forward_port, "tcp");
			parr[i]->start();
		}

		p.start();
		p2.start();

		boost::this_thread::sleep_for(boost::chrono::seconds(25));

		for (int i = 0; i < 5; i++) {

			parr[i]->shutdown();
			delete parr[i];
		}
		p.shutdown();
        p2.shutdown();
	} catch (std::exception &e) {
		std::cout << "test expection : " << e.what() << std::endl;
	}
	return 0;
}
