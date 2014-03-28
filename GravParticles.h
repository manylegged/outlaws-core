
#ifndef GRAVPARTICLES_H
#define GRAVPARTICLES_H

#include "Graphics.h"

struct GravityParticleSystem {
private:
    struct Particle {
        float  startTime = 0.f;
        float  endTime   = 0.f;
        float2 position;
        float2 velocity;
        float  radius;
    };

    vector<Particle>            m_particles;
    TriMesh<VertexPosColorLuma> m_mesh;
    float2                      m_attractorLastPos;
    float2                      m_attractorLastVel;
    int                         m_addPos = 0;
    int                         m_liveCount = 0;

    bool update(float frameTime);

    float2 m_emitterBoundingRadius;

public:
    // properties
    float lifeTime      = 3.f;
    float fadeTime      = 0.5f;
    float attraction    = 500000000.f;
    float attrDamping   = 0.01f;
    float damping       = 0.4f;
    float vel0          = 600.f;
    float minVel        = 0.f;
    uint  edges         = 5;    // sides of particle polygon
    float size          = 10.f;
    float attractorSize = 200.f; // no gravity within this radius
    float emitterSize   = 100.f;
    float timeFactor    = 0.125f;
    float maxDist       = 500;  // kill particles farther than this from the attractor
    float minDist       = 10.f; // kill partciels closer that this to the attractor
    bool  autoemit      = true;
    uint  color         = 0xffffff;
    float palpha        = 1.f;  // alpha to premultiply color
    float alpha         = 1.f;  // alpha for blending
    float luma          = 1.f;

    // autoemitter details
    float2 emitterPos;
    float2 emitterVel;

    // attractor
    float2 attractorPos;

    GravityParticleSystem();
    void render(const ShaderState &ss, float frametime);
    void render(TriMesh<VertexPosColorLuma>& mesH, float frametime);

    void resize(int size);

    bool emit(float2 pos, float2 vel, float radius);

    AABBox getBBox() const
    {
        AABBox bb;
        bb.insertRect(emitterPos, m_emitterBoundingRadius);
        return bb;
    }
};


#endif // GRAVPARTICLES_H
