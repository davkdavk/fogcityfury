// fcfcar: Fog City Fury's real Stallion GT mesh, drawn with the game's own car shader compiled for Xenos by xsc.
// A test title for the libxenon/xsdk port: it proves the game's geometry and its compiled car paint/lighting shader
// run on the 360 GPU. The car turns slowly; Back ends it. It reads a pixel back to check the car actually drew.

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "fcfcar_shaders.h"
#include "car_mesh.h"
#include "wheel_mesh.h"
#include "xsdk/gpu.h"
#include "xsdk/kernel.h"
#include "xsdk/log.h"
#include "xsdk/present.h"
#include "xsdk/shader.h"
#include "xsdk/texture.h"

enum {
	BUFFERS = 2, SWAP_INTERVAL = 1, BEGIN_TIMEOUT_MS = 5000, FENCE_TIMEOUT_MS = 1000,
	CLEAR_ARGB = 0xFF101418, VTX_STRIDE = 48, CUBE_SIZE = 8, CUBE_LEVELS = 4,
};

// The car vertex: pos3 (loc0), normal3 (loc1), colour3 (loc2), uv2 (loc3), material float (loc4).
// xsc routes vertex elements by location: 0=POSITION, 1=COLOR, 2+=TEXCOORD (xsc.md, Interface conventions).
static const XsdkVertexElement layout[] = {
	{ .usage = XSDK_USAGE_POSITION, .usage_index = 0, .format = XSDK_VERTEX_FLOAT3, .offset_bytes = 0 },
	{ .usage = XSDK_USAGE_COLOR,    .usage_index = 0, .format = XSDK_VERTEX_FLOAT3, .offset_bytes = 12 },
	{ .usage = XSDK_USAGE_TEXCOORD, .usage_index = 0, .format = XSDK_VERTEX_FLOAT3, .offset_bytes = 24 },
	{ .usage = XSDK_USAGE_TEXCOORD, .usage_index = 1, .format = XSDK_VERTEX_FLOAT2, .offset_bytes = 36 },
	{ .usage = XSDK_USAGE_TEXCOORD, .usage_index = 2, .format = XSDK_VERTEX_FLOAT1, .offset_bytes = 44 },
};

// ---- column-major mat4 helpers (as GLSL reads them from constants) ----
static void m_ident(float* m){ memset(m,0,64); m[0]=m[5]=m[10]=m[15]=1; }
static void m_mul(float* r, const float* a, const float* b){
	float t[16];
	for(int c=0;c<4;c++)for(int w=0;w<4;w++){ float s=0; for(int k=0;k<4;k++) s+=a[k*4+w]*b[c*4+k]; t[c*4+w]=s; }
	memcpy(r,t,64);
}
static void m_persp(float* m, float fovy, float asp, float n, float f){
	memset(m,0,64); float t=1.0f/tanf(fovy/2);
	m[0]=t/asp; m[5]=t; m[10]=f/(n-f); m[11]=-1; m[14]=n*f/(n-f); // Xenos clip z: 0 at near, 1 at far
}
static void m_lookat(float* m, const float* e, const float* at, const float* up){
	float fx=at[0]-e[0], fy=at[1]-e[1], fz=at[2]-e[2];
	float fl=sqrtf(fx*fx+fy*fy+fz*fz); fx/=fl; fy/=fl; fz/=fl;
	float sx=fy*up[2]-fz*up[1], sy=fz*up[0]-fx*up[2], sz=fx*up[1]-fy*up[0];
	float sl=sqrtf(sx*sx+sy*sy+sz*sz); sx/=sl; sy/=sl; sz/=sl;
	float ux=sy*fz-sz*fy, uy=sz*fx-sx*fz, uz=sx*fy-sy*fx;
	m_ident(m);
	m[0]=sx; m[4]=sy; m[8]=sz; m[1]=ux; m[5]=uy; m[9]=uz; m[2]=-fx; m[6]=-fy; m[10]=-fz;
	m[12]=-(sx*e[0]+sy*e[1]+sz*e[2]); m[13]=-(ux*e[0]+uy*e[1]+uz*e[2]); m[14]=fx*e[0]+fy*e[1]+fz*e[2];
}
static void m_roty(float* m, float a){ m_ident(m); float c=cosf(a), s=sinf(a); m[0]=c; m[8]=s; m[2]=-s; m[10]=c; }
static void m_translate(float* m,float x,float y,float z){ m_ident(m); m[12]=x; m[13]=y; m[14]=z; }
static void m_scale(float* m,float x,float y,float z){ m_ident(m); m[0]=x; m[5]=y; m[10]=z; }

