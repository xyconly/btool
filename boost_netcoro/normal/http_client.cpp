#include <string>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include "http_client.h"

using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace http = boost::beast::http;    // from <boost/beast/http.hpp>

HttpClient::HttpClient()
{
}

HttpClient::~HttpClient()
{
}

bool HttpClient::setLink(const std::string& host, const std::string& port, const std::string& target)
{
	m_host = host;
	m_port = port;
	m_target = target;

	try {
		tcp::resolver resolver{ m_ios };
		tcp::socket socket{ m_ios };

		// Look up the domain name
		auto const lookup = resolver.resolve({ m_host, m_port });
		// Make the connection on the IP address we get from a lookup
		boost::asio::connect(socket, lookup);
	}
	catch (std::exception const&)
	{
		return false;
	}

	return true;
}

bool HttpClient::post_sync(std::string& read_msg, const std::string& send_msg)
{
	read_msg.clear();
	try
	{
		// These objects perform our I/O
		tcp::resolver resolver{ m_ios };
		tcp::socket socket{ m_ios };

		// Look up the domain name
		auto const lookup = resolver.resolve({ m_host, m_port });

		// Make the connection on the IP address we get from a lookup
		boost::asio::connect(socket, lookup);

		// Set up an HTTP GET request message
		if (m_target.empty())
			m_target = "/";

		http::request<http::string_body> req{ http::verb::post, m_target, 11 };
		req.set(http::field::host, m_host);
		req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

		req.body() = send_msg;
		req.content_length(req.body().size());

// 		LOG4CXX_INFO(logger, "HttpClient发送消息" LOG_NOVAR(req.body()));

		// Send the HTTP request to the remote host
		http::write(socket, req);

		// This buffer is used for reading and must be persisted
		boost::beast::flat_buffer buffer;

		// Declare a container to hold the response
		http::response<http::dynamic_body> res;

		// Receive the HTTP response
		http::read(socket, buffer, res);

		// Write the message to standard out
		std::ostringstream myos;
		myos << res << std::endl;
		read_msg = ReadResponse(myos.str());
// 		LOG4CXX_INFO(logger, "HttpClient返回消息" LOG_NOVAR(read_msg));

		// Gracefully close the socket
		boost::system::error_code ec;
		socket.shutdown(tcp::socket::shutdown_both, ec);

		// not_connected happens sometimes
		// so don't bother reporting it.
		//
		if (ec && ec != boost::system::errc::not_connected)
			throw boost::system::system_error{ ec };

		// If we get here then the connection is closed gracefully
	}
	catch (std::exception const& e)
	{
		read_msg = e.what();
		return false;
	}
	return true;
}

bool HttpClient::post_sync(std::string& read_msg, const std::string& send_msg, const std::string& target)
{
    std::string tmp_target(m_target);
    m_target = target;
    bool rslt = post_sync(read_msg, send_msg);
    m_target = tmp_target;

    return rslt;
}

bool HttpClient::get_sync(std::string& read_msg)
{
	read_msg.clear();
	try
	{
		// These objects perform our I/O
		tcp::resolver resolver{ m_ios };
		tcp::socket socket{ m_ios };

		// Look up the domain name
		auto const lookup = resolver.resolve({ m_host, m_port });

		// Make the connection on the IP address we get from a lookup
		boost::asio::connect(socket, lookup);

		// Set up an HTTP GET request message
		if (m_target.empty())
			m_target = "/";

		http::request<http::string_body> req{ http::verb::get, m_target, 11 };
		req.set(http::field::host, m_host);
		req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

		// Send the HTTP request to the remote host
		http::write(socket, req);

		// This buffer is used for reading and must be persisted
		boost::beast::flat_buffer buffer;

		// Declare a container to hold the response
		http::response<http::dynamic_body> res;

		// Receive the HTTP response
		http::read(socket, buffer, res);

		// Write the message to standard out
// 		std::ostringstream myos;
// 		myos << res << std::endl;
// 		read_msg = ReadResponse(myos.str());

        read_msg = boost::beast::buffers_to_string(res.body().data());

        // Gracefully close the socket
		boost::system::error_code ec;
		socket.shutdown(tcp::socket::shutdown_both, ec);

		// not_connected happens sometimes
		// so don't bother reporting it.
		//
		if (ec && ec != boost::system::errc::not_connected)
			throw boost::system::system_error{ ec };

		// If we get here then the connection is closed gracefully
	}
	catch (std::exception const& e)
	{
		read_msg = e.what();
		return false;
	}
	return true;
}

void HttpClient::stop()
{
	m_ios.stop();
}

std::string HttpClient::StripWhiteSpace(const std::string& s)
{
	if (s.size() == 0) return s;

	const static char* ws = " \t\v\n\r";
	size_t start = s.find_first_not_of(ws);
	size_t end = s.find_last_not_of(ws);
	if (start > s.size()) return std::string();
	return s.substr(start, end - start + 1);
}

std::vector<std::string> HttpClient::SplitString(const std::string& str, char c)
{
	std::vector<std::string> sv;
	std::string::size_type ilast = 0;
	std::string::size_type i = ilast;

	while ((i = str.find(c, i)) < str.size())
	{
		sv.push_back(str.substr(ilast, i - ilast));
		ilast = ++i;
	}
	sv.push_back(str.substr(ilast));

	return sv;
}

std::vector<std::string> HttpClient::SplitAndStripString(const std::string& str, char c)
{
	std::vector<std::string> sv;
	std::string::size_type ilast = 0;
	std::string::size_type i = ilast;

	while ((i = str.find(c, i)) < str.size())
	{
		sv.push_back(StripWhiteSpace(str.substr(ilast, i - ilast)));
		ilast = ++i;
	}
	sv.push_back(str.substr(ilast));

	return sv;
}

std::string HttpClient::ReadResponse(const std::string& input)
{
	std::vector<std::string> sv = SplitAndStripString(input, '\n');

	std::vector<std::string> FirstLine = SplitAndStripString(sv[0], ' ');
	size_t i = 1;
	std::map<std::string, std::string> fields;
	while (i < sv.size() && sv[i] != "")
	{
		std::vector<std::string> field = SplitAndStripString(sv[i], ':');
		fields[field[0]] = field[1];
		++i;
	}

// 	++i;
// 	++i;
	if (i < sv.size())
		return sv[i];

	return std::string();
}
