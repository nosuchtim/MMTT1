#include <cstdlib> // for srand, rand

#include "NosuchUtil.h"
#include "PaletteAll.h"
#include "coxdeboor.h"

// std::vector<std::string> Sprite::spriteShapes;

bool Sprite::initialized = false;
long nsprites = 0;
int Sprite::NextSeq = 0;

#define RANDDOUBLE (((double)rand())/RAND_MAX)

double Sprite::vertexNoise()
{
	if ( params.noisevertex > 0.0f ) {
		return params.noisevertex * RANDDOUBLE * ((rand()%2)==0?1:-1);
	} else {
		return 0.0f;
	}
}

void Sprite::initialize() {
	if ( initialized )
		return;
	initialized = true;
#if 0
	spriteShapes.push_back("nothing");  // make sure that sprite 0 is nothing
	spriteShapes.push_back("line");
	spriteShapes.push_back("triangle");
	spriteShapes.push_back("square");
	spriteShapes.push_back("arc");
	spriteShapes.push_back("circle");
	spriteShapes.push_back("outline");
	spriteShapes.push_back("curve");
#endif
}
	
Sprite::Sprite() {
}

void
Sprite::initState(int sidnum, std::string sidsource, NosuchVector& pos, double movedir, double depth) {

	nsprites++;
	Palette::lastsprite = Palette::now;

	// most of the state has been initialized in SpriteState constructor
	state.pos = pos;
	state.direction = movedir;
	state.depth = depth;
	state.sidnum = sidnum;
	state.sidsource = sidsource;

	state.born = Palette::now;
	state.last_tm = Palette::now;
	state.hue = params.hueinitial;
	state.huefill = params.huefillinitial;
	state.alpha = params.alphainitial;
	state.size = params.sizeinitial;
	state.seq = NextSeq++;
	state.rotdir = rotangdirOf(params.rotangdir);
}

Sprite::~Sprite() {
	NosuchDebug(1,"Sprite destructor! s=%d sid=%d/%s",this,state.sidnum,state.sidsource.c_str());
}

double Sprite::degree2radian(double deg) {
	return 2.0f * (double)M_PI * deg / 360.0f;
}

void Sprite::draw(PaletteHost* ph) {
	double scaled_z = scale_z(ph,state.depth);
	draw(ph,scaled_z);
}

#if 0
void Sprite::draw(PaletteHost* app) {
	draw(app,1.0);
}
#endif

