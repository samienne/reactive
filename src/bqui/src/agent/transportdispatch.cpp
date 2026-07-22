#include "bqui/agent/transport.h"

#include "tcptransport.h"

#ifdef _WIN32
#   include "pipetransport.h"
#else
#   include "sockettransport.h"
#endif

#include <memory>
#include <string>

namespace bqui::agent
{

// The endpoint string's shape selects the transport. A `tcp://host:port`,
// `host:port`, or `:port` endpoint is TCP; anything else is the platform's
// local IPC (a named pipe on Windows, a Unix-domain socket elsewhere), which
// keeps every existing endpoint working unchanged.

std::unique_ptr<Transport> connect(std::string const& endpoint)
{
    if (auto address = parseTcpEndpoint(endpoint))
        return tcpConnect(*address);

#ifdef _WIN32
    return pipeConnect(endpoint);
#else
    return unixConnect(endpoint);
#endif
}

std::unique_ptr<TransportListener> listen(std::string const& endpoint)
{
    if (auto address = parseTcpEndpoint(endpoint))
        return tcpListen(*address);

#ifdef _WIN32
    return pipeListen(endpoint);
#else
    return unixListen(endpoint);
#endif
}

} // namespace bqui::agent
