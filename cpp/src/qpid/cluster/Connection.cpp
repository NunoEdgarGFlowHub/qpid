/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */
#include "Connection.h"
#include "qpid/framing/AMQFrame.h"
#include "qpid/framing/Invoker.h"
#include "qpid/framing/AllInvoker.h"
#include "qpid/framing/ClusterConnectionDeliverCloseBody.h"
#include "qpid/log/Statement.h"

#include <boost/current_function.hpp>

namespace qpid {
namespace cluster {

using namespace framing;

Connection::Connection(Cluster& c, sys::ConnectionOutputHandler& out,
                       const std::string& wrappedId, ConnectionId myId)
    : cluster(c), self(myId), output(*this, out),
      connection(&output, cluster.getBroker(), wrappedId)
{}

Connection::Connection(Cluster& c, sys::ConnectionOutputHandler& out,
                       const std::string& wrappedId, MemberId myId)
    : cluster(c), self(myId, this), output(*this, out),
      connection(&output, cluster.getBroker(), wrappedId)
{}

Connection::~Connection() {}

void Connection::received(framing::AMQFrame& ) {
    // FIXME aconway 2008-09-02: not called, codec sends straight to deliver
    assert(0);
}

bool Connection::doOutput() { return output.doOutput(); }

// Delivery of doOutput allows us to run the real connection doOutput()
// which stocks up the write buffers with data.
//
void Connection::deliverDoOutput(uint32_t requested) {
    output.deliverDoOutput(requested);
}

// Handle frames delivered from cluster.
void Connection::deliver(framing::AMQFrame& f) {
    QPID_LOG(trace, "DLVR [" << self << "]: " << f);
    // Handle connection controls, deliver other frames to connection.
    if (!framing::invoke(*this, *f.getBody()).wasHandled())
        connection.received(f);
}

void Connection::closed() {
    try {
        // Called when the local network connection is closed. We still
        // need to process any outstanding cluster frames for this
        // connection to ensure our sessions are up-to-date. We defer
        // closing the Connection object till deliverClosed(), but replace
        // its output handler with a null handler since the network output
        // handler will be deleted.
        // 
        connection.setOutputHandler(&discardHandler); 
        cluster.mcastFrame(AMQFrame(in_place<ClusterConnectionDeliverCloseBody>()), self);
        ++mcastSeq;
    }
    catch (const std::exception& e) {
        QPID_LOG(error, QPID_MSG("While closing connection: " << e.what()));
    }
}

void Connection::deliverClose () {
    connection.closed();
    cluster.erase(self);
}

size_t Connection::decode(const char* buffer, size_t size) { 
    QPID_LOG(trace, "mcastBuffer " << self << " " << mcastSeq << " " << size);
    ++mcastSeq;
    cluster.mcastBuffer(buffer, size, self);
    // FIXME aconway 2008-09-01: deserialize?
    return size;
}

void Connection::deliverBuffer(Buffer& buf) {
    QPID_LOG(trace, "deliverBuffer " << self << " " << deliverSeq << " " << buf.available());
    ++deliverSeq;
    while (decoder.decode(buf))
        deliver(decoder.frame); // FIXME aconway 2008-09-01: Queue frames for delivery in separate thread.
}

}} // namespace qpid::cluster

