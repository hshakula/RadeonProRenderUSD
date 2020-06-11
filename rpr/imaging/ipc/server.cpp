#include "server.h"
#include "tokens.h"
#include "debugCodes.h"

#include <process.hpp>

#include <pxr/base/tf/stringUtils.h>
#include <pxr/base/tf/staticTokens.h>
#include <pxr/base/tf/instantiateSingleton.h>
#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/thisPlugin.h>

PXR_NAMESPACE_OPEN_SCOPE

TF_INSTANTIATE_SINGLETON(zmq::context_t);

zmq::context_t& GetZmqContext() {
    return TfSingleton<zmq::context_t>::GetInstance();
}

namespace {

template <typename U>
struct is_buffer_like {
private:
    template<typename T>
    static constexpr auto check(T*) -> typename
        std::is_pointer<decltype(std::declval<T>().data())>::type;

    template<typename>
    static constexpr std::false_type check(...);

    typedef decltype(check<U>(0)) type;

public:
    static constexpr bool value = type::value;
};
template< class T >
constexpr bool is_buffer_like_v = is_buffer_like<T>::value;

template <typename T>
std::enable_if_t<is_buffer_like_v<T>, zmq::message_t>
GetZmqMessage(T const& command) {
    return {command.data(), command.size()};
}

template <typename T>
std::enable_if_t<std::is_integral_v<T>, zmq::message_t>
GetZmqMessage(T integer) {
    return GetZmqMessage(std::to_string(integer));
}

std::string GetSocketAddress(zmq::socket_t const& socket) {
    char addrBuffer[1024];
    size_t buffLen = sizeof(addrBuffer);
    socket.getsockopt(ZMQ_LAST_ENDPOINT, addrBuffer, &buffLen);
    return std::string(addrBuffer, buffLen - 1);
}

bool SocketReadyForSend(zmq::socket_t& socket) {
    zmq::pollitem_t pollItem{static_cast<void*>(socket), 0, ZMQ_POLLOUT, 0};
    return zmq::poll(&pollItem, 1, 0) == 1;
}

const char* const kInAppCommunicationSockAddr = "inproc://RprIpcServer";

} // namespace anonymous

RprIpcServer::RprIpcServer(Listener* listener)
    : m_listener(listener) {
    auto& zmqContext = GetZmqContext();

    try {
        m_clientSocket = zmq::socket_t(zmqContext, zmq::socket_type::rep);
        m_clientSocket.bind("tcp://127.0.0.1:*");

        m_appSocket = zmq::socket_t(zmqContext, zmq::socket_type::pull);
        m_appSocket.bind(kInAppCommunicationSockAddr);
    } catch (zmq::error_t& e) {
        throw std::runtime_error(TfStringPrintf("Failed to setup RprIpcServer sockets: %d", e.num()));
    }

    // It's an open question how to run or connect to viewer:
    // whether we expect viewer process to exists,
    // or we will have some daemon that will control viewer process
    // or RprIpcServer must create the process
    //
    // For now, we stick to the last option
    {
        PlugPluginPtr plugin = PLUG_THIS_PLUGIN;
        if (!plugin) {
            throw std::runtime_error("Failed to load this plugin. Check plugInfo.json");
        }
        auto viewerScriptPath = plugin->GetResourcePath() + "/viewer.py";

        std::vector<std::string> viewProcessCmd = {
            "C:/dev/USD/build_radeon/usd.cmd",
            "python", // XXX: Calling external python is not that robust, we need to come up with some robust way to spawn the viewer process
            viewerScriptPath,
            "--control", GetSocketAddress(m_clientSocket),
        };

        /*m_viewerProcess = std::make_unique<TinyProcessLib::Process>(viewProcessCmd, std::string(), [](const char *bytes, size_t n) {
            printf("Viewer output from stdout: %.*s)", int(n), bytes);
        }, [](const char *bytes, size_t n) {
            printf("Viewer output from stderr: %.*s)", int(n), bytes);
        });

        int exitStatus;
        if (m_viewerProcess->try_get_exit_status(exitStatus)) {
            throw std::runtime_error(TfStringPrintf("Failed to run viewer process, exit status: %d", exitStatus));
        }*/

        printf("Waiting for connection on socket %s", viewProcessCmd.back().c_str());
    }

    m_networkThread = std::thread([this]() { Run(); });
}

RprIpcServer::~RprIpcServer() {
    if (m_viewerProcess) {
        m_viewerProcess->kill(true);
    }

    zmq::socket_t shutdownSocket(GetZmqContext(), zmq::socket_type::push);
    shutdownSocket.connect(kInAppCommunicationSockAddr);
    shutdownSocket.send(GetZmqMessage(RprIpcTokens->shutdown));
    m_networkThread.join();
}

