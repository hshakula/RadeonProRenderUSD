#ifndef RPR_IMAGING_IPC_SERVER_H
#define RPR_IMAGING_IPC_SERVER_H

#include <zmq.hpp>
#include <pxr/usd/sdf/path.h>
#include <rpr/imaging/ipc/api.h>

#include <thread>
#include <mutex>

namespace TinyProcessLib { class Process; }

PXR_NAMESPACE_OPEN_SCOPE

class RprIpcServer {
public:
    /// Each callback is called from network thread, ideally, they should not be heavyweight
    class Listener {
    public:
        /// RprIpcServer propagates all unprocessed commands to the user through this callback
        virtual std::string ProcessCommand(std::string const& command) = 0;
    };

    RPR_IPC_API
    RprIpcServer(Listener* Listener);

    RPR_IPC_API
    ~RprIpcServer();

    /// \class Sender
    ///
    /// Sender designed to allow easy data transmission from many worker threads to the client.
    /// Sender should be used on the same thread it was created on. It can be created only with \c GetSender.
    class Sender {
    public:
        RPR_IPC_API
        void SendLayer(SdfPath const& layerPath, std::string layer);

        RPR_IPC_API
        void RemoveLayer(SdfPath const& layerPath);

    private:
        friend RprIpcServer;
        Sender();

    private:
        zmq::socket_t m_appSocket;
        std::thread::id m_threadId;
    };

    /// The returned sender can be cached to avoid socket creation and connection.
    /// But the sender should be used on the same thread it was created on.
    /// If this function receives a cached sender it will validate thread conformity
    /// and in case of non-compliance, it will create a new sender.
    RPR_IPC_API
    void GetSender(std::shared_ptr<Sender>* sender);

private:
    void Run();

    void ProcessClient();
    void ProcessApp();

private:
    Listener* m_listener;
    std::thread m_networkThread;

    zmq::socket_t m_clientSocket;
    zmq::socket_t m_appSocket;
    zmq::socket_t m_notifySocket;

    std::mutex m_mutex;
    std::unordered_map<std::thread::id, std::weak_ptr<Sender>> m_senders;

    std::unique_ptr<TinyProcessLib::Process> m_viewerProcess;
};

using RprIpcSenderPtr = std::shared_ptr<RprIpcServer::Sender>;

PXR_NAMESPACE_CLOSE_SCOPE

#endif // RPR_IMAGING_IPC_SERVER_H
