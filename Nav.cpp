
//
// Nav.cpp - PD controller based movement
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

#include "StdAfx.h"
#include "Nav.h"

static const float kLinearPosThreshold  = 0.6; // how closely should we thrust to the direction we are trying to thrust?
static const float kLinearVelThreshhold = 0.3; // when adjusting velocity only, accuracy of thrust direction
static const float kMaxLinearAngAccel   = 0.1;

void sNav::moversDisable()
{
    foreach (snMover* m, movers)
    {
        m->accelEnabled    = 0;
        m->angAccelEnabled = 0;
    }
}

// enable movers to move us vaguely in the right direction...
// FIXME make sure the returned acceleration matches the input direction as closely as possible
// FIXME this means that some thrusters may not be fully enabled
float2 sNav::moversForLinearAccel(float2 dir, float threshold, float angAccel, bool enable)
{
    const float amount = length(dir);
    
    float2 accel         = float2(0);
    float  totalAngAccel = 0.f;
    int    enabled       = 0;
    int    disabled      = 0;

    // enable thrusters that move us in the right direction
    foreach (snMover *mv, movers)
    {
        if (mv->useForTranslation && dot(mv->accelNorm, dir) >= threshold)
        {
            if (enable)
                mv->accelEnabled = amount;
            
            accel         += amount * mv->accel;
            totalAngAccel += amount * mv->accelAngAccel;
            enabled++;
        }
    }

    if (!enable || movers.size() == 1 || enabled == 1)
        return accel;
    
    float angAccelError = totalAngAccel;

#if 1
    // turn off thruster one by one until we stop adding rotation
    while (fabsf(angAccelError) > kMaxLinearAngAccel && (enabled - disabled > 1))
    {
        float    mxaa = 0.f;
        snMover *mxmv = NULL;
        
        foreach (snMover *mv, movers) {
            if (mv->useForTranslation && mv->useForRotation) {
                const float aa = fabsf(mv->accelAngAccel * mv->accelEnabled);
                if (sign(mv->accelAngAccel) == sign(angAccelError) && aa > mxaa) {
                    mxaa = aa;
                    mxmv = mv;
                }
            }
        }

        if (!mxmv)
            break;
        
        const float aa = mxmv->accelEnabled * mxmv->accelAngAccel;
        const float v  = min(mxmv->accelEnabled, angAccelError / aa);

        //ASSERT(enabled > disabled);
        accel         -= v * mxmv->accel;
        angAccelError -= v * mxmv->accelAngAccel;
        
        mxmv->accelEnabled -= v;
        disabled++;
    }
#endif

    return accel;
}

// enable movers to rotate in the desired direction
float sNav::moversForAngAccel(float angAccel, bool enable)
{
    ASSERT(!fpu_error(angAccel));
    ASSERT(fabsf(angAccel) < 1.01f);

    float totalAngAccel = 0;
    foreach (snMover *m, movers)
    {
        if (!m->useForRotation)
            continue;
        
        if (sign(m->accelAngAccel) == sign(angAccel))
        {
            if (enable)
                m->accelEnabled = fabsf(angAccel);
            totalAngAccel += fabsf(angAccel) * m->accelAngAccel;
        }
#if 0
        else if (enable) {
            // turn off thrusters in the wrong direction
            m->accelEnabled = 0.f;
        }
#endif

        if (m->angAccel > 0)
        {
            if (enable)
                m->angAccelEnabled = angAccel;
            totalAngAccel += angAccel * m->angAccel;
        }
    }
    ASSERT(!fpu_error(totalAngAccel));
    return totalAngAccel;
}

void sNav::onMoversChanged()
{
    // FIXME we are assuming forwards...
    maxAccel    = moversForLinearAccel(float2(1, 0), kLinearPosThreshold, 0.f, false);
    maxAngAccel = moversForAngAccel(+1, false);
}


// use a pd controller to rotate the body into the correct rotationally
float sNav::angAccelForTarget(float destAngle, float destAngVel) const
{
    // nothing to do if we can't rotate
    if (maxAngAccel < epsilon)
        return 0;

    if (precision.angleEqual(state.angle, destAngle) &&
        precision.angVelEqual(state.angVel, destAngVel))
        return 0;
    
    float approxRotPeriod = 2 * sqrt(2 * M_PI / maxAngAccel);

    const float kp=1.5, kd=-0.3*approxRotPeriod;
    float angleError = dot(angleToVector(destAngle), angleToVector(state.angle + M_PI/2));
    const float angVelError = state.angVel - destAngVel;
    const float angAction = (kp * angleError) + (kd * angVelError);

    return clamp(angAction, -1.f, 1.f);
}