void Sprite::draw(PaletteHost* app, double scaled_z) {
	if ( ! state.visible ) {
		NosuchDebug("Sprite.draw NOT DRAWING, !visible");
		return;
	}
	// double hue = state.hueoffset + params.hueinitial;
	// double huefill = state.hueoffset + params.huefill;
	
	NosuchColor color = NosuchColor(state.hue, params.luminance, params.saturation);
	NosuchColor colorfill = NosuchColor(state.huefill, params.luminance, params.saturation);
	
	if ( state.alpha <= 0.0f || state.size <= 0.0 ) {
		state.killme = true;
		return;
	}
	
	if ( params.filled ) {
		app->fill(colorfill, state.alpha);
	} else {
		app->noFill();
	}
	app->stroke(color, state.alpha);
	if ( state.size < 0.001f ) {
		state.killme = true;
		return;
	}
	double thickness = params.thickness;
	app->strokeWeight(thickness);
	double aspect = params.aspect;
	
	// double scaled_z = region->scale_z(state.depth);

	double scalex = state.size * scaled_z;
	double scaley = state.size * scaled_z;
	
	scalex *= aspect;
	// scaley *= (1.0f/aspect);
	
	// double w = app->width * scalex;
	// double h = app->height * scaley;
	
	// if (w < 0 || h < 0) {
	// 	NosuchDebug("Hey, wh < 0?  w=%f h=%f\n",w,h);
	// }

	double x;
	double y;
	// NOTE!  The x,y coming in here is scaled to ((0,0),(1,1))
	//        while the x,y computed and given to the drawAt method
	//        is scaled to ((-1,-1),(1,1))
	int xdir;
	int ydir;
	if ( params.mirror == "four" ) {
		x = 2.0f * state.pos.x * app->width - 1.0f;
		y = 2.0f * state.pos.y * app->height - 1.0f;
		xdir = 1;
		ydir = 1;
		drawAt(app,x,y,scalex,scaley,xdir,ydir);
		ydir = -1;
		drawAt(app,x,-y,scalex,scaley,xdir,ydir);
		xdir = -1;
		drawAt(app,-x,y,scalex,scaley,xdir,ydir);
		ydir = 1;
		drawAt(app,-x,-y,scalex,scaley,xdir,ydir);
	} else if ( params.mirror == "vertical" ) {
		x = 2.0f * state.pos.x * app->width - 1.0f;
		y = state.pos.y * app->height;
		xdir = 1;
		ydir = 1;
		drawAt(app,x,y,scalex,scaley,xdir,ydir);
		// y = (1.0f-state.pos.y) * app->height;
		y = (-state.pos.y) * app->height;
		ydir = -1;
		drawAt(app,x,y,scalex,scaley,xdir,ydir);
	} else if ( params.mirror == "horizontal" ) {
		x = state.pos.x * app->width;
		y = 2.0f * state.pos.y * app->height - 1.0f;
		xdir = 1;
		ydir = 1;
		drawAt(app,x,y,scalex,scaley,xdir,ydir);
		// x = (1.0f-state.pos.x) * app->width;
		x = (-state.pos.x) * app->width;
		xdir = -1;
		drawAt(app,x,y,scalex,scaley,xdir,ydir);
	} else {
		x = 2.0f * state.pos.x * app->width - 1.0f;
		y = 2.0f * state.pos.y * app->height - 1.0f;
		xdir = 1;
		ydir = 1;
		drawAt(app,x,y,scalex,scaley,xdir,ydir);
	}
}
	
void Sprite::drawAt(PaletteHost* app, double x,double y, double scalex, double scaley, int xdir, int ydir) {
	// NosuchDebug("Sprite gl_frame=%d drawAt s=%ld xy= %.4lf %.4lf",app->gl_frame,(long)this,x,y);
	app->pushMatrix();
	app->translate(x,y);
	if ( fixedScale() ) {
		app->scale(1.0,1.0);
	} else {
		app->scale(scalex,scaley);
	}
	double degrees = params.rotanginit + state.rotangsofar;
	// NosuchDebug("SpriteDraw seq=%d anginit=%.3f sofar=%.3f degrees=%f",
	// 	state.seq,params.rotanginit,state.rotangsofar,degrees);
	// NosuchDebug("Sprite::drawAt degrees=%.4f  w,h=%f,%f\n",degrees,w,h);
	// NosuchDebug("Sprite::drawAt s=%d degrees=%.4f",(int)this,degrees);
	app->rotate(degrees);
	drawShape(app,xdir,ydir);
	app->popMatrix();
}

NosuchVector Sprite::deltaInDirection(double dt, double dir, double speed) {
	NosuchVector delta( (double) cos(degree2radian(dir)), (double) sin(degree2radian(dir)));
	delta = delta.normalize();
	delta = delta.mult((dt / 1000.0f) * speed);
	return delta;
}

int Sprite::rotangdirOf(std::string s) {
	int dir = 1;
	if ( s == "right" ) {
		dir = 1;
	} else if ( s == "left" ) {
		dir = -1;
	} else if ( s == "random" ) {
		dir = ((rand()%2) == 0) ? 1 : -1;
	} else {
		NosuchDebug("Sprite.advanceto, bad value for rotangdir!? = %s, assuming random",s.c_str());
		dir = ((rand()%2) == 0) ? 1 : -1;
	}
	return dir;
}

double
envelopeValue(double initial, double final, double duration, double born, double now) {
	double dt = now - born;
	double dur = duration * 1000.0;
	if ( dt >= dur )
		return final;
	if ( dt <= 0 )
		return initial;
	return initial + (final-initial) * ((now-born)/(dur));
}

