#include "http_session.h"

namespace sylar
{
namespace http
{

HttpSession::HttpSession(Socket::ptr sock, bool owner)
	: SocketStream(sock, owner)
{
	
}

HttpRequest::ptr HttpSession::recvRequest()
{
	HttpRequestParser::ptr parser(new HttpRequestParser);	
	uint64_t buff_size = HttpRequestParser::GetHttpRequestBufferSize();
	
	std::shared_ptr<char> buff(new char[buff_size], [](char *ptr){ delete[] ptr; });

	char *data = buff.get();
	int offset = 0;
	do {
			int len = read(data + offset, buff_size - offset);
			if(len <= 0){
					return nullptr;
					close();
				}
			len += offset;
			size_t nparser = parser->execute(data, len);
			if(parser->hasError()){
					return nullptr;
					close();
				}
			offset = len - nparser;
			if(offset == (int)buff_size){
					return nullptr;
					close();
				}
			if(parser->isFinished())
				break;
		}
	while(true);
	
	int64_t length = parser->getContentLength();
	if(length > 0)
		{
			std::string body;
			body.resize(length);

			int len = 0;
			if(length >= offset)
				{
					memcpy(&body[0], data, offset);
					len = offset;
				}
			else
				{
					memcpy(&body[0], data, length);
					len = length;	
				}
			
			length -= offset;
			if(length > 0 && readFixSize(&body[len], length) <= 0){
					return nullptr;
					close();
				}
			parser->getData()->setBody(body);
		}
	return parser->getData();
}

int HttpSession::sendResponse(HttpResponse::ptr rsp)
{
	std::stringstream ss;
	ss << *rsp;
	std::string data = ss.str();
	return writeFixSize(data.c_str(), data.size());
}

}
}
