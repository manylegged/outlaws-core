
#include "StdAfx.h"
#include "Shaders.h"
#include "Particles.h"

#ifndef ASSERT_UPDATE_THREAD
#define ASSERT_UPDATE_THREAD()
#endif

static DEFINE_CVAR(int, kMinParticles, 1<<15);
static DEFINE_CVAR(int, kMinQueuedParticles, 2000);
// static DEFINE_CVAR(int, kMaxQueuedParticles, 20000);

// 1 2
// 0 3
static const uint kParticleVerts = 4;
static const f2 kParticleOffsets[] = {f2(0, 0), f2(0, 1), f2(1, 1), f2(1, 0) };
static const uint kParticleIndexes[] = {0,1,2, 0,2,3};
static DEFINE_CVAR(bool, kParticleTris, false);

size_t ParticleSystem::count() const
{
    return m_vertices.size() / m_particle_verts; 
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
    
    m_vertices.resize(m_particle_verts * particles.size());

    int i = 0;
    foreach (const Particle &pr, particles)
    {
        const int vert = i * m_particle_verts;
        m_vertices[vert] = pr;
        m_vertices[vert].offset = float3(0, 0, pr.offset.x);
        for (uint j=1; j<m_particle_verts; j++) {
            m_vertices[vert + j] = pr;
            m_vertices[vert + j].offset = f3(kParticleOffsets[j], pr.offset.x);
        }
        i++;
    }
    
    m_addFirst = 0;
    m_addPos = m_vertices.size();
}

void ParticleSystem::addTrail(const ParticleTrail& p)
{
    // std::lock_guard<std::mutex> l(m_add_mutex);
    m_trails.push_back(p); 
}

void ParticleSystem::add(const Particle &p)
{
    std::lock_guard<std::mutex> l(m_add_mutex);
    if (m_queued_particles.size() >= kMinQueuedParticles -1)
    {
        for_ (pt, m_queued_particles)
            if (!alloc(pt))
                break;
        m_queued_particles.clear();
    }
    m_queued_particles.push_back(p);
}

bool ParticleSystem::alloc(const Particle &p)
{
    // wait 3 steps before trying again if particle buffer is full
    if (m_maxParticles == 0 || (m_simStep < m_lastMaxedStep + 3))
        return false;

    const bool gradient = (p.offset.y == 0.f);

    int end = m_addPos - m_particle_verts;
    const int vsize = m_vertices.size();
    if (end < 0)
        end += vsize;
    if (vsize == 0)
        end = m_addPos;
    for (; m_addPos != end; m_addPos = (m_addPos + m_particle_verts) % vsize)
    {
        if (m_vertices[m_addPos].endTime > m_simTime)
            continue;
        if (m_particle_verts == 1)
        {
            m_vertices[m_addPos] = p;
            m_vertices[m_addPos].offset.y = gradient ? 0 : 1;
        }
        else
        {
            for (uint j=0; j<m_particle_verts; j++) {
                Particle &v = m_vertices[m_addPos + j];
                v = p;
                v.offset = f3(kParticleOffsets[j], p.offset.x);
                // v.position += f3((v.offset - f2(0.5f)) * p.offset.x, 0);
                if (!gradient)
                    v.offset.y += 10;
            }
        }
            
        m_addPos = (m_addPos + m_particle_verts) % vsize;
        return true;
    }

    // drop particles if we have too many
    // FIXME maybe we could put them in a temp buffer, sort, and drop smallest?
    if (count() >= m_maxParticles)
    {
        m_lastMaxedStep = m_simStep;
        return false;
    }

    // else grow
    m_addPos = vsize;
    {
        std::lock_guard<std::mutex> l(m_mutex);
        const int size = clamp((int)vsize * 2, kMinParticles * m_particle_verts, m_maxParticles * m_particle_verts);
        DPRINT(SHADER, ("Changing particle count from %.2e to %.2e", (double) vsize, (double) size));
        m_vertices.resize(size);
    }

    return alloc(p);
}

void ParticleSystem::clear()
{
    m_vertices.clear();
    m_ibo.clear();
    m_vbo.clear();
    m_addFirst      = -1;
    m_addPos        = 0;
    m_lastMaxedStep = 0;
    m_trails.clear();
    m_particle_verts = kParticleTris ? kParticleVerts : 1;
}


ParticleSystem::ParticleSystem()
{
    clear();
    m_vertices.resize(kMinParticles * m_particle_verts);
    m_queued_particles.reserve(kMinQueuedParticles);
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
    // GLint ToPixels;

    bool tri_version = false;

    void LoadTheProgram()
    {
        tri_version = kParticleTris;
        m_header = str_format("#define USE_TRIS %d", (int)kParticleTris);
        m_argstr = str_format("%d", (int)kParticleTris);
        LoadProgram("ShaderParticlePoints");
        offset      = getAttribLocation("Offset");
        startTime   = getAttribLocation("StartTime");
        endTime     = getAttribLocation("EndTime");
        velocity    = getAttribLocation("Velocity");
        color       = getAttribLocation("Color");
        currentTime = getUniformLocation("CurrentTime");
        // GET_UNIF_LOC(ToPixels);
    }

    typedef ParticleSystem::Particle Particle;

    void UseProgram(const ShaderState& ss, const View& view, float time) const
    {
        const Particle* ptr = NULL;
        UseProgramBase(ss, &ptr->position, ptr);

        vertexAttribPointer(offset, &ptr->offset, ptr);
        vertexAttribPointer(startTime, &ptr->startTime, ptr);
        vertexAttribPointer(endTime, &ptr->endTime, ptr);
        vertexAttribPointer(velocity, &ptr->velocity, ptr);
        vertexAttribPointer(color, &ptr->color, ptr);

        glUniform1f(currentTime, time);
        glReportError();
    }

};

