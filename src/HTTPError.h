//
// Created by 许昊文 on 2018/11/22.
//

#ifndef ML_GRIDENGINE_EXECUTOR_HTTPERROR_H
#define ML_GRIDENGINE_EXECUTOR_HTTPERROR_H

#include <Poco/Exception.h>


class HTTPError : public Poco::ApplicationException {
private:
  int _statusCode;

public:
  inline int statusCode() const { return _statusCode; }

  HTTPError(int statusCode, const std::string& msg);

  HTTPError(int statusCode, const std::string& msg, const Poco::Exception& exc);

  HTTPError(HTTPError const& exc);

  ~HTTPError() throw();

  HTTPError& operator=(HTTPError const& exc);

  const char* name() const throw();

  const char* className() const throw();

  Poco::Exception* clone() const;

  void rethrow() const;
};


#endif //ML_GRIDENGINE_EXECUTOR_HTTPERROR_H
