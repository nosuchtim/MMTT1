#ifndef _SPRITE_H
#define _SPRITE_H

// Note - this Params class is different from PaletteParams and RegionParams because
// it doesn't need the Set/Increment/Toggle methods.

class SpriteParams {

public:

	// The constructor for SpriteParams takes:
	//   rp - the region parameter values
	//   op - the parameter values which override region-specific parameters
	//   ro - the flags which say which parameters get overridden
	SpriteParams(Region* r) {

		RegionParams& rp = r->params;
		RegionParams& op = r->palette->regionOverrideParams;
		RegionOverrides& ro = r->palette->regionOverrideFlags;

#define INIT_PARAM(name) name = ro.##name ? op.##name : rp.##name;

#include "SpriteParams_init.h"
	}

#include "SpriteParams_declare.h"
};

class SpriteState {
public:
	SpriteState() {
		visible = false;
		direction = 0.0;
		hue = 0.0f;
		huefill = 0.0f;
		pos = NosuchVector(0.0f,0.0f);
		depth = 0.0;
		size = 0.5;
		alpha = 1.0;
		born = 0;
		last_tm = 0;
		killme = false;
		rotangsofar = 0.0f;
		stationary = false;
		sidnum = 0;
		sidsource = "nosuchsource";
	}
	bool visible;
	double direction;
	double hue;
	double huefill;
	NosuchVector pos;
	double depth;
	double size;
	double alpha;
	int born;
	int last_tm;
	bool killme;
	double rotangsofar;
	bool stationary;
	int sidnum;
	std::string sidsource;
	int seq;     // sprite sequence # (mostly for debugging)
	int rotdir;  // -1, 0, 1
};

class Sprite {
	
public:

	virtual ~Sprite();

	static bool initialized;
	static void initialize();
	// static std::vector<std::string> spriteShapes;
	static double degree2radian(double deg);
	virtual void drawShape(PaletteHost* app, int xdir, int ydir) = 0;
	virtual bool fixedScale() { return false; }

	// Screen space is 2.0x2.0, while cursor space is 1.0x1.0
	void scaleCursorSpaceToScreenSpace(NosuchVector& pos) {
		state.pos.x *= 2.0f;
		state.pos.y *= 2.0f;
	}

	void draw(PaletteHost* app, Region* r);
	void drawAt(PaletteHost* app, double x,double y, double w, double h, int xdir, int ydir);
	NosuchVector deltaInDirection(double dt, double dir, double speed);
	int rotangdirOf(std::string s);
	void advanceTo(int tm);

	virtual void startAccumulate(Cursor* c) { };
	virtual void accumulate(Cursor* c) { }
	
	SpriteParams params;
	SpriteState state;

protected:
	Sprite(int sidnum, std::string sidsource, Region* r);
	double vertexNoise();

	static int NextSeq;
};

class SpriteSquare : public Sprite {

public:
	SpriteSquare(int sidnum, std::string sidsource, Region* r);
	void drawShape(PaletteHost* app, int xdir, int ydir);

private:
	double noise_x0;
	double noise_y0;
	double noise_x1;
	double noise_y1;
	double noise_x2;
	double noise_y2;
	double noise_x3;
	double noise_y3;
};

class CurvePoint {
public:
	CurvePoint(Cursor* c);
	NosuchVector pos;
	double depth;
	double degrees;
};

class SpriteCurve : public Sprite {

public:
	SpriteCurve(int sidnum, std::string sidsource, Region* r);
	void drawShape(PaletteHost* app, int xdir, int ydir);
	bool fixedScale() { return true; }
	void startAccumulate(Cursor* c);
	void accumulate(Cursor* c);

private:
	~SpriteCurve() {
		// NosuchDebug("SpriteCurve destructor!");
	}
	std::vector<CurvePoint> _points;
};

class SpriteTriangle : public Sprite {

public:
	SpriteTriangle(int sidnum, std::string sidsource, Region* r);
	void drawShape(PaletteHost* app, int xdir, int ydir);

private:
	double noise_x0;
	double noise_y0;
	double noise_x1;
	double noise_y1;
	double noise_x2;
	double noise_y2;
};

class SpriteCircle : public Sprite {

public:
	SpriteCircle(int sidnum, std::string sidsource, Region* r);
	void drawShape(PaletteHost* app, int xdir, int ydir);
};

class SpriteOutline : public Sprite {

public:
	SpriteOutline(int sidnum, std::string sidsource, Region* r);
	~SpriteOutline();
	void drawShape(PaletteHost* app, int xdir, int ydir);
	void setOutline(OutlineMem* om, MMTT_SharedMemHeader* hdr);

private:
	int npoints;
	PointMem* points;
};

class SpriteLine : public Sprite {

public:
	SpriteLine(int sidnum, std::string sidsource, Region* r);
	void drawShape(PaletteHost* app, int xdir, int ydir);

private:
	double noise_x0;
	double noise_y0;
	double noise_x1;
	double noise_y1;
};

#endif