void Sprite::advanceTo(int now) {

	// _params->advanceTo(tm);
	state.alpha = envelopeValue(params.alphainitial,params.alphafinal,params.alphatime,state.born,now);
	state.size = envelopeValue(params.sizeinitial,params.sizefinal,params.sizetime,state.born,now);
	
	// NosuchDebug("Sprite::advanceTo tm=%d  life=%f",tm,life);
	if (params.lifetime >= 0.0 && ((now - state.born) > (1000.0 * params.lifetime))) {
		NosuchDebug(2,"Lifetime of Sprite exceeded, setting killme");
		state.killme = true;
	}
	double dt = (double)(now - state.last_tm);
	state.last_tm = now;
	
	if ( ! state.visible ) {
		return;
	}
	
	state.hue = envelopeValue(params.hueinitial,params.huefinal,params.huetime,state.born,now);
	state.huefill = envelopeValue(params.huefillinitial,params.huefillfinal,params.huefilltime,state.born,now);

	// state.hueoffset = fmod((state.hueoffset + params.cyclehue), 360.0);

	if ( state.stationary ) {
		NosuchDebug(2,"Sprite %d is stationary",this);
		return;
	}

	if ( params.rotangspeed != 0.0 ) {
		state.rotangsofar = fmod((state.rotangsofar + state.rotdir * (dt/1000.0) * params.rotangspeed) , 360.0);
	}
	
	if ( params.speed != 0.0 ) {
		
		double dir = state.direction;
		
		NosuchVector delta = deltaInDirection(dt,dir,params.speed);
		
		NosuchVector npos = state.pos.add(delta);
		if ( params.bounce ) { 
			
			if ( npos.x > 1.0f ) {
				dir = fmod(( dir + 180 ) , 360);
				delta = deltaInDirection(dt,dir,params.speed);
				npos = state.pos.add(delta);
			}
			if ( npos.x < 0.0f ) {
				dir = fmod(( dir + 180 ) , 360);
				delta = deltaInDirection(dt,dir,params.speed);
				npos = state.pos.add(delta);
			}
			if ( npos.y > 1.0f ) {
				dir = fmod(( dir + 180 ) , 360);
				delta = deltaInDirection(dt,dir,params.speed);
				npos = state.pos.add(delta);
			}
			if ( npos.y < 0.0f ) {
				dir = fmod(( dir + 180 ) , 360);
				delta = deltaInDirection(dt,dir,params.speed);
				npos = state.pos.add(delta);
			}
			state.direction = dir;
		} else {
			if ( npos.x > 1.0f || npos.x < 0.0f || npos.y > 1.0f || npos.y < 0.0f ) {
				state.killme = true;
			}
		}
		
		state.pos = npos;
	}
}

SpriteList::SpriteList() {
	rwlock = PTHREAD_RWLOCK_INITIALIZER;
	int rc1 = pthread_rwlock_init(&rwlock, NULL);
	if ( rc1 ) {
		NosuchDebug("Failure on pthread_rwlock_init!? rc=%d",rc1);
	}
	NosuchDebug(2,"rwlock has been initialized");
}

void
	SpriteList::lock_read() {
#ifndef DO_OSC_AND_HTTP_IN_MAIN_THREAD
	int e = pthread_rwlock_rdlock(&rwlock);
	if ( e != 0 ) {
		NosuchDebug("rwlock for read failed!? e=%d",e);
		return;
	}
#endif
}

void
SpriteList::lock_write() {
#ifndef DO_OSC_AND_HTTP_IN_MAIN_THREAD
	int e = pthread_rwlock_wrlock(&rwlock);
	if ( e != 0 ) {
		NosuchDebug("rwlock for write failed!? e=%d",e);
		return;
	}
#endif
}

void
SpriteList::unlock() {
#ifndef DO_OSC_AND_HTTP_IN_MAIN_THREAD
	int e = pthread_rwlock_unlock(&rwlock);
	if ( e != 0 ) {
		NosuchDebug("rwlock unlock failed!? e=%d",e);
		return;
	}
#endif
}

void SpriteList::hit() {
	lock_write();
	for ( std::list<Sprite*>::iterator i = sprites.begin(); i!=sprites.end(); i++) {
		Sprite* s = *i;
		NosuchAssert(s);
		s->state.alpha = 1.0;
	}
	unlock();
}