static void gpu_copy(volatile uint32_t* dst, const uint32_t* src, uint32_t words){
	for(uint32_t i=0;i<words;i++) dst[i]=src[i];
}

// Solid-colour cube map for the car's reflections (uEnv). Six faces, each level.
static NTSTATUS make_env_cube(XsdkTexture* tex, volatile uint32_t** mem){
	NTSTATUS st = xsdk_texture_init_cube(tex, XSDK_FORMAT_A8R8G8B8, CUBE_SIZE, CUBE_LEVELS);
	if(!NT_SUCCESS(st)) return st;
	volatile uint32_t* p = xsdk_gpu_alloc(tex->size_bytes);
	if(!p) return 0xC0000001;
	xsdk_texture_attach(tex, p);
	uint32_t sky = 0xFF6080B0; // a calm blue-grey the paint can reflect
	for(uint32_t face=0; face<6; face++){
		uint32_t size=CUBE_SIZE;
		for(uint32_t lvl=0; lvl<CUBE_LEVELS; lvl++){
			uint32_t row[64]; for(uint32_t i=0;i<size;i++) row[i]=sky;
			for(uint32_t y=0;y<size;y++)
				xsdk_texture_upload_slice_rows(tex, lvl, face, y, 1, row, size*4);
			if(size>1) size/=2;
		}
	}
	xsdk_gpu_invalidate(tex->address, tex->size_bytes);
	*mem=p; return 0;
}

