#include "stdafx.h"
#pragma hdrstop

#include "EditObject.h"
#include "EditMesh.h"
#include "motion.h"
#include "bone.h"
#include "ExportSkeleton.h"
#include "ExportObjectOGF.h"
#include "d3dutils.h"
#include "ui_main.h"
#include "render.h"
#include "PropertiesListHelper.h"
#include "ResourceManager.h"
#include "ImageManager.h"

const float tex_w	= LOD_SAMPLE_COUNT*LOD_IMAGE_SIZE;
const float tex_h	= 1*LOD_IMAGE_SIZE;
const float half_p_x= 0.5f*(1.f/tex_w);
const float half_p_y= 0.5f*(1.f/tex_h);
const float offs_x 	= 1.f/tex_w;
const float offs_y 	= 1.f/tex_h;

static Fvector LOD_pos[4]={
	{-1.0f+offs_x, 1.0f-offs_y, 0.0f},
	{ 1.0f-offs_x, 1.0f-offs_y, 0.0f},
	{ 1.0f-offs_x,-1.0f+offs_y, 0.0f},
	{-1.0f+offs_x,-1.0f+offs_y, 0.0f}
};
static FVF::LIT LOD[4]={
	{{-1.0f, 1.0f, 0.0f},  0xFFFFFFFF, {0.0f,0.0f}}, // F 0
	{{ 1.0f, 1.0f, 0.0f},  0xFFFFFFFF, {0.0f,0.0f}}, // F 1
	{{ 1.0f,-1.0f, 0.0f},  0xFFFFFFFF, {0.0f,0.0f}}, // F 2
	{{-1.0f,-1.0f, 0.0f},  0xFFFFFFFF, {0.0f,0.0f}}, // F 3
};

bool CEditableObject::Reload()
{
	ClearGeometry();
    return Load(m_LoadName.c_str());
}

bool CEditableObject::RayPick(float& dist, const Fvector& S, const Fvector& D, const Fmatrix& inv_parent, SRayPickInfo* pinf)
{
	bool picked = false;
    for(EditMeshIt m = m_Meshes.begin();m!=m_Meshes.end();m++)
        if( (*m)->RayPick( dist, S, D, inv_parent, pinf ) )
            picked = true;
	return picked;
}

void CEditableObject::RayQuery(SPickQuery& pinf)
{
    for(EditMeshIt m = m_Meshes.begin();m!=m_Meshes.end();m++)
        (*m)->RayQuery(pinf);
}

void CEditableObject::RayQuery(const Fmatrix& parent, const Fmatrix& inv_parent, SPickQuery& pinf)
{
    for(EditMeshIt m = m_Meshes.begin();m!=m_Meshes.end();m++)
        (*m)->RayQuery(parent, inv_parent, pinf);
}

void CEditableObject::BoxQuery(const Fmatrix& parent, const Fmatrix& inv_parent, SPickQuery& pinf)
{
    for(EditMeshIt m = m_Meshes.begin();m!=m_Meshes.end();m++)
        (*m)->BoxQuery(parent, inv_parent, pinf);
}

#ifdef _EDITOR
bool CEditableObject::FrustumPick(const CFrustum& frustum, const Fmatrix& parent){
	for(EditMeshIt m = m_Meshes.begin();m!=m_Meshes.end();m++)
		if((*m)->FrustumPick(frustum, parent))	return true;
	return false;
}

bool CEditableObject::BoxPick(CCustomObject* obj, const Fbox& box, const Fmatrix& inv_parent, SBoxPickInfoVec& pinf){
	bool picked = false;
    for(EditMeshIt m = m_Meshes.begin();m!=m_Meshes.end();m++)
        if ((*m)->BoxPick(box, inv_parent, pinf)){
        	pinf.back().s_obj = obj;
            picked = true;
        }
	return picked;
}
#endif