void
SpriteList::add(Sprite* s, int limit)
{
	lock_write();
	sprites.push_back(s);
	NosuchAssert(limit >= 1);
	while ( (int)sprites.size() > limit ) {
		Sprite* ps = sprites.front();
		sprites.pop_front();
		delete ps;
	}
	s->state.visible = true;
	unlock();
}

void
SpriteList::draw(PaletteHost* b) {
	lock_read();
	for ( std::list<Sprite*>::iterator i = sprites.begin(); i!=sprites.end(); i++ ) {
		Sprite* s = *i;
		NosuchAssert(s);
		s->draw(b);
	}
	unlock();
}

void
SpriteList::advanceTo(int tm) {
	lock_write();
	for ( std::list<Sprite*>::iterator i = sprites.begin(); i!=sprites.end(); ) {
		Sprite* s = *i;
		NosuchAssert(s);
		s->advanceTo(tm);
		if ( s->state.killme ) {
			i = sprites.erase(i);
			// NosuchDebug("Should be deleting Sprite s=%d",(int)s);
			delete s;
		} else {
			i++;
		}
	}
	unlock();
}

CurvePoint::CurvePoint(Cursor* c) {
	pos = c->curr_pos;
	depth = c->curr_depth;
	degrees = c->curr_degrees;
}

void SpriteCurve::startAccumulate(Cursor* c) {
	state.stationary = true;
	_points.clear();

	NosuchDebug(1,"SpriteCurve startAccumpos=%.3f,%.3f",state.pos.x,state.pos.y);

	_points.push_back(CurvePoint(c));
}

void SpriteCurve::accumulate(Cursor* c) {
	double depth = c->curr_depth;
	if ( ! state.pos.isequal(c->curr_pos) ) {
		_points.push_back(CurvePoint(c));
		state.born = Palette::now;
		NosuchDebug(2,"SpriteCurve accum npoints=%d new pos=%.3f,%.3f",
			_points.size(),c->curr_pos.x,c->curr_pos.y);
	}
}

SpriteCurve::SpriteCurve() {
	_points.clear();
}

double randv() {
	return ( rand() % 1000 ) / 1000.0f;
}

