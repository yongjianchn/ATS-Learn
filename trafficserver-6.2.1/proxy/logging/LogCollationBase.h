/** @file

  A brief file description

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */

#ifndef LOG_COLLATION_BASE_H
#define LOG_COLLATION_BASE_H

//-------------------------------------------------------------------------
// LogCollationBase
//-------------------------------------------------------------------------

class LogCollationBase
{
protected:
  // ToDo: Can we we use the stuff from LogSock.h instead??
  struct NetMsgHeader {
    int msg_bytes; // length of the following message
  };

  enum LogCollEvent {
    LOG_COLL_EVENT_NULL = LOG_COLLATION_EVENT_EVENTS_START,
    LOG_COLL_EVENT_SWITCH,
    LOG_COLL_EVENT_READ_COMPLETE,
    LOG_COLL_EVENT_WRITE_COMPLETE,
    LOG_COLL_EVENT_ERROR
  };
};

#endif // LOG_COLLATION_BASE_H
