// Copyright (c) 2020 by Robert Bosch GmbH. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "uds.hpp"
#include "iceoryx_utils/cxx/smart_c.hpp"

#include <chrono>

UDS::UDS(const std::string& publisherName, const std::string& subscriberName) noexcept
    : m_publisherName(publisherName)
    , m_subscriberName(subscriberName)
{
}

void UDS::initLeader() noexcept
{
    init();

    std::cout << "waiting for follower" << std::endl;

    receivePerfTopic();
}

void UDS::initFollower() noexcept
{
    init();

    std::cout << "registering with the leader, if no leader this will crash with a socket error now" << std::endl;

    sendPerfTopic(sizeof(PerfTopic), true);
}

void UDS::init() noexcept
{
    std::cout << "starting client side ... " << std::flush;

    // initialize the sockAddr data structure with the provided name
    memset(&m_sockAddrPublisher, 0, sizeof(m_sockAddrPublisher));
    m_sockAddrPublisher.sun_family = AF_LOCAL;
    strncpy(m_sockAddrPublisher.sun_path, m_publisherName.c_str(), m_publisherName.size());

    auto socketCallPublisher = iox::cxx::makeSmartC(
        socket, iox::cxx::ReturnMode::PRE_DEFINED_ERROR_CODE, {ERROR_CODE}, {}, AF_LOCAL, SOCK_DGRAM, 0);

    if (socketCallPublisher.hasErrors())
    {
        std::cout << "socket error" << std::endl;
        exit(1);
    }

    m_sockfdPublisher = socketCallPublisher.getReturnValue();

    std::cout << "done" << std::endl;

    std::cout << "starting server side ... " << std::flush;

    // initialize the sockAddr data structure with the provided name
    memset(&m_sockAddrSubscriber, 0, sizeof(m_sockAddrSubscriber));
    m_sockAddrSubscriber.sun_family = AF_LOCAL;
    strncpy(m_sockAddrSubscriber.sun_path, m_subscriberName.c_str(), m_subscriberName.size());

    auto socketCallSubscriber = iox::cxx::makeSmartC(
        socket, iox::cxx::ReturnMode::PRE_DEFINED_ERROR_CODE, {ERROR_CODE}, {}, AF_LOCAL, SOCK_DGRAM, 0);

    if (socketCallSubscriber.hasErrors())
    {
        std::cout << "socket error" << std::endl;
        exit(1);
    }

    m_sockfdSubscriber = socketCallSubscriber.getReturnValue();

    unlink(m_sockAddrSubscriber.sun_path);

    auto bindCall = iox::cxx::makeSmartC(bind,
                                         iox::cxx::ReturnMode::PRE_DEFINED_ERROR_CODE,
                                         {ERROR_CODE},
                                         {},
                                         m_sockfdSubscriber,
                                         reinterpret_cast<struct sockaddr*>(&m_sockAddrSubscriber),
                                         static_cast<socklen_t>(sizeof(m_sockAddrSubscriber)));

    if (bindCall.hasErrors())
    {
        std::cout << "bind error" << std::endl;
        exit(1);
    }

    std::cout << "done" << std::endl;
}

void UDS::shutdown() noexcept
{
    std::cout << "shutdown" << std::endl;

    if (m_sockfdPublisher != INVALID_FD)
    {
        auto closeCall = iox::cxx::makeSmartC(
            closePlatformFileHandle, iox::cxx::ReturnMode::PRE_DEFINED_ERROR_CODE, {ERROR_CODE}, {}, m_sockfdPublisher);

        if (closeCall.hasErrors())
        {
            std::cout << "close error" << std::endl;
            exit(1);
        }
    }

    if (m_sockfdSubscriber != INVALID_FD)
    {
        auto closeCall = iox::cxx::makeSmartC(closePlatformFileHandle,
                                              iox::cxx::ReturnMode::PRE_DEFINED_ERROR_CODE,
                                              {ERROR_CODE},
                                              {},
                                              m_sockfdSubscriber);

        if (closeCall.hasErrors())
        {
            std::cout << "close error" << std::endl;
            exit(1);
        }

        unlink(m_sockAddrSubscriber.sun_path);
    }
}

