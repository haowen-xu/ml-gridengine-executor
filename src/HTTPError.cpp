//
// Created by 许昊文 on 2018/11/22.
//

#include <typeinfo>
#include "HTTPError.h"


HTTPError::HTTPError(int statusCode, std::string const& msg):
  Poco::ApplicationException(msg), _statusCode(statusCode)
{
}

HTTPError::HTTPError(int statusCode, std::string const& msg, Poco::Exception const& exc):
  Poco::ApplicationException(msg, exc), _statusCode(statusCode)
{
}

HTTPError::HTTPError(HTTPError const& exc) :
  Poco::ApplicationException(exc), _statusCode(exc.statusCode())
{
}

HTTPError::~HTTPError() throw() = default;

HTTPError& HTTPError::operator=(HTTPError const& exc)
{
  Poco::ApplicationException::operator=(exc);
  _statusCode = exc.statusCode();
  return *this;
}

const char* HTTPError::name() const throw()
{
  return "HTTP Error";
}

const char* HTTPError::className() const throw()
{
  return typeid(*this).name();
}

Poco::Exception *HTTPError::clone() const
{
  return new HTTPError(*this);
}

void HTTPError::rethrow() const
{
  throw *this;
}
