#pragma once
using namespace std;

#include <iostream>
#include <iostream>
#include <string>
#include <vector>
#include <atomic>
#include <chrono>
#include <mutex>
#include <queue>
#include <thread>
#include <stdexcept>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <cerrno>
#include "interfaces.h"
#include "utilities.h"
#include "pixeltypes.h"
#include "serialization.h"

// How long to wait for a connection to be established or data sent

constexpr auto kConnectTimeout = 2000ms; 
constexpr auto kSendTimeout    = 3000ms; 

// SocketChannel
//
// Represents a socket connection to a NightDriverStrip client. Keeps a queue of frames and 
// pops them off the queue and sends them on a worker thread. The worker thread will attempt
// to connect to the client if it is not already connected. The worker thread will also
// attempt to reconnect if the connection is lost.

class ClientResponse;

class SocketChannel : public ISocketChannel
{
public:
    SocketChannel(const string& hostName, const string& friendlyName, uint16_t port = 49152)
        : _hostName(hostName),
          _friendlyName(friendlyName),
          _port(port),
          _isConnected(false),
          _running(false),
          _socketFd(-1),
          _lastClientResponse(),
          _dataSentCount(0),
          _lastDataSentCountReset(system_clock::now()),
          _lastConnectionAttempt(system_clock::now()),
          _reconnectCount(0)
    {
    }

    ~SocketChannel() override
    {
        Stop();
        CloseSocket();
    }

    uint32_t GetReconnectCount() const override
    {
        return _reconnectCount;
    }

    uint16_t Port() const override
    {
        return _port;
    }

    void Start() override
    {
        lock_guard<mutex> lock(_mutex);
        if (!_running)
        {
            _running = true;
            _workerThread = thread(&SocketChannel::WorkerLoop, this);
        }
    }

    void Stop() override
    {
        {
            lock_guard<mutex> lock(_mutex);
            _running = false;
        }

//        if (_workerThread.joinable())
//            _workerThread.join();

        CloseSocket();
    }

    bool IsConnected() const override
    {
        lock_guard<mutex> lock(_mutex);
        return _isConnected;
    }
    
    const string& HostName() const override { return _hostName; }
    const string& FriendlyName() const override { return _friendlyName; }
    
    // LastClientResponse
    // 
    // A copy of the last success/stats packet we got back from the client
    
    const ClientResponse& LastClientResponse() const override 
    { 
        lock_guard<mutex> lock(_responseMutex);
        return _lastClientResponse; 
    }

    // CompressFrame
    //
    // Takes a frame of binary data, compresses it, and inserts a small header
    // in front of it with a magic number and the size of the compressed data.

    vector<uint8_t> CompressFrame(const vector<uint8_t>& data) override
    {
        constexpr uint32_t COMPRESSED_HEADER_TAG = 0x44415645; // Magic "DAVE" tag
        constexpr uint32_t CUSTOM_TAG = 0x12345678;

        // Compress the data
        auto compressedData = Utilities::Compress(data);

        // Create the compressed frame
        return Utilities::CombineByteArrays(
            Utilities::DWORDToBytes(COMPRESSED_HEADER_TAG),
            Utilities::DWORDToBytes(static_cast<uint32_t>(compressedData.size())),
            Utilities::DWORDToBytes(static_cast<uint32_t>(data.size())),
            Utilities::DWORDToBytes(CUSTOM_TAG),
            std::move(compressedData)
        );
    }

bool EnqueueFrame(vector<uint8_t>&& frameData) override
{
    bool isQueueFull = false;
    {
        lock_guard<mutex> lock(_queueMutex);
        if (_frameQueue.size() >= MaxQueueDepth)
            isQueueFull = true;
        else
            _frameQueue.push(std::move(frameData));
    }

    // If the queue is full, we reset the socket, but we make sure not to be holding the mutex when we do so
    // because CloseSocket will also try to, and that would cause a deadlock.  It is not a re-entrant mutex.

    if (isQueueFull)
    {
        //cout << "Queue is full at " << _hostName << "[" << _friendlyName << "] dropping frame and resetting socket" << endl;
        CloseSocket(); // Called outside the lock
        return false;
    }

    return true;
}

private:

    // Worker Loop
    //
    // The main duties of the WorkerLoop are to send frames to the client and read responses from the client.
    // It continually watches for new packets to appear int he queue and then sends them in batches.
    // It also reads responses from the client and updates the lastClientResponse member variable.

