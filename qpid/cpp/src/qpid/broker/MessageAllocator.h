#ifndef _broker_MessageAllocator_h
#define _broker_MessageAllocator_h

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

/* Used by queues to allocate the next "most desirable" message to a consuming client */


#include "qpid/broker/Consumer.h"

namespace qpid {
namespace broker {

class Queue;
struct QueuedMessage;

class MessageAllocator
{
 protected:
    Queue *queue;
 public:
    MessageAllocator( Queue *q ) : queue(q) {}
    virtual ~MessageAllocator() {};

    // Note: all methods taking a mutex assume the caller is holding the
    // Queue::messageLock during the method call.

    /** Determine the next message available for consumption by the consumer
     * @param next set to the next message that the consumer may acquire.
     * @return true if message is available
     */
    virtual bool nextConsumableMessage( Consumer::shared_ptr& consumer,
                                        QueuedMessage& next,
                                        const sys::Mutex::ScopedLock& lock);

    /** Determine the next message available for browsing by the consumer
     * @param next set to the next message that the consumer may browse.
     * @return true if a message is available
     */
    virtual bool nextBrowsableMessage( Consumer::shared_ptr& consumer,
                                       QueuedMessage& next,
                                       const sys::Mutex::ScopedLock& lock);

    /** check if a message previously returned via next*Message() may be acquired.
     * @param consumer name of consumer that is attempting to acquire the message
     * @param qm the message to be acquired
     * @param messageLock - ensures caller is holding it!
     * @return true if acquire is permitted, false if acquire is no longer permitted.
     */
    virtual bool acquirable( const std::string&,
                             const QueuedMessage&,
                             const sys::Mutex::ScopedLock&);

    /** hook to add any interesting management state to the status map */
    virtual void query(qpid::types::Variant::Map&, const sys::Mutex::ScopedLock&) const;
};

}}

#endif
