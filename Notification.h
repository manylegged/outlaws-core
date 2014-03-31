
//
// Notification.h - centralized notification registration/dispatch system
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

private:
    uint64 m_registeredTypes;
public:

    virtual void OnNotify(const Notification& notif) = 0;
    void registerTypes(uint64 types);
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
