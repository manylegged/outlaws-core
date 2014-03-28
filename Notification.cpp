
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

