//
// Nav.h - PD controller based movement
// 

// Copyright (c) 2013 Arthur Danskin
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

#ifndef NAV_H_INCLUDED
#define NAV_H_INCLUDED

#include "Geometry.h"

// this is our six dimensional configuration space
// all of these values are in the global reference frame (i.e. position and velocity are not rotated).
struct snConfig {
    float2 position;
    float2 velocity;
    float  angle;
    float  angVel;

    snConfig() : angle(0), angVel(0) {}
};


enum snDimension {
    SN_POSITION  = 1<<0,
    SN_VELOCITY  = 1<<1,
    SN_ANGLE     = 1<<2,
    SN_ANGVEL    = 1<<3,
    SN_TARGET_VEL = 1<<4,       // we don't care about our velocity, but we are passing in the target position velocity
};


struct snPrecision {
    float pos    = 10;
    float vel    = 10;
    float angle  = 0.01;
    float angVel = 0.01;

    static snPrecision exact() {
        snPrecision p;
        p.pos = 0.f;
        p.vel = 0.f;
        p.angle = 0.f;
        p.angVel = 0.f;
        return p;
    }

    bool posEqual(float2 a, float2 b) const { return intersectPointCircle(a, b, pos); }
    bool velEqual(float2 a, float2 b) const { return intersectPointCircle(a, b, vel); }
    bool angleEqual(float a, float b) const { return dot(angleToVector(a), angleToVector(b)) + angle >= 1.f; }
    bool angVelEqual(float a, float b) const { return fabsf(a - b) < angVel; }

    bool configEqual(const snConfig& a, const snConfig& b, uint dims) const
    {
        if ((dims&SN_POSITION) && !posEqual(a.position, b.position))
            return false;
        if ((dims&SN_VELOCITY) && !velEqual(a.velocity, b.velocity))
            return false;
        if ((dims&SN_ANGLE) && !angleEqual(a.angle, b.angle))
            return false;
        if ((dims&SN_ANGVEL) && !angVelEqual(a.angVel, b.angVel))
            return false;
        return true;
    }
};

struct snConfigDims {
    uint     dims;
    snConfig cfg;

    snConfigDims() : dims(0) {}
};

// actions take us between configurations    
struct snAction {
    float2 accel;               // rotated with the body
    float  angAccel;

    snAction()
    {
        accel = float2(0);
        angAccel = 0;
    }
};

// an e.g. thruster
struct snMover {
    float2 accel;               // direction & magnitude of linear acceleration capability (always positive)
    float2 accelNorm;
    float  accelAngAccel;       // angular acceleration resulting from linear acceleration (signed)
    float  angAccel;            // direct angular acceleration capability (always positive)
    bool   useForRotation;      // does this mover have a descent torque to force ratio?
    bool   useForTranslation;   // does this mover have a descent force to torque ratio?
    
    float  accelEnabled;        // how much are we accelerating linearly this step? [0,1]
    float  angAccelEnabled;     // how much are we accelerating rotationally this step? [-1,1]


    void reset(float2 offset, float angle, float force, float mass, float torque, float moment)
    {
        float2 direction = angleToVector(angle);
        accel           = direction * force / mass;
        accelNorm       = direction;
        accelAngAccel   = isZero(offset) ? 0.f : cross(offset, direction * force) / moment;
        angAccel        = torque / moment;

        // FIXME we actually want a different threshhold depending on total torque available
        // if we have only a few thrusters we need to use them all to rotate
        // but at some point the minimal additional rotation speed is not worth the slew
        useForRotation  = ((fabsf(accelAngAccel) / length(accel)) > 0.001) || (angAccel > epsilon);

        // FIXME we can't even hack this by itself - we need to look at all the movers.
        useForTranslation = !isZero(accel);

        accelEnabled    = 0;
        angAccelEnabled = 0;
    }

    snMover() { memset(this, 0, sizeof(*this)); }
};

// navigation for one actor
struct sNav {

    // input/output
	vector<snMover*> movers;
    float2           maxAccel;  // dependent on movers
    float            maxAngAccel;

    // inputs
    snPrecision      precision;
    snConfigDims     dest;
    snConfig         state;
    
    // output result
    snAction         action;

    void setDest(const snConfig &dest1, uint dimensions, const snPrecision& prec)
    {
        dest.cfg  = dest1;
        dest.dims = dimensions;
        precision = prec;
    }

    void setDest(const snConfig &dest1, uint dimensions, float rad)
    {
        dest.cfg      = dest1;
        dest.dims     = dimensions;
        precision.pos = rad;
    }

    bool isAtDest() const
    {
        return precision.configEqual(state, dest.cfg, dest.dims);
    }

    bool isAtDest(const snConfigDims& dest_, const snPrecision& prec) const
    {
        return prec.configEqual(state, dest_.cfg, dest_.dims);
    }

    bool isAtDest(const snConfigDims &dest_) const
    {
        return precision.configEqual(state, dest_.cfg, dest_.dims);
    }
    
    void   onMoversChanged();
    float  moversForAngAccel(float angAccel, bool enable);
    float2 moversForLinearAccel(float2 dir, float threshold, bool enable);
    void   moversDisable();
    float  angAccelForTarget(float destAngle, float destAngVel) const;

    // transform inputs to outputs, return true if destination reached
    bool update();
};

#endif // NAV_H_INCLUDED