// get the action for this timestep - call every timestep/frame
// action accelerations are from expressed from [-1.0, 1.0], 
//  where 1.0 is accelerating as fast as possible in the positive direction and 0.0 is no acceleration
bool sNav::update()
{
    this->action.accel = float2(0);
    this->action.angAccel = 0;    
    
    moversDisable();

    if (dest.dims&SN_MISSILE_ANGLE)
    {
        const float2 ddirPerp = rotate(normalize(dest.cfg.position - state.position), dest.cfg.angle + M_PI_2f);
        const float2 mydir    = angleToVector(state.angle);
        const float  aerr     = dot(mydir, ddirPerp);
        
        foreach (snMover *mr, movers)
        {
            if (mr->useForTranslation) {
                mr->accelEnabled = 1.f;
            }
            if (mr->useForRotation) {
                mr->angAccelEnabled = aerr;
            }
        }
    }
    else if ((dest.dims&SN_POSITION) && !precision.posEqual(state.position, dest.cfg.position))
    {
        float2 destp    = this->dest.cfg.position;
        float2 destv    = (dest.dims&(SN_VELOCITY|SN_TARGET_VEL)) ? this->dest.cfg.velocity : float2(0);

        float2 u;
        if (dest.dims&SN_TARGET_VEL) {
            float2 normPErrPerp = rotate(normalize(destp - state.position), M_PI_2f);
            float2 vDiff = (state.velocity - destv);
            float2 dErr = normPErrPerp * dot(normPErrPerp, vDiff);
            u = 1.f * (destp - state.position) - 3.f * dErr;
        } else {
            u = 1.f * (destp - state.position) - 1.f * (state.velocity - destv);
        }

        bool needsRotation = false;
        if (length(u) > 0.00001)
        {
            const float2 uBody          = rotate(u, -state.angle);
            const float2 uBodyDir       = normalize(uBody);
       
            this->action.accel = moversForLinearAccel(uBodyDir, kLinearPosThreshold, 0.f, true);
            needsRotation = isZero(this->action.accel) || (dot(normalize(this->action.accel), uBodyDir) < 0.99);

            // FIXME this assumes we can accelerate straight forwards - this is often not the case!
            //(length(this->action.accel) < 0.75 * scaledMaxAccel)*/)   // OR acceleration is not as high as it could be
            if (needsRotation)
            {
                moversDisable();
                float angAccel = angAccelForTarget(vectorToAngle(u), 0);
                this->action.angAccel = moversForAngAccel(angAccel, true);
            }
        }

#if 0
        if (!needsRotation && fabsf(state.angVel) > 0.01)
        {
            // stop spinning - it just looks silly
            float angAccel = clamp(-state.angVel, -1.f, 1.f);
            this->action.angAccel = moversForAngAccel(this, angAccel, true);
        }
#endif
    }
    else
    {
        if (dest.dims&SN_ANGLE)
        {
            float angAccel = angAccelForTarget(this->dest.cfg.angle, this->dest.dims&SN_ANGVEL ? this->dest.cfg.angVel : 0);
            this->action.angAccel = moversForAngAccel(angAccel, true);
        }

        if ((dest.dims&SN_VELOCITY) && !precision.velEqual(dest.cfg.velocity, state.velocity))
        {
            // action outputs are in the ship reference frame
            float2 ve = dest.cfg.velocity - state.velocity;
            ve /= max(length(ve), 0.05f * length(this->maxAccel));
            float2 accel = rotate(float2(clamp(ve.x, -1.f, 1.f), clamp(ve.y, -1.f, 1.f)), -state.angle);
            action.accel = moversForLinearAccel(accel, kLinearVelThreshhold, 0.f, true);
            if (isZero(action.accel) && (dest.dims&SN_VEL_ALLOW_ROTATION)) {
                moversDisable();
                float angAccel = angAccelForTarget(vectorToAngle(accel), 0.f);
                this->action.angAccel = moversForAngAccel(angAccel, true);
            }
        }

    }

    return isAtDest();
}