extern float 	ssaLIMIT;
extern float	g_fSCREEN;
static const float ssaLim = 64.f*64.f/(640*480);
void CEditableObject::Render(const Fmatrix& parent, int priority, bool strictB2F){
    if (!(m_LoadState.is(LS_RBUFFERS))) DefferedLoadRP();

	Fvector v; float r;
    Fbox bb; bb.xform(m_Box,parent); bb.getsphere(v,r);

    if (EPrefs->object_flags.is(epoDrawLOD)&&(m_Flags.is(eoUsingLOD)&&(CalcSSA(v,r)<ssaLim))){
		if ((1==priority)&&(true==strictB2F)) RenderLOD(parent);
    }else{
        RCache.set_xform_world	(parent);
        if (m_Flags.is(eoHOM)){
            if ((1==priority)&&(false==strictB2F)) 	RenderEdge		(parent,0,0,0x40B64646);
            if ((2==priority)&&(true==strictB2F))	RenderSelection	(parent,0,0,0xA0FFFFFF);
        }else if (m_Flags.is(eoSoundOccluder)){
            if ((1==priority)&&(false==strictB2F))	RenderEdge		(parent,0,0,0xFF000000);
            if ((2==priority)&&(true==strictB2F))	RenderSelection	(parent,0,0,0xA00000FF);
        }else{
            if(psDeviceFlags.is(rsEdgedFaces)&&(1==priority)&&(false==strictB2F))
                RenderEdge(parent);

            for(SurfaceIt s_it=m_Surfaces.begin(); s_it!=m_Surfaces.end(); s_it++){
                if ((priority==(*s_it)->_Priority())&&(strictB2F==(*s_it)->_StrictB2F())){
                    Device.SetShader((*s_it)->_Shader());
                    for (EditMeshIt _M=m_Meshes.begin(); _M!=m_Meshes.end(); _M++)
                        if (IsSkeleton()) 	(*_M)->RenderSkeleton	(parent,*s_it);
                        else				(*_M)->Render			(parent,*s_it);
                }
            }
        }
    }
}

void CEditableObject::RenderSingle(const Fmatrix& parent)
{
	for (int i=0; i<4; i++){
		Render(parent, i, false);
		Render(parent, i, true);
    }
}

void CEditableObject::RenderAnimation(const Fmatrix&){
}

void CEditableObject::RenderEdge(const Fmatrix& parent, CEditableMesh* mesh, CSurface* surf, u32 color)
{
    if (!(m_LoadState.is(LS_RBUFFERS))) DefferedLoadRP();

    Device.SetShader(Device.m_WireShader);
    if(mesh) mesh->RenderEdge(parent, surf, color);
    else for(EditMeshIt _M = m_Meshes.begin();_M!=m_Meshes.end();_M++)
            (*_M)->RenderEdge(parent, surf, color);
}

void CEditableObject::RenderSelection(const Fmatrix& parent, CEditableMesh* mesh, CSurface* surf, u32 color)
{
    if (!(m_LoadState.is(LS_RBUFFERS))) DefferedLoadRP();

    RCache.set_xform_world(parent);
    Device.SetShader(Device.m_SelectionShader);
    Device.RenderNearer(0.0005);
    if(mesh) mesh->RenderSelection(parent, surf, color);
    else for(EditMeshIt _M = m_Meshes.begin();_M!=m_Meshes.end();_M++)
         	(*_M)->RenderSelection(parent, surf, color);
    Device.ResetNearer();
}

IC static void CalculateLODTC(int frame, int w_cnt, int h_cnt, Fvector2& lt, Fvector2& rb)
{
	Fvector2	ts;
    ts.set		(1.f/(float)w_cnt,1.f/(float)h_cnt);
    lt.x        = (frame%w_cnt+0)*ts.x+half_p_x;
    lt.y        = (frame/w_cnt+0)*ts.y+half_p_y;
    rb.x        = (frame%w_cnt+1)*ts.x-half_p_x;
    rb.y        = (frame/w_cnt+1)*ts.y-half_p_y;
}

