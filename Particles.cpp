
#include "StdAfx.h"
#include "Shaders.h"
#include "Particles.h"

#ifndef ASSERT_UPDATE_THREAD
#define ASSERT_UPDATE_THREAD()
#endif

static const uint kParticleEdges = 6;
static const uint kParticleVerts = kParticleEdges + kBluryParticles;
static const uint kMinParticles = 1<<13;

size_t ParticleSystem::count() const
{
    return m_vertices.size() / kParticleVerts; 
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

void ParticleSystem::add(const Particle &p, bool gradient)
{
    //ASSERT_UPDATE_THREAD();
    if (m_maxParticles == 0 || (m_lastMaxedStep == globals.simStep))
        return;

    int end = m_addPos - kParticleVerts;
    if (end < 0)
        end += m_vertices.size();
    if (m_vertices.size())
    {
        for (; m_addPos != end; m_addPos = (m_addPos + kParticleVerts) % m_vertices.size())
        {
            if (m_vertices[m_addPos].endTime <= globals.simTime)
            {
                Particle p1 = p;
                p1.angle += M_TAOf / (2.f * kParticleEdges);

                if (kBluryParticles)
                {
                    // center
                    m_vertices[m_addPos] = p1;
                    m_vertices[m_addPos].offset = float2(0.f);
                    for (uint j=1; j<kParticleVerts; j++) {
                        m_vertices[m_addPos + j] = p1;
                        m_vertices[m_addPos + j].offset = p.offset.x * getCircleVertOffset<kParticleEdges>(j-1);
                        if (gradient)
                            m_vertices[m_addPos + j].color  = 0x0;
                    }
                }
                else
                {
                    for (uint j=0; j<kParticleVerts; j++) {
                        m_vertices[m_addPos + j] = p;
                        m_vertices[m_addPos + j].offset = p.offset.x * getCircleVertOffset<kParticleVerts>(j);
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
        m_lastMaxedStep = globals.simStep;
        return;
    }

    m_addPos = m_vertices.size();
    {
        std::lock_guard<std::mutex> l(m_mutex);
        const uint size = max(kMinParticles * kParticleVerts, (uint) m_vertices.size() * 2);
        DPRINT(SHADER, ("Changing particle count from %d to %d", (int) m_vertices.size(), size));
        m_vertices.resize(size);
    }

    add(p, true);
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

struct ShaderParticles : public IShader {

    GLint offset;
    GLint startTime;
    GLint endTime;
    GLint velocity;
    GLint angle;
    GLint color;
    GLint currentTime;
    
    ShaderParticles()
    {
        LoadProgram("ShaderParticles",
                    "varying vec4 DestinationColor;\n"
                    ,
                    "attribute vec2  Offset;\n"
                    "attribute float StartTime;\n"
                    "attribute float EndTime;\n"
                    "attribute vec3  Velocity;\n"
                    "attribute float Angle;\n"
                    "attribute vec4  Color;\n"
                    "uniform   float CurrentTime;\n"
                    "void main(void) {\n"
                    "    if (CurrentTime >= EndTime) {\n"
                    "        gl_Position = vec4(0.0, 0.0, -99999999.0, 1);\n"
                    "        return;\n"
                    "    }\n"
                    "    float deltaT = CurrentTime - StartTime;\n"
                    "    vec2  rot    = vec2(cos(Angle), sin(Angle));\n"
                    "    vec3  offset = vec3(rot.x * Offset.x - rot.y * Offset.y,\n"
                    "                        rot.y * Offset.x + rot.x * Offset.y, 0);\n"
                    "    vec3  velocity = pow(0.8, deltaT) * Velocity;\n"
                    "    vec3  position = Position.xyz + offset + deltaT * velocity;\n"
                    "    float v = deltaT / (EndTime - StartTime);\n"
                    "    DestinationColor = (1.0 - v) * Color;\n"
                    "    gl_Position = Transform * vec4(position, 1);\n"
                    "}\n"
                    ,
                    "void main(void) {\n"
                    "    gl_FragColor = DestinationColor;\n"
                    "}\n"
                    );
        offset      = getAttribLocation("Offset");
        startTime   = getAttribLocation("StartTime");
        endTime     = getAttribLocation("EndTime");
        velocity    = getAttribLocation("Velocity");
        angle       = getAttribLocation("Angle");
        color       = getAttribLocation("Color");
        currentTime = getUniformLocation("CurrentTime");
        glReportError();
    }

    typedef ParticleSystem::Particle Particle;

    void UseProgram(const ShaderState& ss)
    {
        const Particle* ptr = NULL;
        UseProgramBase(ss, &ptr->position, ptr);

        vertexAttribPointer(offset, &ptr->offset, ptr);
        vertexAttribPointer(startTime, &ptr->startTime, ptr);
        vertexAttribPointer(endTime, &ptr->endTime, ptr);
        vertexAttribPointer(velocity, &ptr->velocity, ptr);
        vertexAttribPointer(angle, &ptr->angle, ptr);
        vertexAttribPointer(color, &ptr->color, ptr);

        glUniform1f(currentTime, OLG_GetRenderSimTime());
        glReportError();
    }

    static ShaderParticles& instance()
    {
        ShaderParticles *p = new ShaderParticles;
        return *p;
    }
};


void ParticleSystem::updateRange(uint first, uint size)
{
    m_vbo.BufferSubData(first, size, &m_vertices[first]);
}

void ParticleSystem::update()
{
    ASSERT_UPDATE_THREAD();
    
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

        if (tr.endTime < globals.simTime) {
            vector_remove_index(m_trails, i);
            continue;
        }

        if ((float) globals.simTime - tr.lastParticleTime > 1.f / tr.rate)
        {
            const float3 pos = tr.position + tr.velocity * ((float)globals.simTime - tr.startTime);
            if (visible(pos, 1000.f))
            {
                Particle pr  = tr.particle;
                const float v = ((float)globals.simTime - tr.startTime) / (tr.endTime - tr.startTime);

                pr.position  = pos;
                pr.angle     = randangle();
                pr.velocity  = float3(rotate(float2(lerp(pr.velocity, tr.particle1.velocity, v)),
                                             randangle()), 0.f);
                pr.startTime = globals.simTime;
                pr.endTime   = globals.simTime + lerp(pr.endTime, tr.particle1.endTime, v);
                pr.offset    = lerp(pr.offset, tr.particle1.offset, v);
                pr.color     = lerpAXXX(pr.color, tr.particle1.color, v);
            
                add(pr, true);

                tr.lastParticleTime = globals.simTime;
            }
        }

        i++;
    }
}

void ParticleSystem::render(float3 origin, const ShaderState &ss, const View& view)
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
            m_vbo.BufferData(m_vertices, GL_DYNAMIC_DRAW);
        }
        m_addFirst = m_addPos;
    }

    if (!m_program)
        m_program = &ShaderParticles::instance(); 

    m_vbo.Bind();
    m_program->UseProgram(ss);
    ss.DrawElements(GL_TRIANGLES, m_ibo);
    m_program->UnuseProgram();
    m_vbo.Unbind();
}