void UDS::send(const void* buffer, uint32_t length) noexcept
{
    auto sendCall = iox::cxx::makeSmartC(sendto,
                                         iox::cxx::ReturnMode::PRE_DEFINED_ERROR_CODE,
                                         {ERROR_CODE},
                                         {},
                                         m_sockfdPublisher,
                                         buffer,
                                         length,
                                         static_cast<int>(0),
                                         reinterpret_cast<struct sockaddr*>(&m_sockAddrPublisher),
                                         static_cast<socklen_t>(sizeof(m_sockAddrPublisher)));

    if (sendCall.hasErrors())
    {
        std::cout << std::endl << "send error" << std::endl;
        exit(1);
    }
}

void UDS::receive(void* buffer) noexcept
{
    auto recvCall = iox::cxx::makeSmartC(recvfrom,
                                         iox::cxx::ReturnMode::PRE_DEFINED_ERROR_CODE,
                                         {ERROR_CODE},
                                         {},
                                         m_sockfdSubscriber,
                                         buffer,
                                         MAX_MESSAGE_SIZE,
                                         0,
                                         nullptr,
                                         nullptr);

    if (recvCall.hasErrors())
    {
        std::cout << "receive error" << std::endl;
        exit(1);
    }
}

void UDS::sendPerfTopic(uint32_t payloadSizeInBytes, bool runFlag) noexcept
{
    uint8_t buffer[payloadSizeInBytes];
    auto sample = reinterpret_cast<PerfTopic*>(&buffer[0]);

    // Specify the payload size for the measurement
    sample->payloadSize = payloadSizeInBytes;
    sample->run = runFlag;
    if (payloadSizeInBytes <= MAX_MESSAGE_SIZE)
    {
        sample->subPacktes = 1;
        send(&buffer, payloadSizeInBytes);
    }
    else
    {
        sample->subPacktes = payloadSizeInBytes / MAX_MESSAGE_SIZE;
        for (uint32_t i = 0; i < sample->subPacktes; ++i)
        {
            send(&buffer, MAX_MESSAGE_SIZE);
        }
    }
}

PerfTopic UDS::receivePerfTopic() noexcept
{
    receive(&m_message[0]);

    auto receivedSample = reinterpret_cast<const PerfTopic*>(&m_message[0]);

    if (receivedSample->subPacktes > 1)
    {
        for (uint32_t i = 0; i < receivedSample->subPacktes - 1; ++i)
        {
            receive(&m_message[0]);
        }
    }

    return *receivedSample;
}


void UDS::prePingPongLeader(uint32_t payloadSizeInBytes) noexcept
{
    sendPerfTopic(payloadSizeInBytes, true);
}

void UDS::postPingPongLeader() noexcept
{
    // Wait for the last response
    receivePerfTopic();

    std::cout << "done" << std::endl;
}

void UDS::triggerEnd() noexcept
{
    sendPerfTopic(sizeof(PerfTopic), false);
}

double UDS::pingPongLeader(int64_t numRoundTrips) noexcept
{
    auto start = std::chrono::high_resolution_clock::now();

    // run the performance test
    for (auto i = 0; i < numRoundTrips; ++i)
    {
        auto perfTopic = receivePerfTopic();
        sendPerfTopic(perfTopic.payloadSize, true);
    }

    auto finish = std::chrono::high_resolution_clock::now();

    constexpr int64_t TRANSMISSIONS_PER_ROUNDTRIP{2};
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start);
    auto latencyInNanoSeconds = (duration.count() / (numRoundTrips * TRANSMISSIONS_PER_ROUNDTRIP));
    auto latencyInMicroSeconds = static_cast<double>(latencyInNanoSeconds) / 1000;
    return latencyInMicroSeconds;
}

void UDS::pingPongFollower() noexcept
{
    while (true)
    {
        auto perfTopic = receivePerfTopic();

        // stop replying when no more run
        if (!perfTopic.run)
        {
            break;
        }

        sendPerfTopic(perfTopic.payloadSize, true);
    }
}