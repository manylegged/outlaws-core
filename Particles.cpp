
#include "StdAfx.h"
#include "Shaders.h"
#include "Particles.h"

#ifndef ASSERT_UPDATE_THREAD
#define ASSERT_UPDATE_THREAD()
#endif

static const uint kParticleEdges = 6;
static const uint kParticleVerts = 1;
 // static const uint kParticleVerts = kParticleEdges + kBluryParticles;
static const uint kMinParticles = 1<<15;

size_t ParticleSystem::count() const
{
    return m_vertices.size() / kParticleVerts; 
}

void ParticleSystem::setTime(Particle &p, float t)
{
    // particles created during simulation, not during render
    p.startTime = m_simTime;
    p.endTime   = m_simTime + max((float)globals.simTimeStep, t);
}

void ParticleSystem::setParticles(vector<Particle>& particles)
{
    std::lock_guard<std::mutex> l(m_mutex);
    
    m_vertices.resize(kParticleVerts * particles.size());

    int i = 0;
    foreach (const Particle &pr, particles)
    {
        const int vert = i * kParticleVerts;
        m_vertices[vert] = pr;
        m_vertices[vert].offset = float2(0.f);
        for (uint j=1; j<kParticleVerts; j++) {
            m_vertices[vert + j] = pr;
            m_vertices[vert + j].offset = pr.offset.x * getCircleVertOffset<kParticleEdges>(j-1);
            m_vertices[vert + j].color  = 0x0;
        }
        i++;
    }
    
    m_addFirst = 0;
    m_addPos = m_vertices.size();
}

void ParticleSystem::add(const Particle &p, float angle, bool gradient)
{
    //ASSERT_UPDATE_THREAD();
    if (m_maxParticles == 0 || (m_lastMaxedStep == m_simStep))
        return;

    int end = m_addPos - kParticleVerts;
    if (end < 0)
        end += m_vertices.size();
    if (m_vertices.size())
    {
        for (; m_addPos != end; m_addPos = (m_addPos + kParticleVerts) % m_vertices.size())
        {
            if (m_vertices[m_addPos].endTime <= m_simTime)
            {
                if (kParticleVerts == 1)
                {
                    m_vertices[m_addPos] = p;
                    m_vertices[m_addPos].offset.y = gradient ? 0 : kParticleEdges;
                }
                else if (kBluryParticles)
                {
                    // center
                    const float2 rot = angleToVector(angle + M_TAOf / (2.f * kParticleEdges));
                    m_vertices[m_addPos] = p;
                    m_vertices[m_addPos].offset = float2(0.f);
                    for (uint j=1; j<kParticleVerts; j++) {
                        m_vertices[m_addPos + j] = p;
                        m_vertices[m_addPos + j].offset = rotate(p.offset.x * getCircleVertOffset<kParticleEdges>(j-1), rot);
                        if (gradient)
                            m_vertices[m_addPos + j].color  = 0x0;
                    }
                }
                else
                {
                    const float2 rot = angleToVector(angle + M_TAOf / (2.f * kParticleEdges));
                    for (uint j=0; j<kParticleVerts; j++) {
                        m_vertices[m_addPos + j] = p;
                        m_vertices[m_addPos + j].offset = rotate(p.offset.x * getCircleVertOffset<kParticleVerts>(j), rot);
                    }
                }
            
                m_addPos = (m_addPos + kParticleVerts) % m_vertices.size();
                return;
            }
        }
    }

    // drop particles if we have too many
    // FIXME maybe we could put them in a temp buffer, sort, and drop smallest?
    if (count() >= m_maxParticles)
    {
        m_lastMaxedStep = m_simStep;
        return;
    }

    m_addPos = m_vertices.size();
    {
        std::lock_guard<std::mutex> l(m_mutex);
        const uint size = max(kMinParticles * kParticleVerts, (uint) m_vertices.size() * 2);
        DPRINT(SHADER, ("Changing particle count from %.2e to %.2e", (double) m_vertices.size(), (double) size));
        m_vertices.resize(size);
    }

    add(p, angle, gradient);
}

void ParticleSystem::clear()
{
    m_vertices.clear();
    m_ibo.clear();
    m_vbo.clear();
    m_addFirst      = ~0;
    m_addPos        = 0;
    m_lastMaxedStep = 0;
    m_planeZ        = 0;
    m_trails.clear();
}


ParticleSystem::ParticleSystem()
{
    clear();
    m_vertices.resize(kMinParticles * kParticleVerts);
}

ParticleSystem::~ParticleSystem()
{
}

struct ShaderParticles : public IParticleShader, public ShaderBase<ShaderParticles> {

    GLint offset;
    GLint startTime;
    GLint endTime;
    GLint velocity;
    GLint angle;
    GLint color;
    GLint currentTime;
    GLint ToPixels;

    void LoadTheProgram()
    {
        if (kParticleVerts == 1)
            LoadProgram("ShaderParticlePoints");
        else
            LoadProgram("ShaderParticles");
        offset      = getAttribLocation("Offset");
        startTime   = getAttribLocation("StartTime");
        endTime     = getAttribLocation("EndTime");
        velocity    = getAttribLocation("Velocity");
        color       = getAttribLocation("Color");
        currentTime = getUniformLocation("CurrentTime");
        GET_UNIF_LOC(ToPixels);
    }