void CEditableObject::GetLODFrame(int frame, Fvector p[4], Fvector2 t[4], const Fmatrix* parent)
{
	R_ASSERT(m_Flags.is(eoUsingLOD));
    Fvector P,S;
    m_Box.get_CD(P,S);
    float r = _max(S.x,S.z);//sqrtf(S.x*S.x+S.z*S.z);
    Fmatrix T,matrix,rot;
    T.scale(r,S.y,r);
    T.translate_over(P);
    if (parent) T.mulA_43(*parent);

    float angle = frame*(PI_MUL_2/float(LOD_SAMPLE_COUNT));
    rot.rotateY(-angle);
    matrix.mul(T,rot);
    Fvector2 lt, rb;
    CalculateLODTC(frame,LOD_SAMPLE_COUNT,1,lt,rb);
    t[0].set(lt);
    t[1].set(rb.x,lt.y);
    t[2].set(rb);
    t[3].set(lt.x,rb.y);
    matrix.transform_tiny(p[0],LOD_pos[0]);
    matrix.transform_tiny(p[1],LOD_pos[1]);
    matrix.transform_tiny(p[2],LOD_pos[2]);
    matrix.transform_tiny(p[3],LOD_pos[3]);
}

void CEditableObject::RenderLOD(const Fmatrix& parent)
{
    Fvector C;
    C.sub			(parent.c,Device.m_Camera.GetPosition()); C.y = 0;
    float m 		= C.magnitude();
    if (m<EPS) return;
    C.div			(m);
	int max_frame;
    float max_dot	= 0;
    Fvector HPB;
    parent.getHPB	(HPB);

    for (int frame=0; frame<LOD_SAMPLE_COUNT; frame++){
    	float angle = angle_normalize(frame*(PI_MUL_2/float(LOD_SAMPLE_COUNT))+HPB.x);

        Fvector D;
        D.setHP(angle,0);
        float dot = C.dotproduct(D);
        if (dot<0.7072f) continue;

        if (dot>max_dot){
        	max_dot = dot;
            max_frame = frame;
        }
    }
	{
    	Fvector    	p[4];
        Fvector2 	t[4];
    	GetLODFrame(max_frame,p,t);
        for (int i=0; i<4; i++){ LOD[i].p.set(p[i]); LOD[i].t.set(t[i]); }
    	RCache.set_xform_world(parent);
        Device.SetShader		(m_LODShader?m_LODShader:Device.m_WireShader);
    	DU.DrawPrimitiveLIT	(D3DPT_TRIANGLEFAN, 2, LOD, 4, true, false);
    }
}

xr_string CEditableObject::GetLODTextureName()
{
    string512 nm; 	strcpy	(nm,m_LibName.c_str()); _ChangeSymbol(nm,'\\','_');
	xr_string 	l_name;
    l_name 			= xr_string("lod_")+nm;
    return ImageLib.UpdateFileName(l_name);
}

void CEditableObject::OnDeviceCreate()
{
}

void CEditableObject::OnDeviceDestroy()
{
	DefferedUnloadRP();
}

