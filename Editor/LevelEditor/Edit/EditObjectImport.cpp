//----------------------------------------------------
// file: CEditableObjectImport.cpp
//----------------------------------------------------

#include "stdafx.h"
#pragma hdrstop

#include "EditObject.h"
#include "lwo2.h"
#include "LW_SHADERDEF.h"
#include "EditMesh.h"
#include "UI_Main.h"
#include "Texture.h"
#include "Shader.h"
#include "xr_trims.h"

extern "C" __declspec(dllimport) lwObject* LWO_ImportObject(char* filename, lwObject *new_obj);
extern "C" __declspec(dllimport) void LWO_CloseFile(lwObject *new_obj);

DEFINE_MAP(void*,int,VMIndexLink,VMIndexLinkIt);

bool CompareFunc(st_VMapPt& vm0, st_VMapPt& vm1){
	return vm0.vmap_index<vm1.vmap_index;
};

bool CEditableObject::Import_LWO(const char* fn, bool bNeedOptimize){
	lwObject *I=0;
    UI.SetStatus("Importing...");
	UI.ProgressStart(100,"Read file:");
	UI.ProgressUpdate(1);
    char fname[MAX_PATH];
    strcpy(fname,fn);
	I=LWO_ImportObject(fname,I);
	UI.ProgressUpdate(100);
	if (I){
        bool bResult=true;
        ELog.Msg( mtInformation, "CEditableObject: import lwo %s...", fname );

        // parse lwo object
        {
        	m_Meshes.reserve	(I->nlayers);
            m_Surfaces.reserve	(I->nsurfs);

            // surfaces
            st_lwSurface* Isf=0;
            {
                int i=0;
                UI.ProgressStart(I->nsurfs,"Check surf:");
                for (Isf=I->surf; Isf; Isf=Isf->next){
                    UI.ProgressUpdate(i);
                    Isf->alpha_mode=i; // ���������� ��� ���������� ����� !!!
                    CSurface* Osf = new CSurface();
                    m_Surfaces.push_back(Osf);
                    if (Isf->name) Osf->SetName(Isf->name); else Osf->SetName("Default");
                    Osf->Set2Sided((Isf->sideflags==3)?TRUE:FALSE);
                    AnsiString en_shader="default", lc_shader="default";
                    XRShader* sh_info = 0;
                    if (Isf->nshaders&&(stricmp(Isf->shader->name,SH_PLUGIN_NAME)==0)){
                    	sh_info 	= (XRShader*)Isf->shader->data;
                        en_shader 	= sh_info->en_name;
                        lc_shader 	= sh_info->lc_name;
                    }else
						ELog.Msg(mtError,"CEditableObject: Shader not found on surface '%s'.",Osf->_Name());

                    if (!Device.Shader._FindBlender(en_shader.c_str())){
						ELog.Msg(mtError,"CEditableObject: Render shader '%s' - can't find in library.\nUsing 'default' shader on surface '%s'.", en_shader.c_str(), Osf->_Name());
	                    en_shader = "default";
                    }
                    if (!Device.ShaderXRLC.Get(lc_shader.c_str())){
						ELog.Msg(mtError,"CEditableObject: Compiler shader '%s' - can't find in library.\nUsing 'default' shader on surface '%s'.", lc_shader.c_str(), Osf->_Name());
	                    lc_shader = "default";
                    }
                    // fill texture layers
                    int cidx;
                    st_lwClip* Icl;
                    DWORD dwNumTextures=0;
                    for (st_lwTexture* Itx=Isf->color.tex; Itx; Itx=Itx->next){
                        char tname[1024]="";
                        dwNumTextures++;
                        cidx = -1;
                        if (Itx->type==ID_IMAP) cidx=Itx->param.imap.cindex;
                        else{
                            ELog.DlgMsg(mtError, "Import LWO (Surface '%s'): 'Texture' is not Image Map!",Osf->_Name());
                            bResult=false;
                            break;
                        }
                        if (cidx!=-1){
                            // get textures
                            for (Icl=I->clip; Icl; Icl=Icl->next)
                                if ((cidx==Icl->index)&&(Icl->type==ID_STIL)){
                                    strcpy(tname,Icl->source.still.name);
                                    break;
                                }
                            if (tname[0]==0){
                                ELog.DlgMsg(mtError, "Import LWO (Surface '%s'): 'Texture' name is empty or non 'STIL' type!",Osf->_Name());
                                bResult=false;
                                break;
                            }
                            char tex_name[_MAX_FNAME];
                            _splitpath( tname, 0, 0, tex_name, 0 );
							Osf->SetTexture(Engine.FS.UpdateTextureNameWithFolder(tex_name));
                            // get vmap refs
                            Osf->SetVMap(Itx->param.imap.vmap_name);
                        }
                    }
                    if (!bResult) break;
                    if (!Osf->_Texture()||!Osf->_Texture()[0]){
						ELog.DlgMsg(mtError, "Can't create shader. Invalid surface '%s'. Textures empty.",Osf->_Name());
                        bResult = false;
                        break;
                    }

                    Osf->SetShader(en_shader.c_str(),Device.Shader.Create(en_shader.c_str(),Osf->_Texture()));
					Osf->SetShaderXRLC(lc_shader.c_str());

                    Osf->SetFVF(D3DFVF_XYZ|D3DFVF_NORMAL|(dwNumTextures<<D3DFVF_TEXCOUNT_SHIFT));
                    i++;
                }
		    }
			if (bResult){
                // mesh layers
            	st_lwLayer* Ilr=0;
	            int k=0;
 	           	for (Ilr=I->layer; Ilr; Ilr=Ilr->next){
                    // create new mesh
                    CEditableMesh* MESH=new CEditableMesh(this);
                    m_Meshes.push_back(MESH);

                    if (Ilr->name)	strcpy(MESH->m_Name,Ilr->name); else strcpy(MESH->m_Name,"");
                    MESH->m_Box.set(Ilr->bbox[0],Ilr->bbox[1],Ilr->bbox[2], Ilr->bbox[3],Ilr->bbox[4],Ilr->bbox[5]);

                    // parse mesh(lwo-layer) data
                    // vmaps
                    st_lwVMap* Ivmap=0;
                    int vmap_count=0;
                    if (Ilr->nvmaps==0){
                        ELog.DlgMsg(mtError, "Import LWO: Mesh layer must contain UV!");
                        bResult=false;
                        break;
                    }

                    // ������� ������������ ������������� ���
					static VMIndexLink VMIndices;
				    VMIndices.clear();

                    for (Ivmap=Ilr->vmap; Ivmap; Ivmap=Ivmap->next){
                    	switch(Ivmap->type){
                        case ID_TXUV:{
                            if (Ivmap->dim!=2){
                                ELog.DlgMsg(mtError, "Import LWO: 'UV Map' must contain 2 value!");
                                bResult=false;
                                break;
                            }
                            MESH->m_VMaps.push_back(new st_VMap(Ivmap->name,vmtUV,Ivmap->perpoly));
                            st_VMap* Mvmap=MESH->m_VMaps.back();
                            int vcnt=Ivmap->nverts;
                            // VMap
                            Mvmap->copyfrom(*Ivmap->val,vcnt);
                            // flip uv
                            for (int k=0; k<Mvmap->size(); k++){
                            	Fvector2& uv = Mvmap->getUV(k);
                                uv.y=1.f-uv.y;
                            }
                            // vmap index
                            VMIndices[Ivmap] = vmap_count++;
                        }break;
						case ID_WGHT:{
                            if (Ivmap->dim!=1){
                                ELog.DlgMsg(mtError, "Import LWO: 'Weight' must contain 1 value!");
                                bResult=false;
                                break;
                            }
                            MESH->m_VMaps.push_back(new st_VMap(Ivmap->name,vmtWeight,FALSE));
                            st_VMap* Mvmap=MESH->m_VMaps.back();
                            int vcnt=Ivmap->nverts;
                            // VMap
                            Mvmap->copyfrom(*Ivmap->val,vcnt);
                            // vmap index
                            VMIndices[Ivmap] = vmap_count++;
                        }break;
						case ID_PICK: ELog.Msg(mtError,"Found 'PICK' VMAP. Import failed."); bResult = false; break;
						case ID_MNVW: ELog.Msg(mtError,"Found 'MNVW' VMAP. Import failed."); bResult = false; break;
						case ID_MORF: ELog.Msg(mtError,"Found 'MORF' VMAP. Import failed."); bResult = false; break;
						case ID_SPOT: ELog.Msg(mtError,"Found 'SPOT' VMAP. Import failed."); bResult = false; break;
						case ID_RGB:  ELog.Msg(mtError,"Found 'RGB' VMAP. Import failed.");  bResult = false; break;
						case ID_RGBA: ELog.Msg(mtError,"Found 'RGBA' VMAP. Import failed."); bResult = false; break;
                        }
	                    if (!bResult) break;
                    }
                    if (!bResult) break;
                    // points
					UI.ProgressStart(Ilr->point.count,"Fill points:");
                    {
                        MESH->m_Points.resize(Ilr->point.count);
                        MESH->m_Adjs.resize(Ilr->point.count);
	                    int id = Ilr->polygon.count/50;
	                    if (id==0) id = 1;
                        for (int i=0; i<Ilr->point.count; i++){
	                    	if ((i%id)==0) UI.ProgressUpdate(i);
                            st_lwPoint& Ipt = Ilr->point.pt[i];
                            Fvector& Mpt	= MESH->m_Points[i];
                            INTVec& a_lst	= MESH->m_Adjs[i];
                            Mpt.set			(Ipt.pos);
                            copy			(Ipt.pol,Ipt.pol+Ipt.npols,inserter(a_lst,a_lst.begin()));
                            sort			(a_lst.begin(),a_lst.end());
                        }
                    }
                    if (!bResult) break;
                    // polygons
					UI.ProgressStart(Ilr->polygon.count,"Fill polygons:");
                    MESH->m_Faces.resize(Ilr->polygon.count);
                    MESH->m_VMRefs.reserve(Ilr->polygon.count*3);
                    INTVec surf_ids;
                    surf_ids.resize(Ilr->polygon.count);
                    int id = Ilr->polygon.count/50;
                    if (id==0) id = 1;
                    for (int i=0; i<Ilr->polygon.count; i++){
                    	if ((i%id)==0) UI.ProgressUpdate(i);
                        st_Face&		Mpol=MESH->m_Faces[i];
                        st_lwPolygon&   Ipol=Ilr->polygon.pol[i];
                        if (Ipol.nverts!=3) {
							ELog.DlgMsg(mtError, "Import LWO: Face must contain only 3 vertices!");
                        	bResult=false;
                            break;
                        }
                        for (int pv_i=0; pv_i<3; pv_i++){
                        	st_lwPolVert& 	Ipv=Ipol.v[pv_i];
                            st_FaceVert&  	Mpv=Mpol.pv[pv_i];
                            Mpv.pindex		=Ipv.index;

							MESH->m_VMRefs.push_back(VMapPtSVec());
							VMapPtSVec&	vm_lst = MESH->m_VMRefs.back();
							Mpv.vmref 		= MESH->m_VMRefs.size()-1;

							// parse uv-map
							int vmpl_cnt		=Ipv.nvmaps;
							st_lwPoint& Ipt 	=Ilr->point.pt[Mpv.pindex];
                            int vmpt_cnt		=Ipt.nvmaps;
                            if (!vmpl_cnt&&!vmpt_cnt){
                                ELog.DlgMsg	(mtError,"Found mesh without UV's!",0);
                                bResult		= false;
                                break;
                            }

                            AStringVec names;
							if (vmpl_cnt){
                            	// ����� �� poly
    							for (int vm_i=0; vm_i<vmpl_cnt; vm_i++){
									if (Ipv.vm[vm_i].vmap->type!=ID_TXUV) continue;
									vm_lst.push_back(st_VMapPt());
									st_VMapPt& pt	= vm_lst.back();
        							pt.vmap_index	= VMIndices[Ipv.vm[vm_i].vmap];// ����� ���� VMap
                                    names.push_back	(Ipv.vm[vm_i].vmap->name);
            						pt.index 		= Ipv.vm[vm_i].index;
                				}
							}
                            if (vmpt_cnt){
                            	// ����� �� points
                                for (int vm_i=0; vm_i<vmpt_cnt; vm_i++){
									if (Ipt.vm[vm_i].vmap->type!=ID_TXUV) continue;
                                    if (find(names.begin(),names.end(),Ipt.vm[vm_i].vmap->name)!=names.end()) continue;
									vm_lst.push_back(st_VMapPt());
									st_VMapPt& pt	= vm_lst.back();
									pt.vmap_index	= VMIndices[Ipt.vm[vm_i].vmap]; // ����� ���� VMap
									pt.index 		= Ipt.vm[vm_i].index;
                                }
							}

                            sort(vm_lst.begin(),vm_lst.end(),CompareFunc);

							// parse weight-map
                            int vm_cnt		=Ipt.nvmaps;
                            for (int vm_i=0; vm_i<vm_cnt; vm_i++){
								if (Ipt.vm[vm_i].vmap->type!=ID_WGHT) continue;
								vm_lst.push_back(st_VMapPt());
								st_VMapPt& pt	= vm_lst.back();
        	                    pt.vmap_index	= VMIndices[Ipt.vm[vm_i].vmap]; // ����� ���� VMap
            	                pt.index 		= Ipt.vm[vm_i].index;
                            }
                        }
                        if (!bResult) break;
						// Ipol.surf->alpha_mode - ��������� ��� ����� ����� surface
                        surf_ids[i]			= Ipol.surf->alpha_mode;
                    }
                    if (!bResult) break;
					int p_idx=0;
                    for (FaceIt pl_it=MESH->m_Faces.begin(); pl_it!=MESH->m_Faces.end(); pl_it++){
						MESH->m_SurfFaces[m_Surfaces[surf_ids[p_idx]]].push_back(p_idx);
                        p_idx++;
                    }
                    if (!bResult) break;
                    k++;
                    //MESH->DumpAdjacency();
                    if (bNeedOptimize) MESH->Optimize(false);
                    //MESH->DumpAdjacency();
                    MESH->RebuildVMaps(); // !!!!!!
                }
    		}
        }
        LWO_CloseFile(I);
		UI.ProgressEnd();
	    UI.SetStatus("");
    	if (bResult) 	VerifyMeshNames();
        else			ELog.DlgMsg(mtError,"Can't parse LWO object.");
		if (bResult)	m_LoadName = ChangeFileExt(fname,".object");
        return bResult;
    }else
		ELog.DlgMsg(mtError,"Can't import LWO object file.");
	UI.ProgressEnd();
	UI.SetStatus("");
    return false;
}


