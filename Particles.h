
#ifndef _PARTICLES_H
#define _PARTICLES_H

#include "StdAfx.h"
#include "Graphics.h"

static const bool kBluryParticles = 1;

struct ShaderParticles;

struct IParticleShader : public ShaderProgramBase {

    virtual void UseProgram(const ShaderState& ss, const View& view, float time) const=0;
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
            //DASSERT(color != 0);
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
    IndexBuffer             m_ibo;
    VertexBuffer<Particle>  m_vbo;
    int                     m_addFirst = 0;
    int                     m_addPos = 0;
    uint                    m_lastMaxedStep = -1;
    float                   m_planeZ = 0.f;
    std::mutex              m_mutex;
    View                    m_view;
    vector<ParticleTrail>   m_trails;
    const IParticleShader  *m_program = NULL;
    
    void updateRange(uint first, uint size);

protected:

    uint                   m_simStep = 0;
    float                  m_simTime = 0.f;
    int                    m_maxParticles = 0;
    
    bool visible(const float3 &pos, float size) const
    {
        return forceVisible || m_view.intersectCircle(float3(pos.x, pos.y, pos.z + m_planeZ), 5.f * size + 100.f);
    }

    bool visible(const float2 &pos, float size) const
    {
        return forceVisible || m_view.intersectCircle(float3(pos.x, pos.y, m_planeZ), 5.f * size + 100.f);
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
    void render(const ShaderState &ss, const View& view, float time);
    void update(uint step, float time);
    void clear();
    
};


#endif // _PARTICLES_H