void ShaderParticlesInstance()
{
    ShaderParticles::instance();
}


void ParticleSystem::shrink_to_fit()
{
    {
        std::lock_guard<std::mutex> l(m_mutex);
        clear();
        m_vertices.shrink_to_fit();
    }

    std::lock_guard<std::mutex> l(m_add_mutex);
    m_queued_particles.shrink_to_fit();
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
    
    // number of particles decreased / render type changed
	const int verts = kParticleTris ? kParticleVerts : 1;
    if (count() > m_maxParticles || verts != m_particle_verts)
    {
        std::lock_guard<std::mutex> l(m_mutex);
        m_vertices.resize(m_maxParticles * m_particle_verts);
        m_particle_verts = verts;
        m_addPos = 0;
        m_addFirst = -1;
    }

    std::lock_guard<std::mutex> l(m_add_mutex);
    for (uint i=0; i<m_trails.size(); )
    {
        ParticleTrail& tr = m_trails[i];

        if (vec_pop_increment(m_trails, i, tr.endTime < m_simTime))
            continue;

        if ((float) m_simTime - tr.lastParticleTime <= 1.f / tr.rate)
            continue;
        
        const float  vln = length(tr.velocity);
        const float  phi = (vln * ((float)m_simTime - tr.startTime)) / tr.arcRadius;
        const float2 rad = tr.arcRadius * rotate90(tr.velocity / vln);
        const float3 pos = tr.position + float3(rotate(rad, phi) - rad, 0.f);
            
        // const float2 pos = tr.position + tr.velocity * ((float)m_simTime - tr.startTime);
        if (!visible(pos, 1000.f))
            continue;

        if (m_queued_particles.size() >= kMinQueuedParticles -1)
            break;
                
        Particle pr  = tr.particle;
        const float v = ((float)m_simTime - tr.startTime) / (tr.endTime - tr.startTime);

        pr.position  = pos;
        pr.velocity  = float3(rotate(float2(lerp(pr.velocity, tr.particle1.velocity, v)),
                                     randangle()), 0.f);
        pr.startTime = m_simTime;
        pr.endTime   = m_simTime + lerp(pr.endTime, tr.particle1.endTime, v);
        pr.offset    = lerp(pr.offset, tr.particle1.offset, v);
        pr.color     = lerpAXXX(pr.color, tr.particle1.color, v);

        m_queued_particles.push_back(pr);
        // add(pr);

        tr.lastParticleTime = m_simTime;
    }
}

void ParticleSystem::render(const ShaderState &ss, const View& view, float time)
{
    if ((m_vertices.empty() && m_queued_particles.empty()) || m_maxParticles == 0)
    {
        if (m_vbo.size())
            clear();
        return;
    }

    {
        std::lock_guard<std::mutex> l(m_add_mutex);
        for_ (pt, m_queued_particles)
            alloc(pt);
        m_queued_particles.clear();
    }
    
    // send particle data to gpu
    const int addPos = m_addPos;
    if (m_addFirst != addPos)
    {
        std::lock_guard<std::mutex> l(m_mutex);

        if (m_vertices.size() == m_vbo.size() && m_addFirst >= 0)
        {
            // assuming we didn't add more than a whole buffer of particles
            if (addPos < m_addFirst)
            {
                updateRange(0, addPos);
                updateRange(m_addFirst, m_vertices.size() - m_addFirst);
            }
            else
            {
                updateRange(m_addFirst, addPos - m_addFirst);
            }
        }
        else
        {
            if (ShaderParticles::instance().tri_version != kParticleTris)
                const_cast<ShaderParticles&>(ShaderParticles::instance()).ReloadProgram();
            
            if (m_particle_verts > 1)
            {
                const uint polys       = m_vertices.size() / m_particle_verts;
                const uint vertsPerPoly = arraySize(kParticleIndexes);

                vector<uint> indices(vertsPerPoly * polys);

                for (uint i=0; i<polys; i++)
                {
                    for (int j=0; j<vertsPerPoly; j++)
                        indices[i * vertsPerPoly + j] = i * m_particle_verts + kParticleIndexes[j];
                }
            
                m_ibo.BufferData(indices, GL_STATIC_DRAW);
            }
            m_vbo.BufferData(m_vertices, GL_DYNAMIC_DRAW);
        }
        m_addFirst = addPos;
    }
    
    if (m_vbo.empty())
        return;

    if (!m_program)
    {
        m_program = &ShaderParticles::instance();
    }

    // make sure to glEnable(GL_PROGRAM_POINT_SIZE); or glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    m_vbo.Bind();
    m_program->UseProgram(ss, view, time);
    if (m_particle_verts == 1)
        ss.DrawArrays(GL_POINTS, m_vbo.size());
    else
        ss.DrawElements(GL_TRIANGLES, m_ibo);
    m_program->UnuseProgram();
    m_vbo.Unbind();
}

