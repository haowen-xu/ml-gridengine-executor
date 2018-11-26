//
// Created by 许昊文 on 2018/11/21.
//

#ifndef ML_GRIDENGINE_EXECUTOR_PERSISTANDCALLBACKMANAGER_H
#define ML_GRIDENGINE_EXECUTOR_PERSISTANDCALLBACKMANAGER_H

#include <string>
#include <map>
#include <Poco/Exception.h>
#include "macros.h"
#include "ProgramExecutor.h"

namespace Poco {
  namespace JSON {
    class Object;
  }
}

/**
 * Class to persist program outputs and executor statuses on the disk,
 * and send these stuff to the callback API server.
 *
 * To ensure that the API server can always receive the executor events,
 * we always save the outputs and statuses on to the disk before sending
 * callback requests.  On the other side, we require the API server to
 * first open there HTTP server, then check the disk files, when it is
 * launched from stratch.  By doing this, we can ensure the API server
 * can get the missing events back when it is recovered form a previous
 * shutdown.
 */
class PersistAndCallbackManager {
  DEFINE_NON_PRIMITIVE_PROPERTY(std::string, statusFile);
  DEFINE_NON_PRIMITIVE_PROPERTY(std::string, uri);
  DEFINE_NON_PRIMITIVE_PROPERTY(std::string, token);

private:
  int _maxRetry;  // maximum number of retrials for each postEvent
  long _timeout;  // timeout for a single request, in milliseconds
  std::string _hostName;
  int _port;
  std::map<std::string, std::string> _lastPostedGeneratedFiles;

  void _postEvent(std::string const& eventType, Poco::JSON::Object const &doc);

public:
  /**
   * Whether or not this manager is enabled?
   *
   * If either `statusFile` or `uri` is configured, then this manager is enabled.
   */
  inline bool enabled() const { return !_statusFile.empty() || !_uri.empty(); }

  /**
   * Construct a new {@class CallbackAPI} instance.
   *
   * @param statusFile The status file path.
   * @param uri The URI of the callback API.
   * @param token The auth token of the callback API.
   * @param timeout Milliseconds before each request is timeout.
   */
  explicit PersistAndCallbackManager(
      std::string const& statusFile, std::string const& uri, std::string const& token=std::string(),
      long timeout=60 * 1000);

  ~PersistAndCallbackManager();

  /**
   * Wait for all background jobs to finish.
   */
  void wait();

  /**
   * Immediately interrupt all background jobs.
   */
  void interrupt();

  /**
   * Save {@arg hostName} and {@arg port} to status file, and port to callback API.
   *
   * @param hostName The hostname of the executor server.
   * @param port The port of the executor server.
   */
  void programStarted(std::string const &hostName, int port);

  /**
   * Save generated file {@arg jsonObject} with tag {@arg fileTag}.
   *
   * @param fileTag The tag of the generated file.
   * @param jsonObject The parsed document of the generated file.
   */
  void fileGenerated(std::string const& fileTag, Poco::JSON::Object const& jsonObject);

  /**
   * Save the final status of the executor.
   *
   * @param executor The program executor.
   */
  void programFinished(ProgramExecutor const& executor);
};


#endif //ML_GRIDENGINE_EXECUTOR_PERSISTANDCALLBACKMANAGER_H