void SpriteCurve::drawShape(PaletteHost* app, int xdir, int ydir) {

#define JUST_LINES

#ifdef JUST_LINES

	unsigned int npoints = _points.size();
	// NosuchVector point0 = _points[0].pos;
	// NosuchVector drawpt0 = NosuchVector(0.0f,0.0f);
	NosuchAssert(npoints>0);
	CurvePoint cpt0 = _points[0];
	NosuchVector pos0 = _points[0].pos;

	NosuchVector quadpt0 = cpt0.pos.sub(pos0);
	NosuchVector quadpt1 = quadpt0;

	for ( unsigned int n=1; n<npoints; n++ ) {
		CurvePoint cpt1 = _points[n];
		NosuchVector cursor_pos0 = cpt0.pos.sub(pos0);
		NosuchVector cursor_pos1 = cpt1.pos.sub(pos0);

		scaleCursorSpaceToScreenSpace(cursor_pos0);
		scaleCursorSpaceToScreenSpace(cursor_pos1);

		// app->line(cursor_pos0.x,cursor_pos0.y,cursor_pos1.x,cursor_pos1.y);

		NosuchVector mid = NosuchVector(
			(cursor_pos0.x+cursor_pos1.x)/2.0f,
			(cursor_pos0.y+cursor_pos1.y)/2.0f);
		double d = cpt1.depth * 0.5f;
		NosuchVector mid1 = mid.add(NosuchVector(d,d));
		NosuchVector mid2 = mid.add(NosuchVector(-d,-d));
		mid1 = mid1.rotate(DEGREE2RADIAN(cpt1.degrees),mid);
		mid2 = mid2.rotate(DEGREE2RADIAN(cpt1.degrees),mid);

		// app->line(mid1.x,mid1.y,mid2.x,mid2.y);

		NosuchVector quadpt2 = mid1;
		NosuchVector quadpt3 = mid2;

		if ( ! params.filled ) {
			app->line(
				quadpt0.x,quadpt0.y,
				quadpt2.x,quadpt2.y);
			app->line(
				quadpt1.x,quadpt1.y,
				quadpt3.x,quadpt3.y);
		} else {
			app->quad(
				quadpt0.x,quadpt0.y,
				quadpt1.x,quadpt1.y,
				quadpt3.x,quadpt3.y,
				quadpt2.x,quadpt2.y);
		}

		quadpt0 = quadpt2;
		quadpt1 = quadpt3;

		cpt0 = cpt1;
	}

#else
	if ( _points.size() < 4 ) {
		// No curve, just lines
		if ( _points.size() > 1 ) {
			NosuchVector point0 = _points[0].pos;
			NosuchVector drawpt0 = NosuchVector(0.0f,0.0f);
			unsigned int npoints = _points.size();
			for ( unsigned int n=1; n<npoints; n++ ) {
				NosuchVector drawpt1 = _points[n].pos.sub(point0);
				scaleCursorSpaceToScreenSpace(drawpt1);
				app->line(drawpt0.x,drawpt0.y,drawpt1.x,drawpt1.y);
				drawpt0 = drawpt1;
			}
		}
		return;
	}
	NosuchDebug(2,"SpriteCurve::drawshape   _point.size=%d",_points.size());
	CoxDeBoorAlgorithm cdb;

	NosuchVector point0 = _points[0].pos;
	cdb.addPoint(NosuchVector(0.0f,0.0f));
	NosuchDebug(2,"   first point=%f,%f",_points[0].pos.x,_points[0].pos.y);

	unsigned int npoints = _points.size();
	NosuchDebug(2,"SpriteCurve::drawShape npoints=%d",npoints);

	for ( unsigned int n=1; n<npoints; n++ ) {
		NosuchVector screenpt1 = _points[n].pos.sub(point0);
		scaleCursorSpaceToScreenSpace(screenpt1);
		NosuchDebug(2,"   adding point=%f,%f",screenpt1.x,screenpt1.y);
		cdb.addPoint(screenpt1);
	}

	cdb.endOfPoints();
	int LOD = npoints*3;
	NosuchVector pt0 = cdb.GetOutpoint(0.0f);
	for(int i=1;i<LOD;++i) {
		double t  = cdb.last_knot() * i / (double)(LOD-1);
		NosuchDebug(2,"  last_knot=%f i=%d t=%f",cdb.last_knot(),i,t);
		double remainder = fmod(t,1.0f);
		if ( remainder < 0.00001 ) {
			t -= 0.001f;
			NosuchDebug(2,"Remainder is small, adjusting t=%f",t);
		}
		if (i==LOD-1) {
			t-=0.001f;
			NosuchDebug(2,"  adjusted final t=%f",t);
		}
		NosuchVector pt1 = cdb.GetOutpoint(t);
		NosuchDebug(2,"  pt1 %f,%f",pt1.x,pt1.y);
		app->line(pt0.x,pt0.y,pt1.x,pt1.y);
		pt0 = pt1;
	}
	NosuchDebug(2,"     SpriteCurve::drawshape end");

#endif
}

SpriteSquare::SpriteSquare() {
	noise_x0 = vertexNoise();
	noise_y0 = vertexNoise();
	noise_x1 = vertexNoise();
	noise_y1 = vertexNoise();
	noise_x2 = vertexNoise();
	noise_y2 = vertexNoise();
	noise_x3 = vertexNoise();
	noise_y3 = vertexNoise();
}

void SpriteSquare::drawShape(PaletteHost* app, int xdir, int ydir) {
	double halfw = 0.2f;
	double halfh = 0.2f;

	double x0 = - halfw + noise_x0 * halfw;
	double y0 = - halfh + noise_y0 * halfh;
	double x1 = -halfw + noise_x1 * halfw;
	double y1 = halfh + noise_y1 * halfh;
	double x2 = halfw + noise_x2 * halfw;
	double y2 = halfh + noise_y2 * halfh;
	double x3 = halfw + noise_x3 * halfw;
	double y3 = -halfh + noise_y3 * halfh;
	NosuchDebug(2,"drawing Square halfw=%.3f halfh=%.3f",halfw,halfh);
	app->quad( x0,y0, x1,y1, x2,y2, x3, y3);
}