void CEditableObject::DefferedLoadRP()
{
	if (m_LoadState.is(LS_RBUFFERS)) return;

    // skeleton
	if (IsSkeleton())
		vs_SkeletonGeom.create(FVF_SV,RCache.Vertex.Buffer(),RCache.Index.Buffer());

//*/
	// ������� LOD shader
	xr_string l_name = GetLODTextureName();
    xr_string fname = xr_string(l_name)+xr_string(".dds");
    m_LODShader.destroy();
//    if (FS.exist(_game_textures_,fname.c_str()))
    if (m_Flags.is(eoUsingLOD))
    	m_LODShader.create(GetLODShaderName(),l_name.c_str());
    m_LoadState.set(LS_RBUFFERS,TRUE);
}
void CEditableObject::DefferedUnloadRP()
{
	if (!(m_LoadState.is(LS_RBUFFERS))) return;
    // skeleton
	vs_SkeletonGeom.destroy();
    // ������� ������
	for (EditMeshIt _M=m_Meshes.begin(); _M!=m_Meshes.end(); _M++)
    	if (*_M) (*_M)->GenerateRenderBuffers();
	// ������� shaders
    for(SurfaceIt s_it=m_Surfaces.begin(); s_it!=m_Surfaces.end(); s_it++)
        (*s_it)->OnDeviceDestroy();
    // LOD
    m_LODShader.destroy();
    m_LoadState.set(LS_RBUFFERS,FALSE);
}
void CEditableObject::EvictObject()
{
	EditMeshIt m 				= m_Meshes.begin();
	for(;m!=m_Meshes.end();m++){
    	(*m)->UnloadCForm		();
    	(*m)->UnloadVNormals	(true);
        (*m)->UnloadSVertices	(true);
        (*m)->UnloadFNormals	(true);
    }
    DefferedUnloadRP			();
}

bool CEditableObject::PrepareOGF(IWriter& F, u8 infl, bool gen_tb, CEditableMesh* mesh)
{
	return IsSkeleton()?PrepareSkeletonOGF(F,infl):PrepareRigidOGF(F,gen_tb,mesh);
}

bool CEditableObject::PrepareRigidOGF(IWriter& F, bool gen_tb, CEditableMesh* mesh)
{
    CExportObjectOGF E(this);
    return E.Export(F,gen_tb,mesh);
}

bool CEditableObject::PrepareSVGeometry(IWriter& F, u8 infl)
{
    CExportSkeleton E(this);
    return E.ExportGeometry(F, infl);
}

bool CEditableObject::PrepareSVKeys(IWriter& F)
{
    CExportSkeleton E(this);
    return E.ExportMotionKeys(F);
}

bool CEditableObject::PrepareSVDefs(IWriter& F)
{
    CExportSkeleton E(this);
    return E.ExportMotionDefs(F);
}

bool CEditableObject::PrepareSkeletonOGF(IWriter& F, u8 infl)
{
    CExportSkeleton E(this);
    return E.Export(F,infl);
}

bool CEditableObject::PrepareOMF(IWriter& F)
{
    CExportSkeleton E(this);
    return E.ExportMotions(F);
}
//---------------------------------------------------------------------------

void __fastcall CEditableObject::OnChangeTransform(PropValue*)
{
	UI->RedrawScene();
}
//---------------------------------------------------------------------------

#include "Blender.h"
IC BOOL BE      (BOOL A, BOOL B)
{
    bool a = !!A;
    bool b = !!B;
    return a==b;
}
bool CEditableObject::CheckShaderCompatible()
{
	bool bRes 			= true;
	for(SurfaceIt s_it=m_Surfaces.begin(); s_it!=m_Surfaces.end(); s_it++)
    {
    	IBlender* 		B = Device.Resources->_FindBlender(*(*s_it)->m_ShaderName); 
        Shader_xrLC* 	C = Device.ShaderXRLC.Get(*(*s_it)->m_ShaderXRLCName);
        if (!B||!C){
        	ELog.Msg	(mtError,"Object '%s': invalid or missing shader [E:'%s', C:'%s']",GetName(),(*s_it)->_ShaderName(),(*s_it)->_ShaderXRLCName());
            bRes 		= false;
        }else{
            if (!BE(B->canBeLMAPped(),!C->flags.bLIGHT_Vertex)){ 
                ELog.Msg	(mtError,"Object '%s': engine shader '%s' non compatible with compiler shader '%s'",GetName(),(*s_it)->_ShaderName(),(*s_it)->_ShaderXRLCName());
                bRes 		= false;
            }
        }
    }
    return bRes;
}
//---------------------------------------------------------------------------


