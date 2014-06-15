#ifndef __VERTEXBUFFER_H
#define __VERTEXBUFFER_H

#include "tarray.h"
#include "gl/utility/gl_clock.h"
#include "gl/system/gl_interface.h"

struct vertex_t;
struct secplane_t;
struct subsector_t;
struct sector_t;


class FVertexBuffer
{
protected:
	unsigned int vbo_id;

public:
	FVertexBuffer();
	virtual ~FVertexBuffer();
	virtual void BindVBO() = 0;
};

struct FFlatVertex
{
	float x,z,y;	// world position
	float u,v;		// texture coordinates

	void SetFlatVertex(vertex_t *vt, const secplane_t &plane);
	void Set(float xx, float zz, float yy, float uu, float vv)
	{
		x = xx;
		z = zz;
		y = yy;
		u = uu;
		v = vv;
	}
};

#define VTO ((FFlatVertex*)NULL)


class FFlatVertexBuffer : public FVertexBuffer
{
	FFlatVertex *map;
	unsigned int mIndex;
	unsigned int mCurIndex;

	void CheckPlanes(sector_t *sector);

	const unsigned int BUFFER_SIZE = 2000000;

	void ImmRenderBuffer(unsigned int primtype, unsigned int offset, unsigned int count);

public:
	TArray<FFlatVertex> vbo_shadowdata;	// this is kept around for updating the actual (non-readable) buffer and as stand-in for pre GL 4.x

	FFlatVertexBuffer();
	~FFlatVertexBuffer();

	void CreateVBO();
	void BindVBO();
	void CheckUpdate(sector_t *sector);

	FFlatVertex *GetBuffer()
	{
		return &map[mCurIndex];
	}
	unsigned int GetCount(FFlatVertex *newptr, unsigned int *poffset)
	{

		unsigned int newofs = (unsigned int)(newptr - map);
		unsigned int diff = newofs - mCurIndex;
		*poffset = mCurIndex;
		mCurIndex = newofs;
		if (mCurIndex >= BUFFER_SIZE) mCurIndex = mIndex;
		return diff;
	}
#ifdef __GL_PCH_H	// we need the system includes for this but we cannot include them ourselves without creating #define clashes. The affected files wouldn't try to draw anyway.
	void RenderArray(unsigned int primtype, unsigned int offset, unsigned int count)
	{
		drawcalls.Clock();
		if (gl.flags & RFL_BUFFER_STORAGE)
		{
			glDrawArrays(primtype, offset, count);
		}
		else
		{
			ImmRenderBuffer(primtype, offset, count);
		}
		drawcalls.Unclock();
	}

	void RenderCurrent(FFlatVertex *newptr, unsigned int primtype, unsigned int *poffset = NULL, unsigned int *pcount = NULL)
	{
		unsigned int offset;
		unsigned int count = GetCount(newptr, &offset);
		RenderArray(primtype, offset, count);
		if (poffset) *poffset = offset;
		if (pcount) *pcount = count;
	}

#endif
	void Reset()
	{
		mCurIndex = mIndex;
	}

private:
	int CreateSubsectorVertices(subsector_t *sub, const secplane_t &plane, int floor);
	int CreateSectorVertices(sector_t *sec, const secplane_t &plane, int floor);
	int CreateVertices(int h, sector_t *sec, const secplane_t &plane, int floor);
	void CreateFlatVBO();
	void UpdatePlaneVertices(sector_t *sec, int plane);

};


struct FSkyVertex
{
	float x, y, z, u, v;
	PalEntry color;

	void Set(float xx, float zz, float yy, float uu=0, float vv=0, PalEntry col=0xffffffff)
	{
		x = xx;
		z = zz;
		y = yy;
		u = uu;
		v = vv;
		color = col;
	}

};

class FSkyVertexBuffer : public FVertexBuffer
{
public:
	static const int SKYHEMI_UPPER = 1;
	static const int SKYHEMI_LOWER = 2;

	enum
	{
		SKYMODE_MAINLAYER = 0,
		SKYMODE_SECONDLAYER = 1,
		SKYMODE_FOGLAYER = 2
	};

private:
	TArray<FSkyVertex> mVertices;
	TArray<unsigned int> mPrimStart;

	int mRows, mColumns;

	void SkyVertex(int r, int c, bool yflip);
	void CreateSkyHemisphere(int hemi);
	void CreateDome();
	void RenderRow(int prim, int row);

public:

	FSkyVertexBuffer();
	virtual ~FSkyVertexBuffer();
	virtual void BindVBO();
	void RenderDome(FMaterial *tex, int mode);

};

#define VSO ((FSkyVertex*)NULL)

struct FModelVertex
{
	float x, y, z;	// world position
	float u, v;		// texture coordinates

	void Set(float xx, float yy, float zz, float uu, float vv)
	{
		x = xx;
		y = yy;
		z = zz;
		u = uu;
		v = vv;
	}
};


class FModelVertexBuffer : public FVertexBuffer
{
	int mIndexFrame[2];

public:
	TArray<FModelVertex> vbo_shadowdata;	// this is kept around for interpolating on GL 2.0

	FModelVertexBuffer();
	~FModelVertexBuffer();

	void BindVBO();
	void UpdateBufferPointers(int frame1, int frame2);
};

#define VMO ((FModelVertex*)NULL)


#endif