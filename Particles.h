
#ifndef _PARTICLES_H
#define _PARTICLES_H

#include "StdAfx.h"
#include "Graphics.h"

static const bool kBluryParticles = 1;

struct ShaderParticles;

struct IParticleShader : public ShaderProgramBase {

    virtual void UseProgram(const ShaderState& ss, const View& view, float depth, float time) const=0;
};

void ShaderParticlesInstance();

struct ParticleSystem : public IDeletable {

    struct Particle {
        float  startTime = 0.f;
        float  endTime = 0.f;

        float3 position;
        float3 velocity;

        float2 offset;
        
        uint color = 0;             // premultiplied alpha abgr

        void setColor(uint c, float alpha)
        {
            color = ALPHAF(alpha)|rgb2bgr(PremultiplyAlphaAXXX(c));
            DASSERT(color != 0);
        }
    };
    
protected:

    struct ParticleTrail {
        float    startTime        = 0.f;
        float    endTime          = 0.f;
        float    rate             = 30.f; // particles per second
        float    lastParticleTime = 0.f;
        float    arcRadius        = 9999999.f;
        float3   position;
        float2   velocity;
        Particle particle;
        Particle particle1;
    };

private:

    friend struct ShaderParticles;

    vector<Particle>        m_vertices;
    IndexBuffer<uint>       m_ibo;
    VertexBuffer<Particle>  m_vbo;
    int                     m_addFirst;
    int                     m_addPos;
    uint                    m_lastMaxedStep;
    float                   m_planeZ;
    std::mutex              m_mutex;
    View                    m_view;
    vector<ParticleTrail>   m_trails;
    const IParticleShader  *m_program = NULL;
    
    void updateRange(uint first, uint size);

protected:

    uint                   m_simStep;
    float                  m_simTime;
    int                    m_maxParticles = 0;
    
    bool visible(const float3 &pos, float size) const
    {
        return forceVisible || m_view.intersectCircle(float3(pos.x, pos.y, pos.z + m_planeZ), 10.f * size);
    }

    bool visible(const float2 &pos, float size) const
    {
        return forceVisible || m_view.intersectCircle(float3(pos.x, pos.y, m_planeZ), 10.f * size);
    }

    void setTime(Particle &p, float t);
    void add(const Particle &p, float angle, bool gradient);
    void setParticles(vector<Particle>& particles);
    void addTrail(const ParticleTrail& p) { m_trails.push_back(p); }

public:

    bool forceVisible = false;

    size_t count() const;
    void setProgram(const IParticleShader *prog) { m_program = prog; }

    ParticleSystem();
    ~ParticleSystem();

    void setView(const View &view) { m_view = view; }
    void render(const ShaderState &ss, const View& view, float time, float depth);
    void update(uint step, float time);
    void clear();
    
};


#endif // _PARTICLES_H