    void WorkerLoop()
    {
        steady_clock::time_point lastSendTime = steady_clock::now();
        constexpr auto kMaxBatchSize = 20;
        constexpr auto xMaxBatchDelay = 250ms;
        constexpr auto reconnectDelay = 1000ms; // 5 second delay between connection attempts

        while (_running)
        {
            try
            {
                vector<uint8_t> combinedBuffer;
                size_t totalBytes = 0;
                size_t packetCount = 0;

                auto now = steady_clock::now();
                auto timeToSend = duration_cast<milliseconds>(now - lastSendTime) >= xMaxBatchDelay;

                // If the queue is not empty and we have enough frames to send, or it's time to send.
                // So we can send on hitting the threshold of packets or delay

                if (!_frameQueue.empty() && (_frameQueue.size() >= kMaxBatchSize || timeToSend))
                {
                    unique_lock<mutex> lock(_queueMutex);
                    
                    while (!_frameQueue.empty() && packetCount < kMaxBatchSize)
                    {
                        vector<uint8_t>& frame = _frameQueue.front();
                        totalBytes += frame.size();
                        packetCount++;
                        combinedBuffer.insert(combinedBuffer.end(), frame.begin(), frame.end());
                        _frameQueue.pop();
                    }
                }

                if (packetCount > 0)
                {
                    // cout << "Sending " << packetCount << " packets, " << totalBytes << " bytes" << " at queue size " << _frameQueue.size() << endl;

                    if (!combinedBuffer.empty())
                    {
                        lastSendTime = steady_clock::now();
                        optional<ClientResponse> response = SendFrame(std::move(combinedBuffer));
                        if (response)
                        {
                            lock_guard<mutex> lock(_responseMutex);
                            _lastClientResponse = std::move(*response);
                        }
                    }
                }
            }
            catch (const exception& e)
            {
                cerr << "WorkerLoop exception: " << e.what() << endl;
                CloseSocket();
                
                // Wait before attempting to reconnect
                auto now = system_clock::now();
                if (duration_cast<milliseconds>(now - _lastConnectionAttempt) < reconnectDelay)
                {
                    cout << "Waiting for " << duration_cast<milliseconds>(reconnectDelay - (now - _lastConnectionAttempt)).count() << "ms before reconnecting" << endl; 
                    this_thread::sleep_for(reconnectDelay - (now - _lastConnectionAttempt));
                    continue;
                }
            }

            this_thread::sleep_for(milliseconds(1));
        }
    }

    // ReadSocketResponse
    //
    // When we send a frame to the client, it sends us a stats/result packet back.  This function reads
    // the response from the socket and returns it as a ClientResponse object.  If the response is invalid
    // or the socket is closed, nullopt is returned.

    optional<ClientResponse> ReadSocketResponse() 
    {
        const size_t cbToRead = sizeof(ClientResponse);

        // Poll for socket readiness
        pollfd pfd;
        pfd.fd = _socketFd;
        pfd.events = POLLIN;

        if (poll(&pfd, 1, 0) <= 0) 
            return nullopt;

        // Read the first byte to determine the byte count.  Older clients might send shorter packetts
        // so we cannot just try to read a full "current version" packet out of an old client's stream,
        // as there won't be enough bytes in the structure to satisfy the read.

        uint8_t byteCount = 0;
        ssize_t readBytes = recv(_socketFd, &byteCount, 1, MSG_PEEK);
        if (readBytes <= 0)
        {
            // Socket error or no data
            return nullopt;
        }

        // Compare the byte count to the expected size
        if (byteCount != static_cast<uint8_t>(cbToRead))
        {
            // Invalid byte count; eat the contents
            vector<uint8_t> tempBuffer(byteCount);
            recv(_socketFd, tempBuffer.data(), byteCount, 0); // Consume the invalid response
            return nullopt;
        }

        // Read the full response
        vector<uint8_t> buffer(cbToRead);
        readBytes = recv(_socketFd, buffer.data(), cbToRead, 0);
        if (readBytes == static_cast<ssize_t>(cbToRead))
        {
            ClientResponse response;
            memcpy(&response, buffer.data(), cbToRead);
            response.TranslateClientResponse();
            return response;
        }

        return nullopt;
    }

    bool SetSocketNonBlocking(int socketFd)
    {
        int flags = fcntl(socketFd, F_GETFL, 0);
        if (flags == -1) return false;
        return (fcntl(socketFd, F_SETFL, flags | O_NONBLOCK) != -1);
    }

    bool SetSocketSendTimeout(int socketFd, milliseconds timeout)
    {
        struct timeval timeouttv;
        timeouttv.tv_sec = timeout.count() / 1000;
        timeouttv.tv_usec = (timeout.count() % 1000) * 1000;

        return setsockopt(socketFd, SOL_SOCKET, SO_SNDTIMEO, &timeouttv, sizeof(timeouttv)) == 0;
    }

