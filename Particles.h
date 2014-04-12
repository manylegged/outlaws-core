
#ifndef _PARTICLES_H
#define _PARTICLES_H

#include "StdAfx.h"
#include "Graphics.h"

static const bool kBluryParticles = 1;

struct ShaderParticles;

struct ParticleSystem : public IDeletable {

protected:
    
    struct Particle {
        float  startTime = 0.f;
        float  endTime = 0.f;

        float3 position;
        float3 velocity;
        float  angle = 0.f;

        float2 offset;
        
        uint color = 0;             // premultiplied alpha ABGR

        void setColor(uint c, float alpha)
        {
            color = ALPHAF(alpha)|RGB2BGR(PremultiplyAlphaAXXX(c));
            ASSERT(color != 0);
        }
        
        void setTime(float t)
        {
            // particles created during simulation, not during render
            const float time = globals.simTime;
            startTime        = time;
            endTime          = time + max((float)globals.simTimeStep, t);
        }
    };

    struct ParticleTrail {
        float    startTime        = 0.f;
        float    endTime          = 0.f;
        float    rate             = 30.f; // particles per second
        float    lastParticleTime = 0.f;
        float3   position;
        float3   velocity;
        Particle particle;
        Particle particle1;
    };

private:

    friend struct ShaderParticles;

    vector<Particle>       m_vertices;
    IndexBuffer<uint>      m_ibo;
    VertexBuffer<Particle> m_vbo;
    int                    m_addFirst;
    int                    m_addPos;
    uint                   m_lastMaxedStep;
    float                  m_planeZ;
    std::mutex             m_mutex;
    View                   m_view;
    vector<ParticleTrail>  m_trails;
    
    void updateRange(uint first, uint size);

protected:

    int                    m_maxParticles = 0;
    
    bool visible(const float3 &pos, float size) const
    {
        return forceVisible || m_view.intersectCircle(float3(pos.x, pos.y, pos.z + m_planeZ), 10.f * size);
    }

    bool visible(const float2 &pos, float size) const
    {
        return forceVisible || m_view.intersectCircle(float3(pos.x, pos.y, m_planeZ), 10.f * size);
    }

    void add(const Particle &p, bool gradient);
    void addTrail(const ParticleTrail& p) { m_trails.push_back(p); }

public:

    bool forceVisible = false;

    size_t count() const;

    ParticleSystem();
    ~ParticleSystem();

    void render(float3 offset, ShaderState s, const View& view);
    void update();
    void clear();
    
};


#endif // _PARTICLES_H

