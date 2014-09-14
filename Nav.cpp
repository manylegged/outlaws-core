
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

static DEFINE_CVAR(float, kLinearPosThreshold, 0.5); // how closely should we thrust to the direction we are trying to thrust?
static DEFINE_CVAR(float, kLinearVelThreshold, 0.3); // when adjusting velocity only, accuracy of thrust direction
static DEFINE_CVAR(float, kMaxLinearAngAccel, 0.1);
static DEFINE_CVAR(float, kNavCanRotateThreshold, 0.1);

static DEFINE_CVAR(bool, kNavThrustWhileTurning, true);

void snMover::reset(float2 offset, float angle, float force, float mass, float torque, float moment)
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

    // FIXME we can't even hack this by itself - we need to look at all the movers.3
    useForTranslation = !isZero(accel);

    accelEnabled    = 0;
    angAccelEnabled = 0;
}

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
    if (fabsf(angAccel) < epsilon)
        return 0.f;

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
    maxAccel       = moversForLinearAccel(float2(1, 0), kLinearPosThreshold, 0.f, false);
    maxPosAngAccel = moversForAngAccel(+1, false);
    maxNegAngAccel = moversForAngAccel(-1, false);
    rotInt         = 0.f;
}


// use a pd controller to rotate the body into the correct rotationally
float sNav::angAccelForTarget(float destAngle, float destAngVel, bool snap) const
{
    if (precision.angleEqual(state.angle, destAngle))
    {
        rotInt = 0.f;
        if (precision.angVelEqual(state.angVel, destAngVel))
            return 0;
    }
    if (snap)
        rotInt = 0.f;

    const float angleError  = getAngleError(destAngle, state.angle);
    const float maxAngAccel = fabsf(angleError > 0.f ? maxPosAngAccel : maxNegAngAccel);
    
    if (maxAngAccel < epsilon)
        return 0.f;

    const float approxRotPeriod = 2.f * sqrt(2.f * M_PIf / maxAngAccel);

    const bool snappy = !kNavThrustWhileTurning || snap || (dest.dims&SN_SNAPPY);
    
    const float kp=1.5f, kd=-0.3f*approxRotPeriod, ki=snappy ? 0.f : 0.01f;
    const float angVelError = state.angVel - destAngVel;
    const float angAction = (kp * angleError) + (kd * angVelError) + (ki * rotInt);

    if (ki > 0.f)
        rotInt = clamp_mag(rotInt + angleError, 0.f, angleError * (kp / ki));

    return clamp(angAction, -1.f, 1.f);
}

void sNav::tryRotateForAccel(float2 accel)
{
    // FIXME this assumes we can accelerate straight forwards - this is often not the case!
    if (dot(float2(1.f, 0.f), maxAccel) < kNavCanRotateThreshold)
        return;
    const float destAngle = vectorToAngle(accel);
    const float angAccel = angAccelForTarget(destAngle, 0.f, false);
    const bool canRotate = (angAccel > 0.f) ? maxPosAngAccel > kNavCanRotateThreshold : maxNegAngAccel < -kNavCanRotateThreshold;
    if (!canRotate)
        return;

    if (!kNavThrustWhileTurning || dotAngles(destAngle, state.angle))
        moversDisable();
    action.angAccel = moversForAngAccel(angAccel, true);
}

// get the action for this timestep - call every timestep/frame
// action accelerations are from expressed from [-1.0, 1.0], 
//  where 1.0 is accelerating as fast as possible in the positive direction and 0.0 is no acceleration
bool sNav::update()
{
    action.accel = float2(0);
    action.angAccel = 0;    
    
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
        const float2 destp = dest.cfg.position;
        const float2 destv = (dest.dims&(SN_VELOCITY|SN_TARGET_VEL)) ? dest.cfg.velocity : float2(0);

        float2 u;
        if (dest.dims&SN_TARGET_VEL) {
            float2 normPErrPerp = rotate(normalize(destp - state.position), M_PI_2f);
            float2 vDiff = (state.velocity - destv);
            float2 dErr = normPErrPerp * dot(normPErrPerp, vDiff);
            u = 1.f * (destp - state.position) - 3.f * dErr;
        } else {
            u = 1.f * (destp - state.position) - 0.95f * (state.velocity - destv);
        }

        if (length(u) > 0.00001)
        {
            const float2 uBody    = rotate(u, -state.angle);
            const float2 uBodyDir = normalize(uBody);
       
            action.accel = moversForLinearAccel(uBodyDir, kLinearPosThreshold, 0.f, true);

            const float rotThresh = (dest.dims&SN_POS_ANGLE) ? 0.5f : 0.99f;
            
            if (isZero(action.accel) || dot(normalize(action.accel), uBodyDir) < rotThresh)
            {
                tryRotateForAccel(u);
            }
            else if (dest.dims&SN_POS_ANGLE)
            {
                const float angAccel = angAccelForTarget(dest.cfg.angle, dest.dims&SN_ANGVEL ? dest.cfg.angVel : 0, false);
                action.angAccel = moversForAngAccel(angAccel, true);
            }
        }
    }
    else
    {
        const bool needsVel = (dest.dims&SN_VELOCITY) && !precision.velEqual(dest.cfg.velocity, state.velocity);
        
        if (dest.dims&SN_ANGLE)
        {
            const float angAccel = angAccelForTarget(dest.cfg.angle,
                                                     dest.dims&SN_ANGVEL ? dest.cfg.angVel : 0,
                                                     !needsVel);
            action.angAccel = moversForAngAccel(angAccel, true);
        }

        if (needsVel)
        {
            // action outputs are in the ship reference frame
            float2 ve = dest.cfg.velocity - state.velocity;
            ve /= max(length(ve), 0.05f * length(maxAccel));
            float2 accel = rotate(float2(clamp(ve.x, -1.f, 1.f), clamp(ve.y, -1.f, 1.f)), -state.angle);
            action.accel = moversForLinearAccel(accel, kLinearVelThreshold, 0.f, true);
            if (isZero(action.accel) && (dest.dims&SN_VEL_ALLOW_ROTATION))
            {
                tryRotateForAccel(accel);
            }
        }

    }

    if (dest.dims == 0)
        rotInt = 0.f;

    return isAtDest();
}

