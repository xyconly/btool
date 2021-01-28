#ifndef __HTTP_CLIENT_H__
#define __HTTP_CLIENT_H__
#include <string>
#include <boost/asio.hpp>

class HttpClient 
{
public:
	HttpClient();
	~HttpClient();

	bool setLink(const std::string& host, const std::string& port, const std::string& target = "/");

	// 以post形式, 发送send_msg(Body中)
	// 正确返回true, read_msg为返回数据;
	// 错误返回false, read_msg为错误内容
	bool post_sync(std::string& read_msg, const std::string& send_msg);
    bool post_sync(std::string& read_msg, const std::string& send_msg, const std::string& target);

	// 以get形式, 发送send_msg
	// 正确返回true, read_msg为返回数据;
	// 错误返回false, read_msg为错误内容
	bool get_sync(std::string& read_msg);

	void stop();

#pragma region https://github.com/boostorg/beast/issues/731#ref-issue-249622270中所写方法
protected:
	std::string ReadResponse(const std::string& input);

private:
	std::string StripWhiteSpace(const std::string& s);

	//! Split a string into a vector of substrings with \c c as a separator
	/*! White space characters will not be removed and the the strings will not be converted to lowercase letters.
	*/
	std::vector<std::string> SplitString(const std::string& str, char c);

	//! Split a string into a vector of substrings with \c c as a separator
	/*! White space characters will be removed and the the strings will be converted to lowercase letters.
	*/
	std::vector<std::string> SplitAndStripString(const std::string& str, char c);
#pragma endregion

private:
	std::string				m_host;
	std::string				m_port;
	std::string				m_target;
	boost::asio::io_service m_ios;
};

#endif
