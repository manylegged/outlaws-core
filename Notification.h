
#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include "SerialCore.h"

struct Block;

#define NOTIFICATION_TYPE_COUNT 8
#define NOTIFICATION_TYPE(F)                    \
    F(GAIN_XP, 1<<0)                            \
    F(GAIN_LEVEL, 1<<1)                         \
    F(COMMAND_DESTROYED, 1<<2)                    \
    F(PLAYER_BLOCK_DEATH, 1<<3)                   \
    F(OPTION_CHANGED, 1<<4)                       \


DEFINE_ENUM(EnumNotification, NOTIFICATION_TYPE);

#define NOTIFY(TYPE, ...) Notifier::instance().notify(Notification(Notification:: TYPE, ## __VA_ARGS__))

struct Notification {

    enum Type { NOTIFICATION_TYPE(TO_ENUM) };
    
    SerialEnum<EnumNotification>  type;
    Block                        *block  = NULL;
    float                         value = 0.f;
    float2                        pos;

    Notification(uint64 t) : type(t) {}
    Notification(uint64 t, Block *bl) : type(t), block(bl) {}
    Notification(uint64 t, double v) : type(t), value(v) {}
    Notification(uint64 t, float2 p) : type(t), pos(p) {}
    Notification(uint64 t, float2 p, double v) : type(t), pos(p), value(v) {}

};

struct INotifyHandler {

    const uint64 m_registeredTypes;

    virtual void OnNotify(const Notification& notif) = 0;
    INotifyHandler(uint64 types);
    virtual ~INotifyHandler();
    
};


struct Notifier {

private:
    Notifier() {}

    vector<INotifyHandler*> m_handlers[NOTIFICATION_TYPE_COUNT];

public:

    void registerHandler(uint64 types, INotifyHandler* handler);
    void unregisterHandler(uint64 types, INotifyHandler *handler);
    void notify(const Notification& notif);

    static Notifier& instance()
    {
        static Notifier *v = new Notifier;
        return *v;
    }
    
};


#endif // NOTIFICATION_H
