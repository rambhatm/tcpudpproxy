#include <iostream>
#include <string>

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>

#include "proxy.h"

namespace tcp_proxy
{
	namespace ip = boost::asio::ip;

	class bridge : public boost::enable_shared_from_this<bridge>
	{
	public:

		typedef ip::tcp::socket socket_type;
		typedef boost::shared_ptr<bridge> ptr_type;

		bridge(boost::asio::io_service& ios)
			: downstream_socket_(ios),
			upstream_socket_(ios)
		{}

		socket_type& downstream_socket()
		{
			return downstream_socket_;
		}

		socket_type& upstream_socket()
		{
			return upstream_socket_;
		}

		void start(const std::string& upstream_host, unsigned short upstream_port)
		{
			upstream_socket_.async_connect(
				ip::tcp::endpoint(
					boost::asio::ip::address::from_string(upstream_host),
					upstream_port),
				boost::bind(&bridge::handle_upstream_connect,
					shared_from_this(),
					boost::asio::placeholders::error));
		}

		void handle_upstream_connect(const boost::system::error_code& error)
		{
			if (!error)
			{
				upstream_socket_.async_read_some(
					boost::asio::buffer(upstream_data_, max_data_length),
					boost::bind(&bridge::handle_upstream_read,
						shared_from_this(),
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred));

				downstream_socket_.async_read_some(
					boost::asio::buffer(downstream_data_, max_data_length),
					boost::bind(&bridge::handle_downstream_read,
						shared_from_this(),
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred));
			}
			else
				close();
		}

	private:

		void handle_downstream_write(const boost::system::error_code& error)
		{
			if (!error)
			{
				upstream_socket_.async_read_some(
					boost::asio::buffer(upstream_data_, max_data_length),
					boost::bind(&bridge::handle_upstream_read,
						shared_from_this(),
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred));
			}
			else
				close();
		}

		void handle_downstream_read(const boost::system::error_code& error,
			const size_t& bytes_transferred)
		{
			if (!error)
			{
				async_write(upstream_socket_,
					boost::asio::buffer(downstream_data_, bytes_transferred),
					boost::bind(&bridge::handle_upstream_write,
						shared_from_this(),
						boost::asio::placeholders::error));
			}
			else
				close();
		}

		void handle_upstream_write(const boost::system::error_code& error)
		{
			if (!error)
			{
				downstream_socket_.async_read_some(
					boost::asio::buffer(downstream_data_, max_data_length),
					boost::bind(&bridge::handle_downstream_read,
						shared_from_this(),
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred));
			}
			else
				close();
		}

		void handle_upstream_read(const boost::system::error_code& error,
			const size_t& bytes_transferred)
		{
			if (!error)
			{
				async_write(downstream_socket_,
					boost::asio::buffer(upstream_data_, bytes_transferred),
					boost::bind(&bridge::handle_downstream_write,
						shared_from_this(),
						boost::asio::placeholders::error));
			}
			else
				close();
		}

		void close()
		{
			boost::mutex::scoped_lock lock(mutex_);

			if (downstream_socket_.is_open())
			{
				downstream_socket_.close();
			}

			if (upstream_socket_.is_open())
			{
				upstream_socket_.close();
			}
		}

		socket_type downstream_socket_;
		socket_type upstream_socket_;

		enum { max_data_length = 8192 };
		unsigned char downstream_data_[max_data_length];
		unsigned char upstream_data_[max_data_length];

		boost::mutex mutex_;

	public:

		class acceptor
		{
		public:

			acceptor(boost::asio::io_service& io_service,
				const std::string& local_host, unsigned short local_port,
				const std::string& upstream_host, unsigned short upstream_port)
				: io_service_(io_service),
				localhost_address(boost::asio::ip::address_v4::from_string(local_host)),
				acceptor_(io_service_, ip::tcp::endpoint(localhost_address, local_port)),
				upstream_port_(upstream_port),
				upstream_host_(upstream_host)
			{}

			bool accept_connections()
			{
				try
				{
					session_ = boost::shared_ptr<bridge>(new bridge(io_service_));

					acceptor_.async_accept(session_->downstream_socket(),
						boost::bind(&acceptor::handle_accept,
							this,
							boost::asio::placeholders::error));
				}
				catch (std::exception& e)
				{
					std::cerr << "TCP exception: " << e.what() << std::endl;
					return false;
				}

				return true;
			}

		private:

			void handle_accept(const boost::system::error_code& error)
			{
				if (!error)
				{
					session_->start(upstream_host_, upstream_port_);
					if (!accept_connections())
					{
						std::cerr << "TCP: Failure during call to accept." << std::endl;
					}
				}
				else
				{
					std::cerr << "TCP Exception: " << error.message() << std::endl;
				}
			}

			boost::asio::io_service& io_service_;
			ip::address_v4 localhost_address;
			ip::tcp::acceptor acceptor_;
			ptr_type session_;
			unsigned short upstream_port_;
			std::string upstream_host_;
		};

	};
}


namespace udp_proxy
{
	namespace ip = boost::asio::ip;
	using namespace boost::asio;
	using namespace std;
	const int max_data_length = 65536;

	class m_udpProxyServer
	{
	public:
		m_udpProxyServer(io_service& io, const string& localhost,
			const int& localport, const string& remotehost, const int& remoteport) :
			downstream_socket_(io, ip::udp::endpoint(ip::udp::v4(), localport)),
			upstream_socket_(io),
			_remotehost(remotehost),
			_remoteport(remoteport),
			upstream_remoteUdpEndpoint(ip::address_v4::from_string(_remotehost), _remoteport)
		{
			start_downstream_receive();
		}