int main(void){
	xsdk_log_write("fcfcar: Fog City Fury Stallion GT with the game's Xenos car shader; Back ends");
	if(!NT_SUCCESS(xsdk_gpu_open())){ xsdk_log_write("fcfcar: gpu_open failed"); return 1; }

	XsdkShader vert={0}, frag={0};
	NTSTATUS st = xsdk_shader_load(&vert, car_vert.container, car_vert.container_bytes);
	if(NT_SUCCESS(st)) st = xsdk_shader_load(&frag, car_frag.container, car_frag.container_bytes);
	if(NT_SUCCESS(st)) st = xsdk_shader_set_vertex_layout(&vert, layout, 5, (const uint32_t[]){ VTX_STRIDE });
	xsdk_log_write("fcfcar: vertex_layout status 0x%08X",(unsigned)st);
	if(!NT_SUCCESS(st)){ xsdk_log_write("fcfcar: shader load/layout failed 0x%08X",(unsigned)st); return 1; }

	// mesh into GPU memory: words[0]=nv, words[1]=ni, then verts (12 words each), then indices
	const uint32_t nv=car_mesh_words[0], ni=car_mesh_words[1];
	const uint32_t vert_words=nv*12, idx_words=ni;
	volatile uint32_t* vmem = xsdk_gpu_alloc(vert_words*4);
	volatile uint32_t* imem = xsdk_gpu_alloc(idx_words*4);
	if(!vmem||!imem){ xsdk_log_write("fcfcar: gpu_alloc failed"); return 1; }
	gpu_copy(vmem, &car_mesh_words[2], vert_words);
	gpu_copy(imem, &car_mesh_words[2+vert_words], idx_words);
	xsdk_gpu_invalidate(xsdk_gpu_address(vmem), vert_words*4);
	xsdk_gpu_invalidate(xsdk_gpu_address(imem), idx_words*4);
	const uint32_t wnv=wheel_mesh_words[0], wni=wheel_mesh_words[1];
	const uint32_t wvert_words=wnv*12;
	volatile uint32_t* wvmem=xsdk_gpu_alloc(wvert_words*4);
	volatile uint32_t* wimem=xsdk_gpu_alloc(wni*4);
	if(!wvmem||!wimem){ xsdk_log_write("fcfcar: wheel alloc failed"); return 1; }
	gpu_copy(wvmem,&wheel_mesh_words[2],wvert_words);
	gpu_copy(wimem,&wheel_mesh_words[2+wvert_words],wni);
	xsdk_gpu_invalidate(xsdk_gpu_address(wvmem),wvert_words*4);
	xsdk_gpu_invalidate(xsdk_gpu_address(wimem),wni*4);

	XsdkTexture env; volatile uint32_t* envmem=NULL;
	if(!NT_SUCCESS(make_env_cube(&env,&envmem))){ xsdk_log_write("fcfcar: env cube failed"); return 1; }
	XsdkTexture shadow;
	xsdk_texture_create(&shadow, XSDK_FORMAT_A8R8G8B8, 4, 4, 1, true);
	{ uint32_t white[16]; for(int i=0;i<16;i++) white[i]=0xFFFFFFFF;
	  xsdk_texture_upload(&shadow,0,white,16); xsdk_gpu_invalidate(shadow.address, shadow.size_bytes); }

	const XsdkSampler samp = { .mag_filter=XSDK_FILTER_LINEAR, .min_filter=XSDK_FILTER_LINEAR, .mip_filter=XSDK_FILTER_LINEAR };
	XsdkSamplerState env_samp, shadow_samp;
	xsdk_texture_sampler_state(&env_samp,&samp); xsdk_texture_sampler_state(&shadow_samp,&samp);

	const XsdkDepthStencil depth = { .depth_test=true, .depth_write=true, .depth_compare=XSDK_COMPARE_LESS_EQUAL };
	XsdkDepthStencilState depth_state; xsdk_gpu_depth_stencil_state(&depth_state,&depth);

	uint32_t width=0,height=0;
	if(!NT_SUCCESS(xsdk_present_open(XSDK_DISPLAY_720P,BUFFERS,SWAP_INTERVAL,&width,&height))){
		xsdk_log_write("fcfcar: present_open failed"); return 1; }

	float cam[3]={5.2f,2.6f,-6.0f}, at[3]={0,0.7f,0}, up[3]={0,1,0};
	float view[16], proj[16], vp[16], model[16], vertconsts[32];
	m_lookat(view,cam,at,up); m_persp(proj,1.05f,(float)width/height,0.1f,100.0f); m_mul(vp,proj,view);

	// pixel constants: the car shader's uniform block, std140 offsets from its SPIR-V (18 registers)
	float pc[72]; memset(pc,0,sizeof pc);
	float sund[3]={0.4f,0.8f,0.5f}; float sl=sqrtf(sund[0]*sund[0]+sund[1]*sund[1]+sund[2]*sund[2]);
	pc[0]=sund[0]/sl; pc[1]=sund[1]/sl; pc[2]=sund[2]/sl;      // uSunDir @0
	pc[4]=1.3f; pc[5]=1.25f; pc[6]=1.1f;                        // uSunCol @16
	pc[8]=0.25f; pc[9]=0.4f; pc[10]=0.65f;                      // uSkyTop @32
	pc[12]=0.75f; pc[13]=0.8f; pc[14]=0.9f;                     // uSkyHor @48
	pc[16]=cam[0]; pc[17]=cam[1]; pc[18]=cam[2];                // uCamPos @64
	pc[21]=0.0f;                                                // uHDR @84: tonemap in shader -> LDR
	// uLightVP @192: maps everything to clip z>1 so shadow() returns 1 without sampling
	memset(&pc[48],0,64); pc[48+14]=3.0f; pc[48+15]=1.0f;
	pc[64]=0.75f; pc[65]=0.03f; pc[66]=0.03f;                   // uPaint @256: red

	uint32_t passed=0, failed=0;
	for(uint32_t frame=0;;frame++){
		INPUT_STATE input={0};
		if(XamInputGetState(0,0,&input)==ERROR_SUCCESS && input.gamepad.buttons & INPUT_GAMEPAD_BACK) break;
		float ang = frame*0.01f;
		m_roty(model,ang);
		memcpy(&vertconsts[0],vp,64); memcpy(&vertconsts[16],model,64);
		pc[19]=frame*0.016f; // uTime @76

		uint32_t address;
		if(!NT_SUCCESS(xsdk_present_begin(&address,BEGIN_TIMEOUT_MS))){ xsdk_log_write("fcfcar: present_begin failed"); break; }
		xsdk_gpu_set_target(width,height);
		xsdk_gpu_set_depth_stencil(&depth_state);
		xsdk_gpu_set_shaders(&vert,&frag);
		xsdk_built_shader_set_literals(&car_vert);
		xsdk_built_shader_set_literals(&car_frag);
		xsdk_gpu_set_vertex_constants(0,vertconsts,8);
		xsdk_gpu_set_pixel_constants(0,pc,18);
		xsdk_gpu_set_texture(0,&env,&env_samp);
		xsdk_gpu_set_texture(1,&shadow,&shadow_samp);
		xsdk_gpu_set_vertices(0,xsdk_gpu_address(vmem),vert_words*4);
		xsdk_gpu_draw_indexed(XSDK_PRIMITIVE_TRIANGLE_LIST,xsdk_gpu_address(imem),ni,true);
		for(int w=0;w<4;w++){
			float x=(w&1?1.0f:-1.0f)*0.86f, z=(w<2?1.45f:-1.40f);
			float T[16],Sc[16],WM[16],tmp[16];
			m_translate(T,x,0.36f,z); m_scale(Sc,0.16f,0.36f,0.36f);
			m_mul(tmp,T,Sc); m_mul(WM,model,tmp);
			memcpy(&vertconsts[16],WM,64);
			xsdk_gpu_set_vertex_constants(0,vertconsts,8);
			xsdk_gpu_set_vertices(0,xsdk_gpu_address(wvmem),wvert_words*4);
			xsdk_gpu_draw_indexed(XSDK_PRIMITIVE_TRIANGLE_LIST,xsdk_gpu_address(wimem),wni,true);
		}
		xsdk_gpu_resolve(address,width,height,CLEAR_ARGB);

		if(frame==2){
			xsdk_gpu_wait(xsdk_gpu_fence(),FENCE_TIMEOUT_MS);
			XsdkTexture fb; uint32_t red=0, nonclear=0;
			if(NT_SUCCESS(xsdk_texture_init(&fb,XSDK_FORMAT_A8R8G8B8,width,height,1,true))){
				xsdk_texture_attach(&fb,xsdk_present_pixels(address));
				uint32_t row[1280];
				const uint32_t ROWS=27, COLS=48;
				for(uint32_t ry=0; ry<ROWS; ry++){
					uint32_t y=(ry*height)/ROWS;
					xsdk_texture_download_rows(&fb,0,y,1,row,sizeof row);
					char line[COLS+1];
					for(uint32_t cx=0; cx<COLS; cx++){
						uint32_t x=(cx*width)/COLS;
						uint32_t p=__builtin_bswap32(row[x]);
						uint32_t r=(p>>16)&255, g=(p>>8)&255, b=p&255;
						if(p!=CLEAR_ARGB) nonclear++;
						if(r>0x35 && r>g+g/2 && r>b+b/2){ red++; line[cx]='R'; }
						else if(p==CLEAR_ARGB) line[cx]=' ';
						else if(r+g+b>0x180) line[cx]='#';
						else line[cx]='.';
					}
					line[COLS]=0;
					xsdk_log_write("fcfcar: |%s|", line);
				}
			}
			bool ok = red>20 && nonclear>100;
			xsdk_log_write("fcfcar: %s red-paint pixels %u, non-clear %u", ok?"PASS":"FAIL",(unsigned)red,(unsigned)nonclear);
			passed+=ok; failed+=!ok;
		}
		xsdk_present_end();
	}
	xsdk_log_write("fcfcar: checks %u passed %u failed",(unsigned)passed,(unsigned)failed);
	xsdk_present_close();
	xsdk_gpu_free(vmem); xsdk_gpu_free(imem); xsdk_gpu_free(wvmem); xsdk_gpu_free(wimem); xsdk_gpu_free(envmem);
	xsdk_texture_free(&shadow); xsdk_shader_free(&frag); xsdk_shader_free(&vert);
	xsdk_gpu_close();
	xsdk_log_write("fcfcar: done");
	return 0;
}