    optional<ClientResponse> SendFrame(const vector<uint8_t>&& frame)
    {
        if (_socketFd == -1)
        {
            if (!ConnectSocket())
            {
                lock_guard<mutex> lock(_mutex);
                _isConnected = false;
                return nullopt;
            }

            // Set the send timeout (e.g., 5 seconds)
            if (!SetSocketSendTimeout(_socketFd, kSendTimeout))
            {
                cerr << "Could not set send timeout for " << _friendlyName << endl;
                CloseSocket();
                lock_guard<mutex> lock(_mutex);
                _isConnected = false;
                return nullopt;
            }
        }

        size_t totalSent = 0;
        while (totalSent < frame.size())
        {
            ssize_t sent = send(_socketFd, 
                                frame.data() + totalSent, 
                                frame.size() - totalSent, 
                                MSG_NOSIGNAL);
            
            if (sent == -1)
            {
                CloseSocket();
                lock_guard<mutex> lock(_mutex);
                _isConnected = false;
                return nullopt;
            }
            totalSent += sent;
        }

        {
            lock_guard<mutex> lock(_mutex);
            _isConnected = true;
            _dataSentCount += totalSent;
        }

        optional<ClientResponse> lastValidResponse = nullopt;
        for (;;)
        {
            optional<ClientResponse> response = ReadSocketResponse();
            if (!response)
                break;

            lastValidResponse = std::move(response);
        }
        return lastValidResponse;
    }


    bool ConnectSocket()
    {
        _lastConnectionAttempt = system_clock::now();
        
        int tempSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (tempSocket == -1)
            return false;

        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(_port);

        if (inet_pton(AF_INET, _hostName.c_str(), &serverAddr.sin_addr) <= 0)
        {
            close(tempSocket);
            return false;
        }

        // Set socket to non-blocking mode
        if (!SetSocketNonBlocking(tempSocket))
        {
            cerr << "Could not set socket to non-blocking mode for " << _friendlyName << endl;
            close(tempSocket);
            return false;
        }

        // Non-blocking connect
        int result = connect(tempSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
        if (result == -1)
        {
            if (errno != EINPROGRESS)
            {
                cerr << "Could not connect to " << _hostName << " [" << _friendlyName << "] errno=" << errno << endl;
                close(tempSocket);
                return false;
            }

            // Wait for connection with timeout
            pollfd pfd;
            pfd.fd = tempSocket;
            pfd.events = POLLOUT;
            
            if (poll(&pfd, 1, kConnectTimeout.count()) <= 0)
            {
                cerr << "Connection timeout to " << _hostName << " [" << _friendlyName << "]" << endl;
                close(tempSocket);
                return false;
            }

            // Check if connection was successful
            int error = 0;
            socklen_t len = sizeof(error);
            if (getsockopt(tempSocket, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0)
            {
                close(tempSocket);
                return false;
            }
        }
        ++_reconnectCount;
        cout << "Connection number " << _reconnectCount << " to " << _hostName << ":" << _port << " [" << _friendlyName << "] on thread " << this_thread::get_id() << endl; 
        _socketFd = tempSocket;
        return true;
    }

    void CloseSocket()
    {
        // Clear the queue
        lock_guard<mutex> queueLock(_queueMutex);
        queue<vector<uint8_t>> empty;
        _frameQueue.swap(empty);

        if (_socketFd != -1)
        {
            cout << "Closing socket connection to " <<_hostName <<  " [" << _friendlyName << "]" << endl;
            close(_socketFd);
            _socketFd = -1;
        }
        _isConnected = false;

    }

    uint32_t BytesPerSecond()
    {
        lock_guard<mutex> lock(_mutex);
        
        system_clock::time_point now = system_clock::now();
        auto elapsed = duration_cast<duration<double>>(now - _lastDataSentCountReset).count();

        if (elapsed == 0.0)
            return 0;

        uint32_t bytesPerSecond = static_cast<uint32_t>(_dataSentCount / elapsed);

        constexpr auto bpsResetTimer = 3.0;
        if (elapsed >= bpsResetTimer)
        {
            _lastDataSentCountReset = now;
            _dataSentCount = 0;
        }

        return bytesPerSecond;
    }
    
private:
    static constexpr uint16_t CommandPixelData = 3;
    static constexpr size_t   MaxQueueDepth = 100;

    string _hostName;
    string _friendlyName;
    uint16_t _port;

    mutable mutex _mutex;
    mutable mutex _queueMutex;
    mutable mutex _responseMutex;
    
    atomic<bool> _isConnected;
    atomic<bool> _running;

    queue<vector<uint8_t>> _frameQueue;
    thread _workerThread;

    ClientResponse _lastClientResponse;
    system_clock::time_point _lastDataSentCountReset;
    system_clock::time_point _lastConnectionAttempt;
    uint32_t _dataSentCount;

    uint32_t _reconnectCount;
    
    int _socketFd;
};