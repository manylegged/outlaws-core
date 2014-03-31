
//
// Notification.cpp - centralized notification registration/dispatch system
// 

// Copyright (c) 2014 Arthur Danskin
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "StdAfx.h"
#include "Save.h"
#include "Blocks.h"

void Notifier::registerHandler(uint64 types, INotifyHandler* handler)
{
    if (!types)
        return;
    
    for (uint i=0; i<NOTIFICATION_TYPE_COUNT; i++)
    {
        if (types&(1<<i)) {
            m_handlers[i].push_back(handler);
        }
    }

    DPRINT(NOTIFICATION, ("Registered %s for %s",
                          TYPE_NAME(*handler),
                          SerialEnum<EnumNotification>(types).toString().c_str()));
}

void Notifier::unregisterHandler(uint64 types, INotifyHandler *handler)
{
    if (!types)
        return;

    int count = 0;
    for (uint i=0; i<NOTIFICATION_TYPE_COUNT; i++)
    {
        if (types&(1<<i)) {
            count += vector_remove_one(m_handlers[i], handler);
        }
    }
    DPRINT(NOTIFICATION, ("Unregistered %s for %s (found %d)",
                          TYPE_NAME(*handler),
                          SerialEnum<EnumNotification>(types).toString().c_str(),
                          count));
}

void Notifier::notify(const Notification& notif)
{
    int handler = findLeadingOne(notif.type.get());
    ASSERT(0 <= handler && handler < NOTIFICATION_TYPE_COUNT);
    foreach (INotifyHandler *han, m_handlers[handler])
    {
        han->OnNotify(notif);
    }
        
    DPRINT(NOTIFICATION, ("Notification %s found %d handlers", 
                          notif.type.toString().c_str(), (int) m_handlers[handler].size()));
}


INotifyHandler::INotifyHandler(uint64 types) : m_registeredTypes(types)
{
    Notifier::instance().registerHandler(types, this);
}

void INotifyHandler::registerTypes(uint64 types)
{
    if (types != m_registeredTypes)
    {
        Notifier::instance().unregisterHandler(m_registeredTypes, this);
        Notifier::instance().registerHandler(types, this);
        m_registeredTypes = types;
    }
}


INotifyHandler::~INotifyHandler()
{
    Notifier::instance().unregisterHandler(m_registeredTypes, this);
}

