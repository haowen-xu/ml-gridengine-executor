//
// Created by 许昊文 on 2018/11/19.
//

#ifndef ML_GRIDENGINE_EXECUTOR_CALLBACKAPI_H
#define ML_GRIDENGINE_EXECUTOR_CALLBACKAPI_H

#include <string>
#include <Poco/Exception.h>
#include "macros.h"

namespace Poco {
  namespace JSON {
    class Object;
  }
}

POCO_DECLARE_EXCEPTION(MLGridEngineExecutor_API, HTTPError, Poco::ApplicationException)

/**
 * Class for posting to the callback API.
 */
class CallbackAPI {
  DEFINE_NON_PRIMITIVE_PROPERTY(std::string, uri);
  DEFINE_NON_PRIMITIVE_PROPERTY(std::string, token);

private:
  long _timeout;

public:
  /**
   * Construct a new {@class CallbackAPI} instance.
   *
   * @param uri The URI of the callback API.
   * @param token The auth token of the callback API.
   * @param timeout Milliseconds before the request is timeout.
   */
  explicit CallbackAPI(std::string const& uri, std::string const& token=std::string(), long timeout=60 * 1000);

  ~CallbackAPI();

  /**
   * Post to the callback API.
   * @param doc The JSON object to be posted.
   * @throw Poco::IllegalStateException If uri is empty.
   */
  void post(Poco::JSON::Object const& doc);

  /**
   * Post to the callback API.
   * @param doc The serialized JSON object to be posted.
   * @throw Poco::IllegalStateException If uri is empty.
   */
  void post(std::string const& doc);
};


#endif //ML_GRIDENGINE_EXECUTOR_CALLBACKAPI_H
