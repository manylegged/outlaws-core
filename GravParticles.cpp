
#include "StdAfx.h"
#include "Graphics.h"
#include "GravParticles.h"
#include "Shaders.h"


GravityParticleSystem::GravityParticleSystem()
{
}

void GravityParticleSystem::resize(int count)
{
    m_particles.resize(count);
    m_mesh.clear();
    m_mesh.cur().luma = luma;
    for (uint i=0; i<m_particles.size(); i++) {
        //m_mesh.PushRect(pl.position, float2(size));
        m_mesh.PushCircleCenterVert(size, edges);
    }
}

bool GravityParticleSystem::emit(float2 pos, float2 vel, float radius)
{
    ASSERT(m_particles.size());

    const float time = globals.renderSimTime;

    for (uint i=0; i<m_particles.size(); i++)
    {
        const int   idx  = ((i + m_addPos) % m_particles.size());
        Particle&   pl   = m_particles[idx];

        if (time >= pl.endTime || 
            !intersectPointCircle(pl.position, attractorPos, maxDist) ||
            intersectPointCircle(pl.position, attractorPos, minDist))
        {
            pl.startTime = time;
            pl.endTime   = time + lifeTime;
            pl.position = pos;
            pl.velocity = vel;
            m_liveCount++;
            return true;
        }
    }
    return false;
}

bool GravityParticleSystem::update()
{
    m_emitterBoundingRadius = float2(0.f);

    const float2 attractorVel   = attractorPos - m_attractorLastPos;
    const float2 attractorAccel = attractorVel - m_attractorLastVel;
    m_attractorLastPos = attractorPos;
    m_attractorLastVel = attractorVel;

    const float time     = globals.renderSimTime;
    const int   verts    = edges+1;
    const float timestep = globals.frameTime * timeFactor;


    if (!m_liveCount && !autoemit)
        return false;

    m_liveCount = 0;

    for (uint i=0; i<m_particles.size(); i++)
    {
        Particle& pl = m_particles[i];

        const float2 deltap = attractorPos - pl.position;
        const float  dist   = length(deltap);

        if (time >= pl.endTime || minDist > dist || dist > maxDist)
        {
            if (autoemit)
            {
                pl.startTime = time;
                pl.endTime   = time + randrange(0.f, lifeTime);
                const float2 offset = emitterSize * randpolar(0.25f, 1.f);
                pl.position = emitterPos + offset;
                pl.velocity = emitterVel + 
                              vel0 * normalize(rotate90(offset)) + 
                              vel0 * randpolar(0.f, 0.25f);
            }
            else
            {
                pl.endTime = 0.f;
                m_addPos = i;
                for (uint j=0; j<verts; j++) {
                    VertexPosColorLuma& vtx = m_mesh.getVertex(i * verts + j);
                    vtx.pos = float3(0.f, 0.f, -9999999999999.f);
                }
                continue;
            }
        }

        m_liveCount++;

        const float damp = lerp(attrDamping, damping, min(1.f, dist / attractorSize));

        if (dist > 0.f) {
            const float  r           = max(dist, attractorSize);
            const float2 toAttractor = deltap / dist;
            const float  accelM      = (attraction / (r * r));
            const float2 accel       = toAttractor * accelM;
            pl.velocity += timestep * accel;
            pl.velocity += 0.75f / timeFactor * attractorAccel;

            if (accelM < minVel) {
                const float aspeed = dot(pl.velocity, toAttractor);
                if (aspeed < minVel) {
                    pl.velocity += (minVel - aspeed) * toAttractor;
                }
            }
        }
        
        pl.velocity -= timestep * pl.velocity * (1.f - damp);
        pl.position += timestep * pl.velocity;

        const float talpha = max(0.f, min(fadeTime, min(time - pl.startTime, pl.endTime - time)) /
                                 fadeTime);
        const float dalpha = min(inv_lerp(maxDist, 0.9f * maxDist, dist),
                                 inv_lerp(minDist, 2.f * minDist, dist));
        const uint vtxColor = PremultiplyAlphaXXX(RGB2BGR(color), (talpha * dalpha * palpha), alpha);

        const float2 rot = normalize(pl.velocity);
        for (uint j=0; j<verts; j++) 
        {
            VertexPosColorLuma& vtx = m_mesh.getVertex(i * verts + j);
            const float2 dir = rotate(getCircleVertOffset(j, edges), rot);
            if (j==0) {
                vtx.pos   = float3(pl.position, 0.f);
                vtx.color = vtxColor;
            } else {
                vtx.pos   = float3(pl.position + size * pl.radius * dir, 0.f);
                vtx.color = 0;
            }
        }
        
        m_emitterBoundingRadius = max(m_emitterBoundingRadius, abs(pl.position - emitterPos));
    }
    return true;
}

void GravityParticleSystem::render(const ShaderState &ss)
{
    if (update())
        m_mesh.Draw(ss, ShaderColor::instance());
}

void GravityParticleSystem::render(TriMesh<VertexPosColorLuma>& mesh)
{
    if (update())
        mesh.Push(m_mesh);
}

