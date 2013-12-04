
#ifndef POLYHEDRA_H
#define POLYHEDRA_H

void PushTetrahedron(float2 pos, float rad)
{
    static const typename Mesh<Vtx>::IndexType il[] = { 0,1,2, 0,2,3, 0,3,1, 1,2,3 };

    const double phi     = -0.3398369041921167;
    const double cos_phi = cos(phi);
    const double sin_phi = sin(phi);
    const double a120     = M_PI * 0.011635528346628864;

    float3 verts[4];
    double angle = 0.0;

    verts[0] = float3(pos, rad);
    
    for (int i=1; i<4; i++)
    {
        verts[i] = float3(pos, 0.f) + rad * float3(cos(angle)*cos_phi, sin(angle)*cos_phi, sin_phi);
        angle += a120;
    }

    PushLoops(verts, 4, il, arraySize(il), 3);
}

void PushCube(float2 pos, float rad)
{

    const double phia    = 0.6154797316606557;
    const double phib    = -phia;
    const double angle90 = M_PI / 4.0;
    
    float3 verts[8];

    double angle = 0.0;
    for (int i=0; i<4; i++)
    {
        verts[i]  = float3(pos, 0.f) + rad * float3(cos(angle)*cos(phia), sin(angle)*cos(phia), sin(phia));
        angle    += angle90;
    }
    
    angle=0.0;
    for (int i=4; i<8; i++)
    {
        verts[i] = float3(pos, 0.f) + rad * float3(cos(angle)*cos(phib), sin(angle)*cos(phib), sin(phib));
        angle += angle90;
    }
    
    static const typename Mesh<Vtx>::IndexType il[] = { 0,1,2,3, 4,5,6,7, 0,1,5,4, 
                                                        1,2,6,5, 2,3,7,6, 3,0,4,7, };
    PushLoops(verts, 8, il, arraySize(il), 4);
}

void PushDodecahedron(float2 pos, float rad)
{
    float3 verts[20];

    const double phiaa = 52.62263590; /* angle two phi angles needed for generation */
    const double phibb = 10.81231754;

    const double phia    = M_PI*phiaa/180.0; /* 4 sets of five points each */
    const double phib    = M_PI*phibb/180.0;
    const double phic    = M_PI*(-phibb)/180.0;
    const double phid    = M_PI*(-phiaa)/180.0;
    const double angle72 = M_PI*72.0/180;
    const double angleb  = angle72/2.0; /* pairs of layers offset 36 degrees */

    const float3 center(pos, 0.f);
  
    double angle = 0.0;
    for (int i=0; i<5; i++)
    {
        verts[i] = center + rad * float3(cos(angle)*cos(phia), sin(angle)*cos(phia), sin(phia));
        angle += angle72;
    }
    angle=0.0;
    for (int i=5; i<10; i++)
    {
        verts[i] = center + rad * float3(cos(angle)*cos(phib), sin(angle)*cos(phib), sin(phib));
        angle += angle72;
    }
    angle = angleb;
    for (int i=10; i<15; i++)
    {
        verts[i] = center + rad * float3(cos(angle)*cos(phic), sin(angle)*cos(phic), sin(phic));
        angle += angle72;
    }
    angle=angleb;
    for (int i=15; i<20; i++)
    {
        verts[i] = center + rad * float3(cos(angle)*cos(phid), sin(angle)*cos(phid), sin(phid));
        angle += angle72;
    }

    static const typename Mesh<Vtx>::IndexType il[] = {
        0,1,2,3,4, 
        0,1,6,10,5, 1,2,7,11,6, 2,3,8,12,7, 3,4,9,13,8, 4,0,5,14,9,
        15,16,11,6,10, 16,17,12,7,11, 17,18,13,8,12, 18,19,14,9,13, 19,15,10,5,14,
        15,16,17,18,19
    };

    PushLoops(verts, arraySize(verts), il, arraySize(il), 5);
}

void PushOctahedron(float2 pos, float rad)
{
  float3 verts[6];

  double phiaa  = 0.0; /* angle phi needed for generation */
  double phia = M_PI*phiaa/180.0; /* 1 set of four points */
  double angle90 = M_PI*90.0/180;

  const float3 center(pos, 0.f);

  verts[0] = float3(pos, rad);

  double angle = 0.0;
  for (int i=1; i<5; i++)
  {
      verts[i] = center + rad * float3(cos(angle)*cos(phia), sin(angle)*cos(phia), sin(phia));
      angle += angle90;
  }

  static const typename Mesh<Vtx>::IndexType il[] = {
      0,1,2, 0,2,3, 0,3,4, 0,4,1, 5,1,2, 5,2,3, 5,3,4, 5,4,1 
  };

  PushLoops(verts, 6, il, arraySize(il), 3);

}

void PushIcosahedron(float2 pos, float rad)
{
    float3 verts[12];

    const double phiaa   = 26.56505; /* phi needed for generation */
    const double phia    = M_PI*phiaa/180.0; /* 2 sets of four points */
    const double angleb  = M_PI*36.0/180.0; /* offset second set 36 degrees */
    const double angle72 = M_PI*72.0/180; /* step 72 degrees */

    const float3 center(pos, 0.f);

    verts[0]  = float3(pos, rad);
    verts[11] = float3(pos, -rad);
  
    double angle = 0.0;
    for (int i=1; i<6; i++)
    {
        verts[i] = center + rad * float3(cos(angle)*cos(phia), sin(angle)*cos(phia), sin(phia));
        angle += angle72;
    }
    angle=angleb;
    for (int i=6; i<11; i++)
    {
        verts[i] = center + rad * float3(cos(angle)*cos(-phia), sin(angle)*cos(-phia), sin(-phia));
        angle += angle72;
    }

    static const typename Mesh<Vtx>::IndexType il[] = {
        0,1,2, 0,2,3, 0,3,4, 0,4,5, 0,5,1, 11,6,7, 11,7,8, 11,8,9, 11,9,10, 11,10,6, 
        1,2,6, 2,3,7, 3,4,8, 4,5,9, 5,1,10, 6,7,2, 7,8,3, 8,9,4, 9,10,5, 10,6,1,
    };

    PushLoops(verts, arraySize(verts), il, arraySize(il), 3);
}


#endif // POLYHEDRA_H