    typedef ParticleSystem::Particle Particle;

    void UseProgram(const ShaderState& ss, const View& view, float depth, float time) const
    {
        const Particle* ptr = NULL;
        UseProgramBase(ss, &ptr->position, ptr);

        vertexAttribPointer(offset, &ptr->offset, ptr);
        vertexAttribPointer(startTime, &ptr->startTime, ptr);
        vertexAttribPointer(endTime, &ptr->endTime, ptr);
        vertexAttribPointer(velocity, &ptr->velocity, ptr);
        vertexAttribPointer(color, &ptr->color, ptr);

        glUniform1f(currentTime, time);
        if (kParticleVerts == 1)
            glUniform1f(ToPixels, view.getWorldPointSizeInPixels(depth));
        glReportError();
    }

};

void ShaderParticlesInstance()
{
    ShaderParticles::instance();
}


void ParticleSystem::updateRange(uint first, uint size)
{
    m_vbo.BufferSubData(first, size, &m_vertices[first]);
}

void ParticleSystem::update(uint step, float time)
{
    ASSERT_UPDATE_THREAD();
    m_simTime = time;
    m_simStep = step;
    
    // number of particles decreased
    if (count() > m_maxParticles)
    {
        m_vertices.resize(m_maxParticles * kParticleVerts);
        m_addPos = 0;
        m_addFirst = 0;
    }

    for (uint i=0; i<m_trails.size(); )
    {
        ParticleTrail& tr = m_trails[i];

        if (vec_pop_increment(m_trails, i, tr.endTime < m_simTime))
            continue;

        if ((float) m_simTime - tr.lastParticleTime > 1.f / tr.rate)
        {
            const float  vln = length(tr.velocity);
            const float  phi = (vln * ((float)m_simTime - tr.startTime)) / tr.arcRadius;
            const float2 rad = tr.arcRadius * rotate90(tr.velocity / vln);
            const float3 pos = tr.position + float3(rotate(rad, phi) - rad, 0.f);
            
            // const float2 pos = tr.position + tr.velocity * ((float)m_simTime - tr.startTime);
            if (visible(pos, 1000.f))
            {
                Particle pr  = tr.particle;
                const float v = ((float)m_simTime - tr.startTime) / (tr.endTime - tr.startTime);

                pr.position  = pos;
                pr.velocity  = float3(rotate(float2(lerp(pr.velocity, tr.particle1.velocity, v)),
                                             randangle()), 0.f);
                pr.startTime = m_simTime;
                pr.endTime   = m_simTime + lerp(pr.endTime, tr.particle1.endTime, v);
                pr.offset    = lerp(pr.offset, tr.particle1.offset, v);
                pr.color     = lerpAXXX(pr.color, tr.particle1.color, v);
            
                add(pr, randangle(), true);

                tr.lastParticleTime = m_simTime;
            }
        }
    }
}

void ParticleSystem::render(float3 origin, float time, const ShaderState &ss, const View& view)
{
	m_planeZ = origin.z;
    m_view   = view;

    if (m_vertices.size() == 0 || m_maxParticles == 0)
    {
        if (m_vbo.size())
            clear();
        return;
    }

    // send particle data to gpu
    if (m_addFirst != m_addPos)
    {
        std::lock_guard<std::mutex> l(m_mutex);

        if (m_vertices.size() == m_vbo.size())
        {
            // assuming we didn't add more than a whole buffer of particles
            if (m_addPos < m_addFirst)
            {
                updateRange(0, m_addPos);
                updateRange(m_addFirst, m_vertices.size() - m_addFirst);
            }
            else
            {
                updateRange(m_addFirst, m_addPos - m_addFirst);
            }
        }
        else
        {
            if (kParticleVerts != 1)
            {
                const uint polys       = m_vertices.size() / kParticleVerts;
                const uint vertPerPoly = 3 * (kParticleVerts-1);

                vector<uint> indices(vertPerPoly * polys);

                for (uint i=0; i<polys; i++)
                {
                    uint* idxptr = &indices[i * vertPerPoly];
                    const uint start = i * kParticleVerts;

                    if (kBluryParticles)
                    {
                        for (uint j=1; j<kParticleVerts; j++) {
                            *idxptr++ = start;
                            *idxptr++ = start + j;
                            *idxptr++ = start + (j % (kParticleVerts-1)) + 1;
                        }
                    }
                    else
                    {
                        for (uint j=2; j<kParticleVerts; j++) {
                            *idxptr++ = start;
                            *idxptr++ = start + j - 1;
                            *idxptr++ = start + j;                    
                        }
                    }
                }
            
                m_ibo.BufferData(indices, GL_STATIC_DRAW);
            }
            m_vbo.BufferData(m_vertices, GL_DYNAMIC_DRAW);
        }
        m_addFirst = m_addPos;
    }

    if (!m_program)
    {
        m_program = &ShaderParticles::instance();
    }

    // make sure to glEnable(GL_PROGRAM_POINT_SIZE); or glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    m_vbo.Bind();
    m_program->UseProgram(ss, view, m_planeZ, time);
    if (kParticleVerts == 1)
        ss.DrawArrays(GL_POINTS, m_vbo.size());
    else
        ss.DrawElements(GL_TRIANGLES, m_ibo);
    m_program->UnuseProgram();
    m_vbo.Unbind();
}