void RprIpcServer::GetSender(std::shared_ptr<Sender>* senderPtr) {
    // the same zmq::socket_t can be safely reused from the same thread
    // so we cache Sender per thread to minimize the number of created sockets

    auto thisThreadId = std::this_thread::get_id();

    // Check if cached sender can be reused
    if (*senderPtr) {
        auto cachedSender = senderPtr->get();
        if (cachedSender->m_threadId == thisThreadId) {
            return;
        }
    }

    // Check if we have valid cached sender for this thread
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_senders.find(thisThreadId);
        if (it != m_senders.end()) {
            if (auto sender = it->second.lock()) {
                *senderPtr = sender;
            } else {
                m_senders.erase(it);
            }
        }
    }

    // Create new sender for this thread
    struct Dummy : public Sender {};
    auto sender = std::make_shared<Dummy>();
    sender->m_threadId = thisThreadId;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_senders.emplace(thisThreadId, sender);
    }

    *senderPtr = sender;
}

RprIpcServer::Sender::Sender()
    : m_appSocket(GetZmqContext(), zmq::socket_type::push) {
    m_appSocket.connect(kInAppCommunicationSockAddr);
}

void RprIpcServer::Sender::SendLayer(SdfPath const& layerPath, std::string layer) {
    try {
        m_appSocket.send(GetZmqMessage(RprIpcTokens->layer), zmq::send_flags::sndmore);
        m_appSocket.send(GetZmqMessage(layerPath.GetString()), zmq::send_flags::sndmore);
        m_appSocket.send(zmq::message_t(layer.c_str(), layer.size()));
    } catch (zmq::error_t& e) {
        TF_RUNTIME_ERROR("Error on layer send: %d", e.num());
    }
}

void RprIpcServer::Sender::RemoveLayer(SdfPath const& layerPath) {
    try {
        m_appSocket.send(GetZmqMessage(RprIpcTokens->layerRemove), zmq::send_flags::sndmore);
        m_appSocket.send(GetZmqMessage(layerPath.GetString()));
    } catch (zmq::error_t& e) {
        TF_RUNTIME_ERROR("Error on layer remove: %d", e.num());
    }
}

void RprIpcServer::Run() {
    std::vector<zmq::pollitem_t> pollItems = {
        {m_clientSocket, 0, ZMQ_POLLIN, 0},
        {m_appSocket, 0, ZMQ_POLLIN, 0},
    };

    while (m_clientSocket) {
        try {
            zmq::poll(pollItems);

            if (pollItems[0].revents & ZMQ_POLLIN) {
                ProcessClient();
            }

            if (pollItems[1].revents & ZMQ_POLLIN) {
                ProcessApp();
            }
        } catch (zmq::error_t& e) {
            TF_RUNTIME_ERROR("Network error: %d", e.num());
        }
    }
}

void RprIpcServer::ProcessClient() {
    zmq::message_t msg;
    m_clientSocket.recv(msg);

    std::string command(msg.data<char>(), msg.size());
    TF_DEBUG(RPR_IPC_DEBUG_MESSAGES).Msg("RprIpcServer: received \"%s\" command", command.c_str());

    // Process any RprIpcServer specific commands first
    if (RprIpcTokens->connect == command) {
        if (msg.more()) {
            m_clientSocket.recv(msg);

            std::string addr(msg.data<char>(), msg.size());
            try {
                m_notifySocket = zmq::socket_t(GetZmqContext(), zmq::socket_type::push);
                m_notifySocket.connect(addr);
                m_clientSocket.send(GetZmqMessage(RprIpcTokens->ok));

                TF_DEBUG(RPR_IPC_DEBUG_MESSAGES).Msg("RprIpcServer: connected notifySocket to %s", addr.c_str());
                return;
            } catch (zmq::error_t& e) {
                TF_DEBUG(RPR_IPC_DEBUG_MESSAGES).Msg("RprIpcServer: invalid notify socket address: %s", addr.c_str());
            }
        }

        m_clientSocket.send(GetZmqMessage(RprIpcTokens->fail));
    } else if (RprIpcTokens->disconnect == command) {
        m_notifySocket.close();
        m_clientSocket.send(GetZmqMessage(RprIpcTokens->ok));
    } else if (RprIpcTokens->shutdown == command) {
        m_clientSocket.close();
    } else if (RprIpcTokens->ping == command) {
        m_clientSocket.send(GetZmqMessage(RprIpcTokens->ping));
    } else {
        // notify command listener about any other commands
        auto response = m_listener->ProcessCommand(command);
        TF_DEBUG(RPR_IPC_DEBUG_MESSAGES).Msg("RprIpcServer: response for \"%s\" command: %s", command.c_str(), response.c_str());
        m_clientSocket.send(GetZmqMessage(response));
    }
}

void RprIpcServer::ProcessApp() {
    // Proxy all messages from the application to the client

    zmq::message_t msg;
    m_appSocket.recv(msg);

    {
        // Sniff the packet for `shutdown` command
        if (RprIpcTokens->shutdown.size() == msg.size() &&
            std::strncmp(RprIpcTokens->shutdown.GetText(), msg.data<char>(), msg.size()) == 0) {
            m_clientSocket.close();
            return;
        }
    }

    bool moreMessages;
    do {
        moreMessages = msg.more();

        if (m_notifySocket) {
            auto sendFlags = moreMessages ? zmq::send_flags::sndmore : zmq::send_flags::none;
            m_notifySocket.send(msg, sendFlags);
        }

        if (moreMessages) {
            m_appSocket.recv(msg);
        }
    } while (moreMessages);
}

PXR_NAMESPACE_CLOSE_SCOPE
