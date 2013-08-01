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
#include "RemoteBackup.h"
#include "QueueGuard.h"
#include "TxReplicator.h"
#include "qpid/broker/Broker.h"
#include "qpid/broker/Connection.h"
#include "qpid/broker/Queue.h"
#include "qpid/broker/QueueRegistry.h"
#include "qpid/log/Statement.h"
#include <boost/bind.hpp>

namespace qpid {
namespace ha {

using sys::Mutex;
using boost::bind;

RemoteBackup::RemoteBackup(
    const BrokerInfo& info, broker::Connection* c
) : brokerInfo(info), replicationTest(NONE), started(false), connection(c), reportedReady(false)
{
    std::ostringstream oss;
    oss << "Remote backup at " << info << ": ";
    logPrefix = oss.str();
    QPID_LOG(debug, logPrefix << "Connected");
}

RemoteBackup::~RemoteBackup() {
    // Don't cancel here, cancel must be called explicitly in a locked context
    // where we know the connection pointer is still good.
}

void RemoteBackup::cancel() {
    QPID_LOG(debug, logPrefix << "Cancelled " << (connection? "connected":"disconnected")
             << " backup: " << brokerInfo);
    for (GuardMap::iterator i = guards.begin(); i != guards.end(); ++i)
        i->second->cancel();
    guards.clear();
    if (connection) {
        connection->abort();
        connection = 0;
    }
}

bool RemoteBackup::isReady() {
    return started && connection && catchupQueues.empty();
}

void RemoteBackup::catchupQueue(const QueuePtr& q, bool createGuard) {
    if (replicationTest.getLevel(*q) == ALL) {
        QPID_LOG(debug, logPrefix << "Catch-up queue"
                 << (createGuard ? " and guard" : "") << ": " << q->getName());
        catchupQueues.insert(q);
        if (createGuard) guards[q].reset(new QueueGuard(*q, brokerInfo));
    }
}

RemoteBackup::GuardPtr RemoteBackup::guard(const QueuePtr& q) {
    GuardMap::iterator i = guards.find(q);
    GuardPtr guard;
    if (i != guards.end()) {
        guard = i->second;
        guards.erase(i);
    }
    return guard;
}

namespace {
typedef std::set<boost::shared_ptr<broker::Queue> > QS;
struct QueueSetPrinter {
    const QS& qs;
    std::string prefix;
    QueueSetPrinter(const std::string& p, const QS& q) : qs(q), prefix(p) {}
};
std::ostream& operator<<(std::ostream& o, const QueueSetPrinter& qp) {
    if (!qp.qs.empty()) o << qp.prefix;
    for (QS::const_iterator i = qp.qs.begin(); i != qp.qs.end(); ++i)
        o << (*i)->getName() << " ";
    return o;
}
}

void RemoteBackup::ready(const QueuePtr& q) {
    catchupQueues.erase(q);
    if (catchupQueues.size()) {
        QPID_LOG(debug, logPrefix << "Caught up on queue: " << q->getName() << ", "
                 << catchupQueues.size() << " remain to catch up");
    }
    else
        QPID_LOG(debug, logPrefix << "Caught up on queue: " << q->getName() );
}

// Called via BrokerObserver::queueCreate and from catchupQueue
void RemoteBackup::queueCreate(const QueuePtr& q) {
    if (replicationTest.getLevel(*q) == ALL)
        guards[q].reset(new QueueGuard(*q, brokerInfo));
}

// Called via BrokerObserver
void RemoteBackup::queueDestroy(const QueuePtr& q) {
    catchupQueues.erase(q);
    GuardMap::iterator i = guards.find(q);
    if (i != guards.end()) {
        i->second->cancel();
        guards.erase(i);
    }
}

bool RemoteBackup::reportReady() {
    if (!reportedReady && isReady()) {
        reportedReady = true;
        return true;
    }
    return false;
}

}} // namespace qpid::ha
