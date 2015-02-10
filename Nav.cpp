
//
// Nav.cpp - PD controller based movement
// 

// Copyright (c) 2013-2015 Arthur Danskin
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

static DEFINE_CVAR(float, kNavRotKp, 1.2f);
static DEFINE_CVAR(float, kNavRotKd, 0.3f);


#define USE_EIGEN 0
#if USE_EIGEN
#include "../eigen/Eigen/Dense"
static DEFINE_CVAR(bool, kNavUseEigen, true);
#else
static const bool kNavUseEigen = false;
#endif

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

static float3 moversForAccelEigen(float3 accel, vector<snMover*> &movers, bool enable)
{
#if USE_EIGEN
    typedef Eigen::Matrix<double, 3, Eigen::Dynamic> MatrixN3d;
    typedef Eigen::Matrix<double, Eigen::Dynamic, 3> Matrix3Nd;

    MatrixN3d A(3, movers.size());
    for (int i=0; i<movers.size(); i++)
    {
        A(0, i) = movers[i]->accel.x;
        A(1, i) = movers[i]->accel.y;
        A(2, i) = movers[i]->accelAngAccel;
    }

    Eigen::Vector3d desired(accel.x, accel.y, accel.z);
    // Eigen::VectorXd solution = A.colPivHouseholderQr().solve(desired);
    Eigen::VectorXd solution = Eigen::VectorXd::Zero(A.cols());

    Matrix3Nd M = A.transpose()*(A*A.transpose()).inverse();

    for (int i=0; i<10; i++)
    {
        Eigen::Vector3d err = desired - A * solution;
        solution += M*err;
        for (int j=0; j<solution.size(); j++)
            solution[j] = max(0.0, solution[j]);
    }

    float3 output;
    for (int i=0; i<movers.size(); i++)
    {
        const float val = clamp((float)solution(i), 0.f, 1.f);
        if (enable)
            movers[i]->accelEnabled = val;
        output += val * float3(movers[i]->accel, movers[i]->accelAngAccel);
    }
    return output;
#else
    return float3();
#endif
}

// enable movers to move us vaguely in the right direction...
// FIXME make sure the returned acceleration matches the input direction as closely as possible
// FIXME this means that some thrusters may not be fully enabled
float2 sNav::moversForLinearAccel(float2 dir, float threshold, float maxAngAccel, bool enable)
{
    if (movers.size() == 0)
        return float2();
    
    if (kNavUseEigen)
    {
        float3 val = moversForAccelEigen(float3(1000.f * dir, 0.f), movers, enable);
        return float2(val.x, val.y);
    }
    
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

    if (!enable || movers.size() == 1 || enabled == 1 || (maxAngAccel < 0.f))
        return accel;
    
    float angAccelError = totalAngAccel;

    // turn off thruster one by one until we stop adding rotation
    while (fabsf(angAccelError) > maxAngAccel && (enabled - disabled > 1))
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
    return accel;
}

// enable movers to rotate in the desired direction
float sNav::moversForAngAccel(float angAccel, bool enable)
{
    if (fabsf(angAccel) < epsilon)
        return 0.f;

    if (kNavUseEigen)
    {
        return moversForAccelEigen(float3(0.f, 0.f, 100.f * angAccel), movers, enable).z;
    }

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
    maxAccel       = moversForLinearAccel(float2(1, 0), kLinearPosThreshold, kMaxLinearAngAccel, false);
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

    const float angleError  = distanceAngles(destAngle, state.angle);
    const float maxAngAccel = fabsf(angleError > 0.f ? maxPosAngAccel : maxNegAngAccel);
    
    if (maxAngAccel < epsilon)
        return 0.f;

    const float approxRotPeriod = 2.f * sqrt(M_TAOf / maxAngAccel);

    const bool snappy = !kNavThrustWhileTurning || snap || (dest.dims&SN_SNAPPY);
    
    const float kd=-kNavRotKd*approxRotPeriod, ki=snappy ? 0.f : 0.01f;
    const float angVelError = state.angVel - destAngVel;
    const float angAction = (kNavRotKp * angleError) + (kd * angVelError) + (ki * rotInt);

    if (ki > 0.f)
        rotInt = clamp_mag(rotInt + angleError, 0.f, angleError * (kNavRotKp / ki));

    return clamp(angAction, -1.f, 1.f);
}

bool sNav::tryRotateForAccel(float2 accel)
{
    // FIXME this assumes we can accelerate straight forwards - this is often not the case!
    if (dot(float2(1.f, 0.f), maxAccel) < kNavCanRotateThreshold)
        return true;
    const float destAngle = vectorToAngle(accel);
    const float angAccel = angAccelForTarget(destAngle, 0.f, false);
    const bool canRotate = (angAccel > 0.f) ? maxPosAngAccel > kNavCanRotateThreshold : maxNegAngAccel < -kNavCanRotateThreshold;
    if (!canRotate)
        return true;

    if (!kNavThrustWhileTurning || dotAngles(destAngle, state.angle))
        moversDisable();
    action.angAccel = moversForAngAccel(angAccel, true);
    return action.angAccel == 0.f;
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

            action.accel = moversForLinearAccel(uBodyDir, kLinearPosThreshold, kMaxLinearAngAccel, true);

            const float rotThresh = (dest.dims&SN_POS_ANGLE) ? 0.5f : 0.99f;
            
            if (isZero(action.accel) || dot(normalize(action.accel), uBodyDir) < rotThresh)
            {
                if (tryRotateForAccel(u))
                {
                    // already rotated, try moving again ignoring rotation
                    action.accel = moversForLinearAccel(uBodyDir, kLinearPosThreshold, -1.f, true);
                }
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
        if (kNavUseEigen)
        {
            float angAccel = 0.f;
            if (dest.dims&SN_ANGLE)
                angAccel = angAccelForTarget(dest.cfg.angle,
                                             (dest.dims&SN_ANGVEL) ? dest.cfg.angVel : 0,
                                             true);
            float3 val = moversForAccelEigen(float3((dest.dims&SN_VELOCITY) ? rotate(dest.cfg.velocity - state.velocity, -state.angle) : float2(),
                                                    100.f * angAccel),
                                             movers, true);
            action.accel = float2(val.x, val.y);
            action.angAccel = val.z;
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
                action.accel = moversForLinearAccel(accel, kLinearVelThreshold,
                                                    (dest.dims&SN_VEL_ALLOW_ROTATION) ? -1.f : kMaxLinearAngAccel,
                                                    true);
                if (isZero(action.accel) && (dest.dims&SN_VEL_ALLOW_ROTATION))
                {
                    tryRotateForAccel(accel);
                }
            }
        }

    }

    if (dest.dims == 0)
        rotInt = 0.f;

    return isAtDest();
}