SpriteTriangle::SpriteTriangle() {
	noise_x0 = vertexNoise();
	noise_y0 = vertexNoise();
	noise_x1 = vertexNoise();
	noise_y1 = vertexNoise();
	noise_x2 = vertexNoise();
	noise_y2 = vertexNoise();
}
	
void SpriteTriangle::drawShape(PaletteHost* app, int xdir, int ydir) {
	// double halfw = w / 2.0f;
	// double halfh = h / 2.0f;
	double sz = 0.2f;
	NosuchVector p1 = NosuchVector(sz,0.0f);
	NosuchVector p2 = p1;
	p2 = p2.rotate(Sprite::degree2radian(120));
	NosuchVector p3 = p1;
	p3 = p3.rotate(Sprite::degree2radian(-120));
	
	app->triangle(p1.x+noise_x0*sz,p1.y+noise_y0*sz,
			     p2.x+noise_x1*sz,p2.y+noise_y1*sz,
			     p3.x+noise_x2*sz,p3.y+noise_y2*sz);
}

SpriteLine::SpriteLine() {
	noise_x0 = vertexNoise();
	noise_y0 = vertexNoise();
	noise_x1 = vertexNoise();
	noise_y1 = vertexNoise();
}
	
void SpriteLine::drawShape(PaletteHost* app, int xdir, int ydir) {
	// NosuchDebug("SpriteLine::drawShape wh=%f %f\n",w,h);
	double halfw = 0.2f;
	double halfh = 0.2f;
	double x0 = -0.2f;
	double y0 =  0.0f;
	double x1 =  0.2f;
	double y1 =  0.0f;
	app->line(x0 + noise_x0, y0 + noise_y0, x1 + noise_x1, y1 + noise_y1);
}

SpriteCircle::SpriteCircle() {
}

void SpriteCircle::drawShape(PaletteHost* app, int xdir, int ydir) {
	// NosuchDebug("SpriteCircle drawing");
	app->ellipse(0, 0, 0.2f, 0.2f);
}

SpriteOutline::SpriteOutline() {
	npoints = 0;
	points = NULL;
}

static void
normalize(NosuchVector* v)
{
	v->x = (v->x * 2.0) - 1.0;
	v->y = (v->y * 2.0) - 1.0;
}

void
SpriteOutline::drawShape(PaletteHost* app, int xdir, int ydir) {
	// NosuchDebug("SpriteOutline drawing");
	// app->ellipse(0, 0, 0.2f, 0.2f);
	// The points we're getting are already centered on the center of
	// the outline, and in the range (-1,-1) to (1,1)
	app->polygon(points,npoints);
#if 0
	NosuchVector v0;
	NosuchVector v1;
	NosuchVector vfirst;
	for ( int pn=0; pn<npoints; pn++ ) {
		PointMem* p = &points[pn];
		if ( pn == 0 ) {
			v0 = NosuchVector(p->x,p->y);
			vfirst = v0;
		} else {
			v1 = NosuchVector(p->x,p->y);
			app->line(v0.x,v0.y,v1.x,v1.y);
			v0 = v1;
		}
	}
	if ( npoints > 2 ) {
		// app->stroke(color2,1.0);
		// app->strokeWeight(1.0);
		app->line(v0.x,v0.y,vfirst.x,vfirst.y);
	}
#endif
}

void
SpriteOutline::setOutline(OutlineMem* om, MMTT_SharedMemHeader* hdr) {
	buff_index b = hdr->buff_to_display;
	PointMem* p = hdr->point(b,om->index_of_firstpoint);
	npoints = om->npoints;
	points = new PointMem[npoints];
	memcpy(points,p,npoints*sizeof(PointMem));
}

SpriteOutline::~SpriteOutline() {
	if ( points ) {
		delete points;
	}
}