		void start_downstream_receive()
		{
			downstream_socket_.async_receive_from(
				boost::asio::buffer(downstream_data_, max_data_length),
				downstream_remoteUdpEndpoint,
				boost::bind(&m_udpProxyServer::upstream_connect, this,
					boost::asio::placeholders::bytes_transferred,
					boost::asio::placeholders::error));
		}

		void upstream_connect(const size_t& bytes_transferred,
			const boost::system::error_code& error)
		{
			if (!error)
			{
				upstream_socket_.async_connect(
					upstream_remoteUdpEndpoint,
					boost::bind(&m_udpProxyServer::handle_upstream_connect,
						this, bytes_transferred, boost::asio::placeholders::error));
			}
			else
			{
				std::cerr << "UDP exception: " << error.message() << std::endl;
			}
			start_downstream_receive();
		}

		void handle_upstream_connect(const size_t& bytes_transferred,
			const boost::system::error_code& error)
		{
			upstream_socket_.async_send_to(
				boost::asio::buffer(downstream_data_, bytes_transferred),
				upstream_remoteUdpEndpoint,
				boost::bind(&m_udpProxyServer::handle_upstream_send,
					this, boost::asio::placeholders::error));
		}

		void handle_upstream_send(const boost::system::error_code& error)
		{
			if (!error)
			{
				upstream_socket_.async_receive_from(
					boost::asio::buffer(upstream_data_, max_data_length),
					upstream_remoteUdpEndpoint,
					boost::bind(&m_udpProxyServer::handle_upstream_receive,
						this,
						boost::asio::placeholders::bytes_transferred,
						boost::asio::placeholders::error));

			}
			else
			{
				std::cerr << "UDP Exception: " << error.message() << std::endl;
			}
		}

		void handle_upstream_receive(const size_t& bytes_transferred,
			const boost::system::error_code& error)
		{
			if (!error)
			{
				downstream_socket_.async_send_to(
					boost::asio::buffer(upstream_data_, bytes_transferred),
					downstream_remoteUdpEndpoint,
					boost::bind(&m_udpProxyServer::handle_downstream_send,
						this,
						boost::asio::placeholders::error));
			}
			else
			{
				std::cerr << "UDP Exception: " << error.message() << std::endl;
			}
		}

		void handle_downstream_send(const boost::system::error_code& error)
		{
			if (!error)
			{
				downstream_socket_.async_receive_from(
					boost::asio::buffer(downstream_data_, max_data_length),
					downstream_remoteUdpEndpoint,
					boost::bind(&m_udpProxyServer::handle_downstream_receive, this,
						boost::asio::placeholders::bytes_transferred,
						boost::asio::placeholders::error));
			}
			else
			{
				std::cerr << "UDP exception: " << error.message() << std::endl;
			}
		}

		void handle_downstream_receive(const size_t& bytes_transferred,
			const boost::system::error_code& error)
		{
			upstream_socket_.async_send_to(
				boost::asio::buffer(downstream_data_, bytes_transferred),
				upstream_remoteUdpEndpoint,
				boost::bind(&m_udpProxyServer::handle_upstream_send,
					this, boost::asio::placeholders::error));
		}

	private:
		ip::udp::socket downstream_socket_;
		ip::udp::socket upstream_socket_;
		string _remotehost;
		int _remoteport;
		ip::udp::endpoint downstream_remoteUdpEndpoint;
		ip::udp::endpoint upstream_remoteUdpEndpoint;
		unsigned char downstream_data_[max_data_length];
		unsigned char upstream_data_[max_data_length];
	};
}

proxy::proxy(port_type local_port, host_type remote_host, port_type remote_port, std::string protocol)
{
	const std::string local_host = "127.0.0.1";
	if (protocol == "tcp" || protocol == "both") {
		this->tcp_proxy = (void *)new tcp_proxy::bridge::acceptor(this->ios, local_host, local_port, remote_host, remote_port);
		this->tcp_enabled = true;
	}
	if (protocol == "udp" || protocol == "both") {
		this->udp_proxy = (void *)new udp_proxy::m_udpProxyServer(this->ios, local_host, local_port, remote_host, remote_port);
		this->udp_enabled = true;
	}
	this->local_port = local_port;
	this->remote_host = remote_host;
	this->remote_port = remote_port;
}

void proxy::thread_func()
{
	try {
		if (this->tcp_enabled)
			(static_cast<tcp_proxy::bridge::acceptor*>(this->tcp_proxy))->accept_connections();
		this->ios.run();
	}
	catch (std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return;
	}
}

void proxy::start()
{
	try {
		this->proxy_thread = new boost::thread(&proxy::thread_func, this);
	}
	catch (std::exception& e) {
		std::cerr << "Proxy Error: " << e.what() << std::endl;
		return;
	}
}

void proxy::shutdown()
{
	this->ios.stop();
	this->proxy_thread->join();
}

proxy::~proxy()
{
	if (this->tcp_enabled)
		delete static_cast<tcp_proxy::bridge::acceptor*>(this->tcp_proxy);
	if (this->udp_enabled)
		delete static_cast<udp_proxy::m_udpProxyServer*>(this->udp_proxy);
	delete this->proxy_thread;
}
