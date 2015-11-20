#include <pthread.h>

#include <iostream>
#include <sstream>
#include <fstream>
#include <strstream>
#include <cstdlib> // for srand, rand
#include <ctime>   // for time

#include "UT_SharedMem.h"
#include "SharedMemHeader.h"
#include "PaletteAll.h"
#include "FFGLLib.h"
#include "Scale.h"
#include "osc/OscOutboundPacketStream.h"

// #define FFPARAM_BRIGHTNESS (0)

////////////////////////////////////////////////////////////////////////////////////////////////////
//  Plugin information
////////////////////////////////////////////////////////////////////////////////////////////////////

bool PaletteHost::StaticInitialized = false;

extern "C"
{

bool
ffgl_setdll(std::string dllpath)
{
	dllpath = NosuchToLower(dllpath);

	size_t lastslash = dllpath.find_last_of("/\\");
	size_t lastunder = dllpath.find_last_of("_");
	size_t lastdot = dllpath.find_last_of(".");
	std::string suffix = (lastdot==dllpath.npos?"":dllpath.substr(lastdot));

	if ( suffix != ".dll" ) {
		NosuchDebug("Hey! dll name (%s) isn't of the form *.dll!?",dllpath.c_str());
		return FALSE;
	}

	std::string dir = dllpath.substr(0,lastslash);
	std::string prefix = dllpath.substr(lastslash+1,lastdot-lastslash-1);

	NosuchCurrentDir = dir;

	NosuchDebugSetLogDirFile(dir,"debug.txt");

	struct _stat statbuff;
	int e = _stat(NosuchCurrentDir.c_str(),&statbuff);
	if ( ! (e == 0 && (statbuff.st_mode | _S_IFDIR) != 0) ) {
		NosuchDebug("Hey! No directory %s!?",NosuchCurrentDir.c_str());
		return FALSE;
	}

	NosuchDebug("Setting NosuchCurrentDir = %s",NosuchCurrentDir.c_str());
	return TRUE;
}
}

void *network_threadfunc(void *arg)
{
	PaletteDaemon* b = (PaletteDaemon*)arg;
	return b->network_input_threadfunc(arg);
}

PaletteDaemon::PaletteDaemon(PaletteHost* mf, int osc_input_port, std::string osc_input_host, int http_input_port)
{
	NosuchDebug(2,"PaletteDaemon CONSTRUCTOR!");

	_paletteHost = mf;
	_network_thread_created = false;
	daemon_shutting_down = false;

	if ( osc_input_port < 0 ) {
		NosuchDebug("NOT CREATING _oscinput!! because osc_input_port<0");
		_oscinput = NULL;
	} else {
		NosuchDebug(2,"CREATING _oscinput and PaletteServer!!");
		_oscinput = new PaletteOscInput(mf,osc_input_host.c_str(),osc_input_port);
		_oscinput->Listen();
	}

	_http = new PaletteHttp(mf, http_input_port,60);

	NosuchDebug(2,"About to use pthread_create in PaletteDaemon");
	int err = pthread_create(&_network_thread, NULL, network_threadfunc, this);
	if (err) {
		NosuchDebug("pthread_create failed!? err=%d\n",err);
		NosuchErrorOutput("pthread_create failed!?");
	} else {
		_network_thread_created = true;
		NosuchDebug("PaletteDaemon is running");
	}
}

PaletteDaemon::~PaletteDaemon()
{
	NosuchDebug("PaletteDaemon DESTRUCTOR starts!");
	daemon_shutting_down = true;
	if ( _network_thread_created ) {
		// pthread_detach(_network_thread);
		pthread_join(_network_thread,NULL);
	}

	NosuchAssert(_http != NULL);

	NosuchDebug("PaletteDaemon destructor, removing processor from _http!");
	delete _http;
	_http = NULL;

	if ( _oscinput ) {
		NosuchDebug("PaletteDaemon destructor, removing processor from _oscinput!");
		_oscinput->UnListen();
		delete _oscinput;
		NosuchDebug("PaletteDaemon destructor, after removing processor from _oscinput!");
		_oscinput = NULL;
	}

	NosuchDebug("PaletteDaemon DESTRUCTOR ends!");
}

void *PaletteDaemon::network_input_threadfunc(void *arg)
{
#ifdef DO_OSC_AND_HTTP_IN_MAIN_THREAD
	NosuchDebug("PaletteDaemon network_input_threadfunc IS DISABLED!!!!");
	return NULL;
#else
	int textcount = 0;
	while (daemon_shutting_down == false ) {
		// Don't check 
		if ( textcount++ > 100 ) {
			_paletteHost->CheckTimeouts(Palette::now);
		}
		if ( _http ) {
			_http->Check();
		}
		if ( _oscinput ) {
			_oscinput->Check();
		}
		Sleep(1);
	}
	return NULL;
#endif
}

std::string PaletteHost::HtmlDir() {
	return NosuchFullPath("..\\html");
}

std::string PaletteHost::ParamConfigDir() {
	std::string dirname = "..\\config\\palette\\params";
	return NosuchFullPath(dirname);
}

std::string PaletteHost::ConfigFileName(std::string name) {
	return NosuchFullPath("..\\config\\palette\\"+name);
}

int PaletteHost::NumEffectSet() {
	return NUM_EFFECT_SETS;
}

void PaletteHost::LoadEffectSet(int eset) {
	NosuchDebug(1,"PaletteHost::LoadEffectSet eset=%d",eset);
	if ( eset >= NumEffectSet() ) {
		NosuchDebug("Hey!  eset=%d is >= NumEffectSet?",eset);
		return;
	}
#ifdef RANDOM_EFFECT_7
	if ( eset == 7 ) {
		NosuchDebug("EFFECT SELECTION! ALL RANDOM!");
		LoadEffectSetRandom();
	} else
#endif
	{
		NosuchDebug("EFFECT SELECTION! set=%d",eset);
		EffectSet es = buttonEffectSet[eset];
		for ( int e=0; e<13; e++ ) {
			EnableEffect(e, es.effectOn[e]);
		}
	}
}

void PaletteHost::LoadEffectSetRandom() {
	NosuchDebug("PaletteHost::LoadEffectSetRandom");
	for ( int e=0; e<13; e++ ) {
		double r = (((double)rand())/RAND_MAX);
		// 25% of them will be on
		EnableEffect(e, r < 0.25 ? 1 : 0);
	}
}

void PaletteHost::StaticInitialization()
{
	if ( StaticInitialized ) {
		return;
	}
	StaticInitialized = true;

	srand( Pt_Time() );

	// Default debugging stuff
	NosuchDebugLevel = 0;   // 0=minimal messages, 1=more, 2=extreme
	NosuchDebugTimeTag = true;
	NosuchDebugThread = true;
	NosuchDebugToConsole = true;
	NosuchDebugToLog = true;
	NosuchAppName = "Palette";
#ifdef DEBUG_TO_BUFFER
	NosuchDebugToBuffer = true;
#endif
	NosuchDebugAutoFlush = true;
	NosuchDebugLogFile = NosuchFullPath("debug.txt");
	
	NosuchDebug(1,"=== PaletteHost Static Initialization!");
}

PaletteHost* RealPaletteHost = NULL;

PaletteHost::PaletteHost(std::string defaultsfile)
{
	NosuchDebug(1,"=== PaletteHost is being constructed.");

	NosuchErrorPopup = PaletteHost::ErrorPopup;

	_global_defaults_file = defaultsfile;
	_recompileFunc = NULL;
	_python_enabled = FALSE;
	_python_events_disabled = TRUE;
	_dotest = FALSE;
	_textEraseTime = 0;
	_lastActivity = -1;
	_attractOn = 0;
	_sharedmem_outlines = NULL;
	_sharedmem_last_attempt = 0;

	LoadGlobalDefaults();

	openSharedMemOutlines();

	_scheduler = new NosuchScheduler();

	_looper = new NosuchLooper(this);
	_daemon = NULL;

	initialized = false;
	gl_shutting_down = false;
	gl_frame = 0;

	width = 1.0f;
	height = 1.0f;

	// Don't do any OpenGL calls here, it isn't initialized yet.

	NosuchLockInit(&json_mutex,"json");
	json_cond = PTHREAD_COND_INITIALIZER;
	json_pending = false;

	NosuchLockInit(&palette_mutex,"palette");
	NosuchLockInit(&python_mutex,"python");

	m_filled = false;
	m_stroked = false;
	
	disabled = false;
	disable_on_exception = false;
}

PaletteHost::~PaletteHost()
{
	NosuchDebug(1,"PaletteHost destructor called");
	gl_shutting_down = true;
	scheduler()->Stop();
	delete _scheduler;
	_scheduler = NULL;

	if ( _sharedmem_outlines != NULL ) {
		delete _sharedmem_outlines;
		_sharedmem_outlines = NULL;
		_do_sharedmem = false;
	}
	if ( _daemon != NULL ) {
		delete _daemon;
		_daemon = NULL;
	}
	NosuchDebug(1,"PaletteHost destructor end");
}

void PaletteHost::ErrorPopup(const char* msg) {
		MessageBoxA(NULL,msg,"Palette",MB_OK);
}

void
PaletteHost::LoadGlobalDefaults()
{
	// These are default values, which can be overridden by the config file.
	_osc_input_port = DEFAULT_OSC_INPUT_PORT;
	_osc_input_host = DEFAULT_OSC_INPUT_HOST;
	_http_input_port = DEFAULT_HTTP_INPUT_PORT;
	_do_tuio = false;
	_do_midi = false;
	_do_sharedmem = true;
	_patchFile = "default.mnf";
	_graphicbehaviour = "default";
	_musicbehaviour = "default";

	_resolume_output_host = "127.0.0.1";  // This should always be the case
	_resolume_output_port = DEFAULT_RESOLUME_PORT;

	_pyffle_output_host = "127.0.0.1";  // This should always be the case
	_pyffle_output_port = DEFAULT_PYFFLE_PORT;

	// Config file can override those values
	std::ifstream f;
	std::string fname = _global_defaults_file;

	f.open(fname.c_str());
	if ( ! f.good() ) {
		std::string err = NosuchSnprintf("No config file (%s), assuming defaults\n",fname.c_str());
		NosuchDebug("%s",err.c_str());  // avoid re-interpreting %'s and \\'s in name
		return;
	}

	NosuchDebug("Loading config=%s\n",fname.c_str());
	std::string line;
	std::string jstr;
	while ( getline(f,line) ) {
		if ( line.size()>0 && line.at(0)=='#' ) {
			NosuchDebug(1,"Ignoring comment line=%s\n",line.c_str());
			continue;
		}
		jstr += line;
	}
	f.close();

	LoadConfigDefaultsJson(jstr);
}

GraphicBehaviour*
PaletteHost::makeGraphicBehaviour(Region* r) {
	return GraphicBehaviour::makeBehaviour(_graphicbehaviour,r);
}

MusicBehaviour*
PaletteHost::makeMusicBehaviour(Region* r) {
	return MusicBehaviour::makeBehaviour(_musicbehaviour,r);
}

MidiBehaviour*
PaletteHost::makeMidiBehaviour(std::string type, Channel* c) {
	return MidiBehaviour::makeBehaviour(type,c);
}

static cJSON *
getNumber(cJSON *json,char *name)
{
	cJSON *j = cJSON_GetObjectItem(json,name);
	if ( j && j->type == cJSON_Number )
		return j;
	return NULL;
}

static cJSON *
getString(cJSON *json,char *name)
{
	cJSON *j = cJSON_GetObjectItem(json,name);
	if ( j && j->type == cJSON_String )
		return j;
	return NULL;
}

void
PaletteHost::LoadConfigDefaultsJson(std::string jstr)
{
	cJSON *json = cJSON_Parse(jstr.c_str());
	if ( ! json ) {
		NosuchDebug("Unable to parse json for config!?  json= %s\n",jstr.c_str());
		return;
	}

	cJSON *j;

	if ( (j=getNumber(json,"sharedmem")) != NULL ) {
		_do_sharedmem = (j->valueint != 0);
	}
	if ( (j=getNumber(json,"tuio")) != NULL ) {
		_do_tuio = (j->valueint != 0);
	}
	if ( (j=getNumber(json,"tuioport")) != NULL ) {
		_osc_input_port = j->valueint;
	}
	if ( (j=getString(json,"tuiohost")) != NULL ) {
		_osc_input_host = j->valuestring;
		NosuchDebug("Setting tuiohost to %s",_osc_input_host.c_str());
	}
	if ( (j=getNumber(json,"httpport")) != NULL ) {
		_http_input_port = j->valueint;
	}
	if ( (j=getNumber(json,"resolumeport")) != NULL ) {
		_resolume_output_port = j->valueint;
	}
	if ( (j=getString(json,"config")) != NULL ) {
		_configFile = std::string(j->valuestring);
	}
	if ( (j=getString(json,"patch")) != NULL ) {
		_patchFile = std::string(j->valuestring);
	}
	if ( (j=getNumber(json,"debugtoconsole")) != NULL ) {
		NosuchDebugToConsole = j->valueint?TRUE:FALSE;
	}
	if ( (j=getNumber(json,"debugtolog")) != NULL ) {
		bool b = j->valueint?TRUE:FALSE;
		// If we're turning debugtolog off, put a final
		// message out so we know that!
		if ( NosuchDebugToLog && !b ) {
			NosuchDebug("ALERT: NosuchDebugToLog is being set to false!");
		}
		NosuchDebugToLog = b;
	}
	if ( (j=getNumber(json,"debugautoflush")) != NULL ) {
		NosuchDebugAutoFlush = j->valueint?TRUE:FALSE;
	}
	if ( (j=getString(json,"debuglogfile")) != NULL ) {
		NosuchDebugSetLogDirFile(NosuchDebugLogDir,std::string(j->valuestring));
	}
	if ( (j=getString(json,"graphicbehaviour")) != NULL ) {
		_graphicbehaviour = std::string(j->valuestring);
	}
	if ( (j=getString(json,"musicbehaviour")) != NULL ) {
		_musicbehaviour = std::string(j->valuestring);
	}
}

void
PaletteHost::openSharedMemOutlines()
{
	if ( ! _do_sharedmem || _sharedmem_outlines != NULL ) {
		return;
	}
	if ( ! Pt_Started() ) {
		return;
	}
	long now = Pt_Time();
	// Only check once in a while
	if ( (now - _sharedmem_last_attempt) < 5000 ) {
		return;
	}
	_sharedmem_outlines = new UT_SharedMem("mmtt_outlines");
	_sharedmem_last_attempt = now;
	UT_SharedMemError err = _sharedmem_outlines->getErrorState();
	if ( err != UT_SHM_ERR_NONE ) {
		NosuchDebug("Unable to open shared memory for outlines?  Is MMTT running?  err=%d",err);
		delete _sharedmem_outlines;
		_sharedmem_outlines = NULL;
	}
}

int
PaletteHost::EnableEffect(int effectnum, bool enabled)
{
    char buffer[1024];
    osc::OutboundPacketStream p( buffer, sizeof(buffer) );
	// The effectnum internally is 0-12 (or whatever the number of effects is)
	// but Resolume knows them as effect 2-14 (effect 1 is PaletteHost)
	effectnum += 2;
	std::string addr = NosuchSnprintf("/activeclip/video/effect%d/bypassed",effectnum);
	int bypassed = enabled ? 0 : 1;
    p << osc::BeginMessage( addr.c_str() ) << bypassed << osc::EndMessage;
    return SendToResolume(p);
}

int PaletteHost::SendToResolume(osc::OutboundPacketStream& p) {
	NosuchDebug(1,"SendToResolume host=%s port=%d",_resolume_output_host,_resolume_output_port);
    return SendToUDPServer(_resolume_output_host,_resolume_output_port,p.Data(),(int)p.Size());
}

void
PaletteHost::ShowChoice(std::string bn, std::string text, int x, int y, int timeout) {
    char buffer[1024];
    osc::OutboundPacketStream p( buffer, sizeof(buffer) );
    // p << osc::BeginMessage( "/set_text" ) << text.c_str() << osc::EndMessage;
    p << osc::BeginMessage( "/set_choice" ) << text.c_str() << bn.c_str() << osc::EndMessage;
    SendToUDPServer(_pyffle_output_host,_pyffle_output_port,p.Data(),(int)p.Size());
    p.Clear();
    p << osc::BeginMessage( "/set_pos" ) << x << y << osc::EndMessage;
    SendToUDPServer(_pyffle_output_host,_pyffle_output_port,p.Data(),(int)p.Size());
	if ( timeout > 0 ) {
		_textEraseTime = Pt_Time() + timeout;
	}
}

void
PaletteHost::ShowAttract(int onoff) {
    char buffer[1024];
	_attractOn = onoff;
    osc::OutboundPacketStream p( buffer, sizeof(buffer) );
    p << osc::BeginMessage( "/attract" ) << (onoff?1:0) << osc::EndMessage;
    SendToUDPServer(_pyffle_output_host,_pyffle_output_port,p.Data(),(int)p.Size());
    p.Clear();
}

void
PaletteHost::CheckTimeouts(int millinow) {
	// This is currently called from the network thread.
	int attractTimeout = 30000;
	bool inactive = ( _lastActivity > 0 && millinow > (_lastActivity+attractTimeout) );
	if ( _attractOn>0 && ( _lastActivity > 0 && millinow < (_lastActivity+attractTimeout) ) ) {
		NosuchDebug("UNSHOW Attract!");
		ShowAttract(0);
	} else if ( _attractOn>0 ) {
		// should switch between ShowAttract 1 and 2
	} else if ( _lastActivity<0 || inactive ) {
		if ( _attractOn == 0) {
			NosuchDebug("ShowAttract!");
			ShowAttract(1);
		}
	}
	if ( _textEraseTime > 0 && millinow > _textEraseTime ) {
		NosuchDebug("Erasing Text!");
		_textEraseTime = 0;
		ShowAttract(0);
		ShowChoice("","",0,0,0);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  Methods
////////////////////////////////////////////////////////////////////////////////////////////////////

void PaletteHost::rect(double x, double y, double w, double h) {
	// if ( w != 2.0f || h != 2.0f ) {
	// 	NosuchDebug("Drawing rect xy = %.3f %.3f  wh = %.3f %.3f",x,y,w,h);
	// }
	quad(x,y, x+w,y,  x+w,y+h,  x,y+h);
}
void PaletteHost::fill(NosuchColor c, double alpha) {
	m_filled = true;
	m_fill_color = c;
	m_fill_alpha = alpha;
}
void PaletteHost::stroke(NosuchColor c, double alpha) {
	// glColor4d(c.r()/255.0f, c.g()/255.0f, c.b()/255.0f, alpha);
	m_stroked = true;
	m_stroke_color = c;
	m_stroke_alpha = alpha;
}
void PaletteHost::noStroke() {
	m_stroked = false;
}
void PaletteHost::noFill() {
	m_filled = false;
}
void PaletteHost::background(int b) {
	NosuchDebug("PaletteHost::background!");
}
void PaletteHost::strokeWeight(double w) {
	glLineWidth((GLfloat)w);
}
void PaletteHost::rotate(double degrees) {
	glRotated(-degrees,0.0f,0.0f,1.0f);
}
void PaletteHost::translate(double x, double y) {
	glTranslated(x,y,0.0f);
}
void PaletteHost::scale(double x, double y) {
	glScaled(x,y,1.0f);
	// NosuchDebug("SCALE xy= %f %f",x,y);
}
void PaletteHost::quad(double x0, double y0, double x1, double y1, double x2, double y2, double x3, double y3) {
	NosuchDebug(2,"   Drawing quad = %.3f %.3f, %.3f %.3f, %.3f %.3f, %.3f %.3f",x0,y0,x1,y1,x2,y2,x3,y3);
	if ( m_filled ) {
		glBegin(GL_QUADS);
		NosuchColor c = m_fill_color;
		glColor4d(c.r()/255.0f, c.g()/255.0f, c.b()/255.0f, m_fill_alpha);
		glVertex2d( x0, y0); 
		glVertex2d( x1, y1); 
		glVertex2d( x2, y2); 
		glVertex2d( x3, y3); 
		glEnd();
	}
	if ( m_stroked ) {
		NosuchColor c = m_stroke_color;
		glColor4d(c.r()/255.0f, c.g()/255.0f, c.b()/255.0f, m_stroke_alpha);
		glBegin(GL_LINE_LOOP); 
		glVertex2d( x0, y0); 
		glVertex2d( x1, y1); 
		glVertex2d( x2, y2); 
		glVertex2d( x3, y3); 
		glEnd();
	}
	if ( ! m_filled && ! m_stroked ) {
		NosuchDebug("Hey, quad() called when both m_filled and m_stroked are off!?");
	}
}
void PaletteHost::triangle(double x0, double y0, double x1, double y1, double x2, double y2) {
	NosuchDebug(2,"Drawing triangle xy0=%.3f,%.3f xy1=%.3f,%.3f xy2=%.3f,%.3f",x0,y0,x1,y1,x2,y2);
	if ( m_filled ) {
		NosuchColor c = m_fill_color;
		glColor4d(c.r()/255.0f, c.g()/255.0f, c.b()/255.0f, m_fill_alpha);
		NosuchDebug(2,"   fill_color=%d %d %d alpha=%.3f",c.r(),c.g(),c.b(),m_fill_alpha);
		glBegin(GL_TRIANGLE_STRIP); 
		glVertex3d( x0, y0, 0.0f );
		glVertex3d( x1, y1, 0.0f );
		glVertex3d( x2, y2, 0.0f );
		glEnd();
	}
	if ( m_stroked ) {
		NosuchColor c = m_stroke_color;
		glColor4d(c.r()/255.0f, c.g()/255.0f, c.b()/255.0f, m_stroke_alpha);
		NosuchDebug(2,"   stroke_color=%d %d %d alpha=%.3f",c.r(),c.g(),c.b(),m_stroke_alpha);
		glBegin(GL_LINE_LOOP); 
		glVertex2d( x0, y0); 
		glVertex2d( x1, y1);
		glVertex2d( x2, y2);
		glEnd();
	}
	if ( ! m_filled && ! m_stroked ) {
		NosuchDebug("Hey, triangle() called when both m_filled and m_stroked are off!?");
	}
}

void PaletteHost::line(double x0, double y0, double x1, double y1) {
	// NosuchDebug("Drawing line xy0=%.3f,%.3f xy1=%.3f,%.3f",x0,y0,x1,y1);
	if ( m_stroked ) {
		NosuchColor c = m_stroke_color;
		glColor4d(c.r()/255.0f, c.g()/255.0f, c.b()/255.0f, m_stroke_alpha);
		// NosuchDebug(2,"   stroke_color=%d %d %d alpha=%.3f",c.r(),c.g(),c.b(),m_stroke_alpha);
		glBegin(GL_LINES); 
		glVertex2d( x0, y0); 
		glVertex2d( x1, y1);
		glEnd();
	} else {
		NosuchDebug("Hey, line() called when m_stroked is off!?");
	}
}

static double degree2radian(double deg) {
	return 2.0f * (double)M_PI * deg / 360.0f;
}

void PaletteHost::ellipse(double x0, double y0, double w, double h) {
	NosuchDebug(2,"Drawing ellipse xy0=%.3f,%.3f wh=%.3f,%.3f",x0,y0,w,h);
	if ( m_filled ) {
		NosuchColor c = m_fill_color;
		glColor4d(c.r()/255.0f, c.g()/255.0f, c.b()/255.0f, m_fill_alpha);
		NosuchDebug(2,"   fill_color=%d %d %d alpha=%.3f",c.r(),c.g(),c.b(),m_fill_alpha);
		glBegin(GL_TRIANGLE_FAN);
		double radius = w;
		glVertex2d(x0, y0);
		for ( double degree=0.0f; degree <= 360.0f; degree+=5.0f ) {
			glVertex2d(x0 + sin(degree2radian(degree)) * radius, y0 + cos(degree2radian(degree)) * radius);
		}
		glEnd();
	}
	if ( m_stroked ) {
		NosuchColor c = m_stroke_color;
		glColor4d(c.r()/255.0f, c.g()/255.0f, c.b()/255.0f, m_stroke_alpha);
		NosuchDebug(2,"   stroke_color=%d %d %d alpha=%.3f",c.r(),c.g(),c.b(),m_stroke_alpha);
		glBegin(GL_LINE_LOOP);
		double radius = w;
		for ( double degree=0.0f; degree <= 360.0f; degree+=5.0f ) {
			glVertex2d(x0 + sin(degree2radian(degree)) * radius, y0 + cos(degree2radian(degree)) * radius);
		}
		glEnd();
	}

	if ( ! m_filled && ! m_stroked ) {
		NosuchDebug("Hey, ellipse() called when both m_filled and m_stroked are off!?");
	}
}

void PaletteHost::polygon(PointMem* points, int npoints) {
	if ( m_filled ) {
		NosuchColor c = m_fill_color;
		glColor4d(c.r()/255.0f, c.g()/255.0f, c.b()/255.0f, m_fill_alpha);
		glBegin(GL_TRIANGLE_FAN);
		glVertex2d(0.0, 0.0);
		for ( int pn=0; pn<npoints; pn++ ) {
			PointMem* p = &points[pn];
			glVertex2d(p->x,p->y);
		}
		glEnd();
	}
	if ( m_stroked ) {
		NosuchColor c = m_stroke_color;
		glColor4d(c.r()/255.0f, c.g()/255.0f, c.b()/255.0f, m_stroke_alpha);
		glBegin(GL_LINE_LOOP);
		for ( int pn=0; pn<npoints; pn++ ) {
			PointMem* p = &points[pn];
			glVertex2d(p->x,p->y);
		}
		glEnd();
	}

	if ( ! m_filled && ! m_stroked ) {
		NosuchDebug("Hey, ellipse() called when both m_filled and m_stroked are off!?");
	}
}

void PaletteHost::popMatrix() {
	glPopMatrix();
}

void PaletteHost::pushMatrix() {
	glPushMatrix();
}

static PyObject*
getpythonfunc(PyObject *module, const char *name, const char *modname)
{
	PyObject *f = PyObject_GetAttrString(module, name);
	if (!(f && PyCallable_Check(f))) {
		NosuchErrorOutput("Unable to find python method %s in %s",name,modname);
		return NULL;
	}
	return f;
}

PyObject*
PaletteHost::python_lock_and_call(PyObject* func, PyObject *pArgs)
{
	lock_python();
	PyObject *msgValue = PyObject_CallObject(func, pArgs);
	unlock_python();
	return msgValue;
}

bool
PaletteHost::python_recompileModule(char *modulename)
{
	PyObject *pArgs;
	bool r = FALSE;

	if ( _recompileFunc == NULL ) {
		NosuchDebug("Hey, _recompileFunc is NULL!?");
		return FALSE;
	}

	pArgs = Py_BuildValue("(s)", modulename);
	if ( pArgs == NULL ) {
		NosuchDebug("Cannot create python arguments to recompile");
		goto getout;
	}            
	PyObject *msgValue = python_lock_and_call(_recompileFunc, pArgs);
	Py_DECREF(pArgs);
	if (msgValue == NULL) {
		NosuchDebug("Call to recompile of %s failed\n",modulename);
	} else if (msgValue == Py_None) {
		NosuchDebug("Call to recompile of %s returned None?\n",modulename);
	} else {
		char *msg = PyString_AsString(msgValue);
		r = (*msg != '\0') ? FALSE : TRUE;
		if ( r == FALSE ) {
			NosuchDebug("Call to recompile of %s failed, msg=%s\n",modulename,msg);
		}
		Py_DECREF(msgValue);
	}
getout:
	Py_XDECREF(_nosuchUtilModule);
	return r;
}

#define RANDONE (((double)rand())/RAND_MAX)
#define RANDB ((((double)rand())/RAND_MAX)*2.0f-1.0f)

void
PaletteHost::test_draw()
{
	for ( int i=0; i<1000; i++ ) {
		glColor4d(RANDONE,RANDONE,RANDONE,RANDONE);
		glBegin(GL_QUADS);
		glVertex2d(RANDB,RANDB);
		glVertex2d(RANDB,RANDB);
		glVertex2d(RANDB,RANDB);
		glVertex2d(RANDB,RANDB);
		glVertex2d(RANDB,RANDB);
		glEnd();
	}
}

#if 0
std::string
PaletteHost::python_process_osc()
{
	NosuchAssert(_callBoundFunc);

	PyObject *pArgs = Py_BuildValue("(O)",_processorOscFunc);
	if ( pArgs == NULL ) {
		return "Cannot create python arguments to _callBoundFunc";
	}            
	PyObject *msgobj = python_lock_and_call(_callBoundFunc, pArgs);
	Py_DECREF(pArgs);
	
	if (msgobj == NULL) {
		return "Call to _callBoundFunc failed";
	}
	if (msgobj == Py_None) {
		return "Call to _callBoundFunc returned None?";
	}
	std::string msg = std::string(PyString_AsString(msgobj));
	Py_DECREF(msgobj);
	if ( msg != "" ) {
		NosuchDebug("python_process_osc returned msg = %s",msg.c_str());
	}
	return msg;
}
#endif

std::string
PaletteHost::python_draw()
{
	NosuchAssert(_callBoundFunc);
	NosuchAssert(_processorDrawFunc);

	PyObject *pArgs = Py_BuildValue("(O)",_processorDrawFunc);
	if ( pArgs == NULL ) {
		return "Cannot create python arguments to _callBoundFunc";
	}            
	PyObject *msgobj = python_lock_and_call(_callBoundFunc, pArgs);
	Py_DECREF(pArgs);
	
	if (msgobj == NULL) {
		return "Call to _callBoundFunc failed";
	}
	if (msgobj == Py_None) {
		return "Call to _callBoundFunc returned None?";
	}
	std::string msg = std::string(PyString_AsString(msgobj));
	Py_DECREF(msgobj);
	if ( msg != "" ) {
		NosuchDebug("python_callbehaviour returned msg = %s",msg.c_str());
	}
	return msg;
}

PyObject*
PaletteHost::python_getProcessorObject(const char *btype)
{
	std::string dir = NosuchForwardSlash(NosuchFullPath(""));
	PyObject *pArgs = Py_BuildValue("(ss)", btype,dir.c_str());
	if ( pArgs == NULL ) {
		NosuchDebug("Cannot create python arguments to _getProcessorFunc");
		return NULL;
	}            
	PyObject *obj = python_lock_and_call(_getProcessorFunc, pArgs);
	Py_DECREF(pArgs);
	if (obj == NULL) {
		NosuchDebug("Call to _getProcessorFunc failed");
		return NULL;
	}
	if (obj == Py_None) {
		NosuchDebug("Call to _getProcessorFunc returned None?");
		return NULL;
	}
	return obj;
}

static PyObject* nosuchmedia_debug(PyObject* self, PyObject* args)
{
    const char* name;
 
    if (!PyArg_ParseTuple(args, "s", &name))
        return NULL;
 
	NosuchDebug("(from python) %s",name);
 
    Py_RETURN_NONE;
}
 
static PyObject* nosuchmedia_glVertex2d(PyObject* self, PyObject* args)
{
	double v1, v2;

    if (!PyArg_ParseTuple(args, "ff", &v1, &v2))
        return NULL;
 
	// NosuchDebug("(from python) glVertex2d %f %f",v1,v2);
	glVertex2d( v1,v2);
 
    Py_RETURN_NONE;
}
 
static PyObject* nosuchmedia_cursors(PyObject* self, PyObject* args)
{
	double v1, v2;

    if (!PyArg_ParseTuple(args, "ff", &v1, &v2))
        return NULL;
 
    Py_RETURN_NONE;
}
 
static PyObject* nosuchmedia_next_event(PyObject* self, PyObject* args)
{
	PyEvent* ev = RealPaletteHost->palette()->popPyEvent();
	if ( ev == NULL ) {
	    Py_RETURN_NONE;
	} else {
		PyObject* e = ev->python_object();
		delete ev;
		return e;
	}
}

static PyObject* nosuchmedia_region_define(PyObject* self, PyObject* args) {
	const char* region_name;
	const char* region_type;
	int sid_low;
	int sid_high;

	// NosuchDebug("(builtin) region_define");
    if (!PyArg_ParseTuple(args, "ssii", &region_name, &region_type, &sid_low, &sid_high))
        return NULL;

	Region* region;
	NosuchDebug(1,"region_define region=%s sid_low=%d sid_high=%d type=%s",region_name,sid_low,sid_high,region_type);
	if ( strcmp(region_type,"surface") == 0 ) {
		region = Palette::palette()->NewSurfaceNamed(region_name,sid_low,sid_high);
	} else if ( strcmp(region_type,"button") == 0 ) {
		region = Palette::palette()->NewButtonNamed(region_name,sid_low,sid_high);
	} else {
		NosuchDebug("Unrecognized region type in region_define: %s",region_type);
	}
    Py_RETURN_NONE;
}
 
static PyObject* nosuchmedia_region_change_sound(PyObject* self, PyObject* args) {
	const char* region_name;
	const char* sound_name;

	// NosuchDebug("(builtin) region_change_sound");
    if (!PyArg_ParseTuple(args, "ss", &region_name, &sound_name))
        return NULL;

	NosuchDebug(1,"region_change_sound region=%s sound=%s",region_name,sound_name);
	int ch = Palette::palette()->setRegionSound(region_name,sound_name);
    // Py_RETURN_NONE;
	return Py_BuildValue("i", ch);
}
 
static PyObject* nosuchmedia_note_on(PyObject* self, PyObject* args) {
	int sid;
	int ch;
	int milli;
	int pitch;

	// NosuchDebug("(builtin) note_on");
    if (!PyArg_ParseTuple(args, "iiii", &sid, &ch, &milli, &pitch))
        return NULL;

	NosuchDebug(1,"builtin nosuchmedia_note_on start");
	Palette::palette()->schedNewNoteInMilliseconds(sid,ch,milli,pitch);
	NosuchDebug(1,"builtin nosuchmedia_note_on end");

    Py_RETURN_NONE;
}
 
static PyObject* nosuchmedia_session_end(PyObject* self, PyObject* args) {
	int sid;

	NosuchDebug(1,"(builtin) session_end");
    if (!PyArg_ParseTuple(args, "i", &sid))
        return NULL;

	Palette::palette()->schedSessionEnd(sid);

    Py_RETURN_NONE;
}
 
static PyMethodDef PaletteHostMethods[] =
{
     {"debug", nosuchmedia_debug, METH_VARARGS, "Log debug output."},
     {"glVertex2d", nosuchmedia_glVertex2d, METH_VARARGS, "Invoke glVertext2d."},
     {"next_event", nosuchmedia_next_event, METH_VARARGS, "Get next event."},
     {"region_change_sound", nosuchmedia_region_change_sound, METH_VARARGS, "Change the sound of a region."},
     {"region_define", nosuchmedia_region_define, METH_VARARGS, "Specify a new region."},
     {"session_note_on", nosuchmedia_note_on, METH_VARARGS, "Generate MIDI note-on."},
     {"session_end", nosuchmedia_session_end, METH_VARARGS, "Generate MIDI note-offs."},
     {NULL, NULL, 0, NULL}
};
 
bool PaletteHost::python_init() {
	if ( ! Py_IsInitialized() ) {
		NosuchDebug("PaletteHost::initialize IS CALLING PY_INITIALIZE!");
		Py_Initialize();
	} else {
		NosuchDebug("PaletteHost::initialize is NOT calling PY_INITIALIZE, already running!");
	}

	(void) Py_InitModule("manifold.builtin", PaletteHostMethods);

	// We want to add our directory to the sys.path
	std::string script = NosuchSnprintf(
		"import time\nimport sys\n"
		"sys.path.insert(0,'%s')\n",
			NosuchForwardSlash(NosuchFullPath("python")).c_str()
		);
	PyRun_SimpleString(script.c_str());

	// PyObject *obj = PyString_FromString("nosuchutil");
    // _nosuchUtilModule = PyImport_Import(obj);
    // Py_DECREF(obj);

	const char *pyffleutil = "manifold.util";
	PyObject *pName = PyString_FromString(pyffleutil);
    _PyffleUtilModule = PyImport_Import(pName);
    Py_DECREF(pName);

	if ( _PyffleUtilModule == NULL) {
		python_disable("Unable to import pyffleutil module");
		return FALSE;
	}

	if (!(_recompileFunc = getpythonfunc(_PyffleUtilModule,"recompile",pyffleutil))) {
		python_disable("Can't get recompile function from manifold module?!");
		return FALSE;
	}

	if (!(_callBoundFunc=getpythonfunc(_PyffleUtilModule,"callboundfunc",pyffleutil))) {
		python_disable("Unable to find callboundfunc func");
		return FALSE;
	}

	if (!(_getProcessorFunc=getpythonfunc(_PyffleUtilModule,"getprocessor",pyffleutil))) {
		python_disable("Unable to find getprocessor func");
		return FALSE;
	}

#if 0
	NosuchDebug("Trying to recompile manifold.");
	if ( python_recompileModule("manifold") == FALSE ) {
		python_disable("Unable to recompile manifold module");
		return FALSE;
	}
	NosuchDebug("recompile manifold worked.");
#endif

	const char *processor = "Default";
	if ( !python_change_processor(processor) ) {
		std::string msg = NosuchSnprintf("Unable to change processor to %s",processor);
		python_disable(msg.c_str());
		return FALSE;
	}

	// python_draw();

	return TRUE;
}

bool
PaletteHost::python_change_processor(const char* behavename) {

	if ( !(_processorObj = python_getProcessorObject(behavename))) {
		NosuchErrorOutput("Can't get behavename=%s!",behavename);
		return FALSE;
	}
	if ( !(_processorDrawFunc = getpythonfunc(_processorObj, "processOpenGL", behavename)) ) {
		NosuchErrorOutput("Can't get processOpenGL from behavename=%s!",behavename);
		return FALSE;
	}
#if 0
	if ( !(_processorOscFunc = getpythonfunc(_processorObj, "processOSC", behavename)) ) {
		return FALSE;
	}
#endif
	return TRUE;
}

void PaletteHost::python_disable(std::string msg) {
	NosuchErrorOutput("python is being disabled!  msg=%s",msg.c_str());
	_python_enabled = FALSE;
}

bool PaletteHost::python_reloadPyffleUtilModule() {

	PyObject* newmod = PyImport_ReloadModule(_PyffleUtilModule);
	if ( newmod == NULL) {
		python_disable("Unable to reload manifold module");
		return FALSE;
	}
	_PyffleUtilModule = newmod;

	return TRUE;
}

int PaletteHost::python_runfile(std::string filename) {
	std::string fullpath = NosuchFullPath("python") + "\\" + filename;
	std::ifstream f(fullpath.c_str(), std::ifstream::in);
	if ( ! f.is_open() ) {
		NosuchErrorOutput("Unable to open python file: %s",fullpath.c_str());
		return 1;
	}
	std::string contents;
	std::string line;
	while (!std::getline(f,line,'\n').eof()) {
		contents += (line+"\n");
	}
	f.close();
	int r = PyRun_SimpleString(contents.c_str());
	if ( r != 0 ) {
		NosuchErrorOutput("Error executing contents of: %s",fullpath.c_str());
	}
	return r;
}

static bool
istrue(std::string s)
{
	return(s == "true" || s == "True" || s == "1");
}

void PaletteHost::config_and_initialize() {
	std::string filename = ConfigFileName("default.mnf");
	NosuchDebug("Reading PaletteHost configuration from %s",filename.c_str());
	std::ifstream f(filename.c_str(), std::ifstream::in | std::ifstream::binary);
	if ( ! f.is_open() ) {
		NosuchErrorOutput("Unable to open config file: %s",filename.c_str());
	} else {
		read_config(f);
		f.close();
	}
	if ( _python_enabled ) {
		python_init();
	}
	NosuchDebug("After reading PaletteHost configuration from %s",filename.c_str());
}

// Return everything after the '=' (and whitespace)
std::string
everything_after_char(std::string line, char lookfor = '=')
{
	const char *p = line.c_str();
	const char *q = strchr(p,lookfor);
	if ( q == NULL ) {
		NosuchDebug("Invalid format (no =): %s",p);
		return "";
	}
	q++;
	while ( *q != 0 && isspace(*q) ) {
		q++;
	}
	size_t len = strlen(q);
	if ( q[len-1] == '\r' ) {
		len--;
	}
	return std::string(q,len);
}

void PaletteHost::read_config(std::ifstream& f) {

	std::string line;
	std::string current_synth = "";
	int current_soundset = -1;
	std::string current_soundbank_name;
	int current_soundbank = -1;

	while (!std::getline(f,line,'\n').eof()) {
		std::vector<std::string> words = NosuchSplitOnAnyChar(line," \t\r=");
		if ( words.size() == 0 ) {
			continue;
		}
		std::string word0 = words[0];
		if ( word0 == "" || word0[0] == '#' ) {
			continue;
		}
		std::string word1 = (words.size()>1) ? words[1] : "";
		if ( word0 == "channel" ) {
			if ( words.size() < 3 ) {
				NosuchDebug("Invalid format (too short): %s",line.c_str()); 
				continue;
			}
			int chan = atoi(word1.c_str());
			Synths[chan] = words[2];
		} else if ( word0 == "include" ) {
			if ( words.size() < 2 ) {
				NosuchDebug("Invalid format (too short): %s",line.c_str()); 
				continue;
			}
			// as a hack to eliminate the need for an '=' on the
			// include line, we take everything after the 'e'
			// (last character in "include").
			std::string basename = everything_after_char(line,'e');
			std::string filename = ConfigFileName(basename);
			std::ifstream incf(filename.c_str(), std::ifstream::in | std::ifstream::binary);
			if ( ! incf.is_open() ) {
				if ( basename.find_first_of("local") == 0 ) {
					// This is not necessarily an error
					NosuchDebug("Local include file not defined: %s",filename.c_str());
				} else {
					NosuchErrorOutput("Unable to open include file: %s",filename.c_str());
				}
			} else {
				read_config(incf);
				incf.close();
			}
		} else if ( word0 == "sid" ) {
			if ( words.size() < 4 ) {
				NosuchDebug("Invalid format (too short): %s",line.c_str()); 
				continue;
			}
			int sidnum = atoi(word1.c_str());
			// std::string sid = sidString(sidnum,"");
			std::string name = words[3];
			std::string word2 = words[2];
			Region* rgn;
			if ( word2 == "surface" ) {
				rgn = _palette->NewSurfaceNamed(name,sidnum,sidnum+32);
			} else if ( word2 == "button" ) {
				rgn = _palette->NewButtonNamed(name,sidnum,sidnum+32);
			} else {
				NosuchDebug("Invalid format (expecting surface or button): %s",line.c_str()); 
				continue;
			}
			NosuchDebug("New Region of type=%s, name=%s sidnum=%d rid=%d",word2.c_str(),name.c_str(),sidnum,rgn->id);
		} else if ( word0 == "debug" ) {
			if ( words.size() < 3 ) {
				NosuchDebug("Invalid format (too short): %s",line.c_str()); 
				continue;
			}
			if ( word1 == "level" ) {
				NosuchDebugLevel = atoi(everything_after_char(line).c_str());
			} else if ( word1 == "tologfile" ) {
				NosuchDebugToLog = istrue(everything_after_char(line));
			} else if ( word1 == "toconsole" ) {
				NosuchDebugToConsole = istrue(everything_after_char(line));
			} else if ( word1 == "autoflush" ) {
				NosuchDebugAutoFlush = istrue(everything_after_char(line));
			}
		} else if ( word0 == "python" ) {
			if ( words.size() < 3 ) {
				NosuchDebug("Invalid format (too short): %s",line.c_str()); 
				continue;
			}
			if ( word1 == "enabled" ) {
				_python_enabled = istrue(everything_after_char(line));
			} else if ( word1 == "path" ) {
				_python_path = everything_after_char(line).c_str();
			}
		} else if ( word0 == "midi" ) {
			_do_midi = true;
			if ( words.size() < 3 ) {
				NosuchDebug("Invalid format (too short): %s",line.c_str()); 
				continue;
			}
			if ( word1 == "input" ) {
				_midi_input = everything_after_char(line);
			} else if ( word1 == "output" ) {
				_midi_output = everything_after_char(line);
				// std::string progname = everything_after_char(line);
			}
			NosuchDebug("MIDI! _do_midi set to true");
		} else if ( word0 == "synth" ) {
			if ( words.size() < 2 ) {
				NosuchDebug("Invalid format (too short): %s",line.c_str()); 
				continue;
			}
			current_synth = word1;
		} else if ( word0 == "program" ) {
			if ( current_synth == "" ) {
				NosuchDebug("Invalid format (program seen before synth): %s",line.c_str()); 
				continue;
			}
			if ( words.size() < 2 ) {
				NosuchDebug("Invalid format (too short): %s",line.c_str()); 
				continue;
			}
			int pnum = atoi(word1.c_str());
			std::string progname = everything_after_char(line);
			NosuchDebug(1,"Program %d = ((%s))",pnum,progname.c_str());
			Sounds[progname] = Sound(current_synth,pnum);
		} else if ( word0 == "soundbank" ) {
			if ( words.size() < 3 ) {
				NosuchDebug("Invalid format (too short): %s",line.c_str()); 
				continue;
			}
			current_soundbank = atoi(word1.c_str());
			NosuchAssert(current_soundbank >=0 && current_soundbank < NUM_SOUNDBANKS);
			current_soundbank_name = words[2];
		} else if ( word0 == "soundset" ) {
			if ( words.size() < 2 ) {
				NosuchDebug("Invalid format (too short): %s",line.c_str()); 
				continue;
			}
			current_soundset = atoi(word1.c_str());
			NosuchAssert(current_soundset >=0 && current_soundset < NUM_SOUNDSETS);
		} else if ( word0 == "sound" ) {
			if ( words.size() < 3 ) {
				NosuchDebug("Invalid format (too short): %s",line.c_str()); 
				continue;
			}
			int soundnum = atoi(word1.c_str());
			NosuchAssert(soundnum >=0 && soundnum < NUM_SOUNDS_IN_SET);
			std::string soundname = everything_after_char(line);
			SoundBank[current_soundbank][current_soundset][soundnum] = soundname;
		} else {
			NosuchDebug("Invalid format (unrecognized first word): %s",line.c_str()); 
			continue;
		}
	}
}

bool PaletteHost::initStuff() {

	NosuchDebug(1,"PaletteHost::initStuff starts");

	// test_stuff();

	bool r = true;
	try {
		// static initializations
		RegionParams::Initialize();
		ChannelParams::Initialize();
		Scale::initialize();
		Cursor::initialize();
		GraphicBehaviour::initialize();
		MusicBehaviour::initialize();
		Sound::initialize();
		Sprite::initialize();
		Palette::initialize();
	
		_palette = new Palette(this);

		// Not static, config gets read here.
		config_and_initialize();

		_palette->init_loops();

		if ( _midi_output == "" ) {
			NosuchDebug("Warning: MIDI output wasn't defined in configuration!");
			_midi_output = "Midi Yoke:  1";
		}
		if ( _midi_input == "" ) {
			NosuchDebug("Warning: MIDI input wasn't defined in configuration!");
			_midi_input = "Midi Yoke:  2";
		}
		_scheduler->StartMidi(_midi_input,_midi_output);
		_scheduler->SetClickClient(_looper);
		_scheduler->SetSchedulerClient(this);

		_palette->initRegionSounds();

		_palette->now = Pt_Time();

		NosuchDebug(1,"CREATING new PaletteDaemon!!");
		_daemon = new PaletteDaemon(this,
			_do_tuio?_osc_input_port:-1,_osc_input_host,_http_input_port);

		/*
		_palette->ConfigLoad("default");
		_palette->ConfigLoad("default");
		*/

	} catch (NosuchException& e) {
		NosuchDebug("NosuchException: %s",e.message());
		r = false;
	} catch (...) {
		// Does this really work?  Not sure
		NosuchDebug("Some other kind of exception occured!?");
		r = false;
	}
	NosuchDebug(2,"PaletteHost::initStuff returns %s\n",r?"true":"false");
	return r;
}

void
PaletteHost::CursorDownNotification(Cursor* c) {
	Region* r = c->region();
	NosuchAssert(r);
	if ( ! r->Looping() ) {
		return;
	}
#if 0
	// XXX - UNFINISHED CURSOR MOTION LOOPING!
	NosuchDebug("Should be adding to loop cursor=%s",c->DebugString().c_str());
	NosuchCursorMotion* cm = new NosuchCursorMotion(0,c->pos(),c->depth());
	CursorLoopNotification(cm,r->loop());
#endif
}

void
PaletteHost::CursorLoopNotification(NosuchCursorMotion* cm, NosuchLoop* lp) {
	int clk = lp->click();
	SchedEvent* ev = new SchedEvent(cm,clk,lp->id());
	int nn = lp->AddLoopEvent(ev);
}

void
PaletteHost::OutputNotificationMidiMsg(MidiMsg* mm, int sidnum) {
	// The sid can be a TUIO session ID or a loop id
	if ( sidnum < 0 ) {
		NosuchDebug(1,"OutputNotificationMidiMsg ignoring sidnum=%d",sidnum);
		return;
	}
	Region* r = _palette->RegionForSid(sidnum);
	if ( r == NULL ) {
		NosuchDebug(1,"OutputNotificationMidiMsg no region for sid=%d?",sidnum);
		return;
	}
	if ( r->type != Region::SURFACE ) {
		NosuchDebug(1,"OutputNotificationMidiMsg region for sid=%d surface?",sidnum);
		return;
	}
	NosuchAssert(r);

	if ( isSidCursor(sidnum) ) {
		// If the output is coming from a cursor, then we see if looping is on

		NosuchDebug(2,"PaletteHost OutputNotificationMidiMsg for Cursor, sid=%d",sidnum);
		r->OutputNotificationMidiMsg(mm,sidnum);

		if ( r->Looping() ) {
			NosuchLoop* lp = r->loop();
			int clk = lp->click();
			MidiMsg* newmm = mm->clone();
			if ( newmm ) {
				SchedEvent* ev = new SchedEvent(newmm,clk,lp->id());
				NosuchDebug(2,"Creating new SchedEvent = %d",ev);
				int nn = lp->AddLoopEvent(ev);
				int maxnotes = r->params.loopnotes;
				NosuchDebug(1,"Added LoopEvent lp=%d nnotes=%d maxnotes=%d ev=%s",
					lp->id(),nn,maxnotes,ev->DebugString().c_str());
				if ( nn > maxnotes ) {
					NosuchDebug(1,"Removing note, maxnotes=%d nnotes=%d",maxnotes,nn);
					r->loop()->removeOldestNoteOn();
				}
			}
			// NosuchDebug("  After AddLoopEvent, lp = %s",lp->DebugString().c_str());
		}
	} else {
		// It's a MidiMsg from Looper output
		// NosuchDebug(2,"PaletteHost OutputNotificationMidiMsg for Loop, sid=%s mm=%s",sid.c_str(),mm->DebugString().c_str());
		r->OutputNotificationMidiMsg(mm,sidnum);
	}
}


MMTT_SharedMemHeader*
PaletteHost::outlines_memory() {

	if ( !_sharedmem_outlines ) {
		return NULL;
	}
	void *data = _sharedmem_outlines->getMemory();
	if ( !data ) {
			return NULL;
	}
	return (MMTT_SharedMemHeader*) data;
}

DWORD PaletteHost::PaletteHostProcessOpenGL(ProcessOpenGLStruct *pGL)
{
	gl_frame++;
	// NosuchDebug("PaletteHostProcessOpenGL");
	if ( gl_shutting_down ) {
		return FF_SUCCESS;
	}
	if ( disabled ) {
		return FF_SUCCESS;
	}

	if ( ! initialized ) {
		NosuchDebug(1,"PaletteHost calling initStuff()");
		if ( ! initStuff() ) {
			NosuchDebug("PaletteHost::initStuff failed, disabling plugin!");
			disabled = true;
			return FF_FAIL;
		}
		initialized = true;
		RealPaletteHost = this;
	}

#ifdef FRAMELOOPINGTEST
	static int framenum = 0;
	static bool framelooping = FALSE;
#endif

#ifdef DO_HTTP_AND_OSC_IN_MAIN_THREAD
	extern long nsprites;
	static long check = 0;
	if ( check++ > 10 ) {
		CheckText(Palette::now);
		_daemon->Check();
		check = 0;
	}
#endif

	// NosuchDebug("About to Lock json A");
	NosuchLock(&json_mutex,"json");
	// NosuchDebug("After Lock json A");
	if (json_pending) {
		// Execute json stuff and generate response
		// NosuchDebug("####### EXECUTING json method=%s now=%d",json_method.c_str(),Palette::now);
		json_result = ExecuteJsonAndCatchExceptions(json_method, json_params, json_id);
		json_pending = false;
		// NosuchDebug("####### Signaling json_cond! now=%d",Palette::now);
		int e = pthread_cond_signal(&json_cond);
		if ( e ) {
			NosuchDebug("ERROR from pthread_cond_signal e=%d\n",e);
		}
		// NosuchDebug("####### After signaling now=%d",Palette::now);
	}
	// NosuchDebug("About to UnLock json A");
	NosuchUnlock(&json_mutex,"json");

	bool passthru = FALSE;
	if ( passthru == TRUE && pGL != NULL ) {
		if (pGL->numInputTextures<1)
			return FF_FAIL;

		if (pGL->inputTextures[0]==NULL)
			return FF_FAIL;
  
		FFGLTextureStruct &Texture = *(pGL->inputTextures[0]);

		//bind the texture handle
		glBindTexture(GL_TEXTURE_2D, Texture.Handle);

		 //enable texturemapping
		glEnable(GL_TEXTURE_2D);
		
		//get the max s,t that correspond to the 
		//width,height of the used portion of the allocated texture space
		FFGLTexCoords maxCoords = GetMaxGLTexCoords(Texture);
		glColor4f(1.0, 1.0, 1.0, 1.0);
		glBegin(GL_QUADS);
		glTexCoord2d(0.0, 0.0);					glVertex2f(-1,-1);
		glTexCoord2d(0.0, maxCoords.t);			glVertex2f(-1,1);
		glTexCoord2d(maxCoords.s, maxCoords.t); glVertex2f(1,1);
		glTexCoord2d(maxCoords.s, 0.0);			glVertex2f(1,-1);
		glEnd();
		
		//unbind the texture
		glBindTexture(GL_TEXTURE_2D, 0);
	}

#if 0
	// Draw line just to show we're alive
	glColor4d(0.0, 0.0, 1.0, 1.0);
	glBegin(GL_LINES); 
	glVertex2d( -0.5, -0.5); 
	glVertex2d( 0.5, 0.5); 
	glEnd(); 
#endif

	lock_paletteHost();

	MMTT_SharedMemHeader *mem = NULL;

	openSharedMemOutlines();

	if ( _sharedmem_outlines ) {

		_sharedmem_outlines->lock();

		void *data = _sharedmem_outlines->getMemory();
		if ( !data ) {
			NosuchDebug("PaletteHost:: NULL returned from getMemory of Shared Memory!  (B)");
		} else {
			mem = (MMTT_SharedMemHeader*) data;
			mem->buff_to_display = BUFF_UNSET;
			if ( mem->buff_to_display_next == BUFF_UNSET ) {
				// Use the buffer that was displayed last time
				if ( mem->buff_displayed_last_time == BUFF_UNSET ) {
					NosuchDebug("HEY!  Both buff_to_display_next and buff_displayed_last_time are UNSET??");
					// Leave buff_to_display set to BUFF_UNSET
				} else {
					mem->buff_to_display = mem->buff_displayed_last_time;
				}
			} else {
				mem->buff_to_display = mem->buff_to_display_next;
				if ( mem->buff_displayed_last_time != BUFF_UNSET ) {
					mem->buff_inuse[mem->buff_displayed_last_time] = false;
				}
				mem->buff_displayed_last_time = mem->buff_to_display_next;
				mem->buff_to_display_next = BUFF_UNSET;
				// The buff_inuse flags are unchanged;
			}
		}
		_sharedmem_outlines->unlock();
	}

	bool gotexception = false;
	try {
		CATCH_NULL_POINTERS;

		int tm = _palette->now;
		int begintm = _palette->now;
		int endtm = Pt_Time();
		NosuchDebug(2,"ProcessOpenGL tm=%d endtm=%d dt=%d",tm,endtm,(endtm-tm));

		if ( _python_enabled ) {
			python_draw();
		}

		if ( mem != NULL && mem->buff_to_display != BUFF_UNSET ) {
			// draw things from buff_to_display
			// NOTE: we don't hold a lock on the shared memory!
			_palette->process_cursors_from_buff(mem);
			// _palette->make_sprites_along_outlines(mem);
		}

		int midimsg;
		int midilimit = 16;  // max number of messages to process
		while ( _scheduler->GetMidiMsg(&midimsg) && midilimit-- > 0 ) {
			// NosuchDebug("GOT MIDI in ProcessOpenGL! midimsg=0x%x",midimsg);
			_palette->process_midi_input(midimsg);
		}

		glDisable(GL_TEXTURE_2D); 
		glEnable(GL_BLEND); 
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
		glLineWidth((GLfloat)3.0f);

		int ndt = 1;
		int n;
		for ( n=1; n<=ndt; n++ ) {
			tm = (int)(begintm + 0.5 + n * ((double)(endtm-begintm)/(double)ndt));
			if ( tm > endtm ) {
				tm = endtm;
			}
			int r = _palette->draw();

			if ( _dotest ) {
				test_draw();
			}
			_palette->advanceTo(tm);
			if ( r > 0 ) {
				NosuchDebug("Palette::draw returned failure? (r=%d)\n",r);
				gotexception = true;
				break;
			}
		}
	} catch (NosuchException& e ) {
		NosuchDebug("NosuchException in Palette::draw : %s",e.message());
		gotexception = true;
	} catch (...) {
		NosuchDebug("UNKNOWN Exception in Palette::draw!");
		gotexception = true;
	}

	if ( gotexception && disable_on_exception ) {
		NosuchDebug("DISABLING PaletteHost due to exception!!!!!");
		disabled = true;
	}

	unlock_paletteHost();

	glDisable(GL_BLEND); 
	glEnable(GL_TEXTURE_2D); 
	// END NEW CODE

#ifdef FRAMELOOPINGTEST
	int w = Texture.Width;
	int h = Texture.Height;
#define NFRAMES 300
	static GLubyte* pixelArray[NFRAMES];
	if ( framelooping ) {
		glRasterPos2i(-1,-1);
		glDrawPixels(w,h,GL_RGB,GL_UNSIGNED_BYTE,pixelArray[framenum]);
		framenum = (framenum+1)%NFRAMES;
	} else {
		if ( framenum < NFRAMES ) {
			pixelArray[framenum] = new GLubyte[w*h*3];
			glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,pixelArray[framenum]);
			framenum++;
		} else {
			framelooping = TRUE;
			framenum = 0;
		}
	}
#endif

	//disable texturemapping
	glDisable(GL_TEXTURE_2D);
	
	//restore default color
	glColor4d(1.f,1.f,1.f,1.f);
	
	return FF_SUCCESS;
}

void PaletteHost::lock_paletteHost() {
	// NosuchLock(&palette_mutex,"paletteHost");
}

void PaletteHost::unlock_paletteHost() {
	// NosuchUnlock(&palette_mutex,"paletteHost");
}

void PaletteHost::lock_python() {
	NosuchLock(&python_mutex,"python");
}

void PaletteHost::unlock_python() {
	NosuchUnlock(&python_mutex,"python");
}

#if 0
DWORD PaletteHost::GetPaletteHostParameter(DWORD dwIndex)
{
	return FF_FAIL;  // no parameters
}

DWORD PaletteHost::SetPaletteHostParameter(const SetPaletteHostParameterStruct* pParam)
{
	return FF_FAIL;  // no parameters
}
#endif

bool has_invalid_char(const char *nm)
{
	for ( const char *p=nm; *p!='\0'; p++ ) {
		if ( ! isalnum(*p) )
			return true;
	}
	return false;
}

std::string PaletteHost::jsonDoubleResult(double r, const char *id) {
	return NosuchSnprintf("{ \"jsonrpc\": \"2.0\", \"result\": %f, \"id\": \"%s\" }",r,id);
}

std::string PaletteHost::jsonIntResult(int r, const char *id) {
	return NosuchSnprintf("{ \"jsonrpc\": \"2.0\", \"result\": %d, \"id\": \"%s\" }\r\n",r,id);
}

std::string PaletteHost::jsonStringResult(std::string r, const char *id) {
	return NosuchSnprintf("{ \"jsonrpc\": \"2.0\", \"result\": \"%s\", \"id\": \"%s\" }\r\n",r.c_str(),id);
}

std::string PaletteHost::jsonMethError(std::string e, const char *id) {
	return jsonError(-32602, e,id);
}

std::string PaletteHost::jsonError(int code, std::string e, const char* id) {
	return NosuchSnprintf("{ \"jsonrpc\": \"2.0\", \"error\": {\"code\": %d, \"message\": \"%s\" }, \"id\":\"%s\" }\r\n",code,e.c_str(),id);
}

std::string PaletteHost::jsonConfigResult(std::string name, const char *id) {

	// Remove the filename suffix on the config name
	int suffindex = name.length() - Palette::configSuffix.length();
	if ( suffindex > 0 && name.substr(suffindex) == Palette::configSuffix ) {
		name = name.substr(0,name.length()-Palette::configSuffix.length());
	}
	return jsonStringResult(name,id);
}

std::string needString(std::string meth,cJSON *params,std::string nm) {

	cJSON *c = cJSON_GetObjectItem(params,nm.c_str());
	if ( ! c ) {
		throw NosuchException("Missing %s argument on %s method",nm.c_str(),meth.c_str());
	}
	if ( c->type != cJSON_String ) {
		throw NosuchException("Unexpected type for %s argument to %s method, expecting string",nm.c_str(),meth.c_str());
	}
	return c->valuestring;
}

std::string needRegionOrMidiNum(std::string meth, cJSON *params, int* idnum) {
	cJSON *cregion = cJSON_GetObjectItem(params,"region");
	cJSON *cmidi = cJSON_GetObjectItem(params,"midi");
	if ( !cregion && !cmidi ) {
		throw NosuchException("Missing region or midi argument on %s method",meth.c_str());
	}
	std::string idtype;
	cJSON *c;
	if ( cregion ) {
		c = cregion;
		idtype = "region";
	} else {
		c = cmidi;
		idtype = "midi";
	}
	if ( c->type != cJSON_Number ) {
		throw NosuchException("Unexpected type for %s argument to %s method, expecting number",idtype.c_str(),meth.c_str());
	}
	*idnum = c->valueint;
	return idtype;
}

int needInt(std::string meth,cJSON *params,std::string nm) {
	cJSON *c = cJSON_GetObjectItem(params,nm.c_str());
	if ( ! c ) {
		throw NosuchException("Missing %s argument on %s method",nm.c_str(),meth.c_str());
	}
	if ( c->type != cJSON_Number ) {
		throw NosuchException("Unexpected type for %s argument to %s method, expecting number",nm.c_str(),meth.c_str());
	}
	return c->valueint;
}

double needDouble(std::string meth,cJSON *params,std::string nm) {
	cJSON *c = cJSON_GetObjectItem(params,nm.c_str());
	if ( ! c ) {
		throw NosuchException("Missing %s argument on %s method",nm.c_str(),meth.c_str());
	}
	if ( c->type != cJSON_Number ) {
		throw NosuchException("Unexpected type for %s argument to %s method, expecting double",nm.c_str(),meth.c_str());
	}
	return (double)(c->valuedouble);
}

void needParams(std::string meth, cJSON* params) {
	if(params==NULL) {
		throw NosuchException("No parameters on %s method?",meth.c_str());
	}
}

std::string PaletteHost::ExecuteJsonAndCatchExceptions(std::string meth, cJSON *params, const char *id) {
	std::string r;
	try {
		CATCH_NULL_POINTERS;

		r = ExecuteJson(meth,params,id);
	} catch (NosuchException& e) {
		std::string s = NosuchSnprintf("NosuchException in ProcessJson!! - %s",e.message());
		r = error_json(-32000,s.c_str(),id);
	} catch (...) {
		// This doesn't seem to work - it doesn't seem to catch other exceptions...
		std::string s = NosuchSnprintf("Some other kind of exception occured in ProcessJson!?");
		r = error_json(-32000,s.c_str(),id);
	}
	return r;
}

std::string PaletteHost::ExecuteJson(std::string meth, cJSON *params, const char *id) {

	static std::string errstr;  // So errstr.c_str() stays around, but I'm not sure that's now needed

	if ( meth == "debug_tail" ) {
#if 0
		cJSON *c_amount = cJSON_GetObjectItem(params,"amount");
		if ( ! c_amount ) {
			return error_json(-32000,"Missing amount argument",id);
		}
		if ( c_amount->type != cJSON_String ) {
			return error_json(-32000,"Expecting string type in amount argument to get",id);
		}
#endif
#ifdef DEBUG_DUMP_BUFFER
		std::string s = NosuchDebugDumpBuffer();
		std::string s2 = NosuchEscapeHtml(s);
		std::string result = 
			"{\"jsonrpc\": \"2.0\", \"result\": \""
			+ s2
			+ "\", \"id\": \""
			+ id
			+ "\"}";
		return(result);
#else
		return error_json(-32000,"DEBUG_DUMP_BUFFER not defined",id);
#endif
	}
	if ( meth == "_echo" || meth == "echo" ) {
		cJSON *c_value = cJSON_GetObjectItem(params,"value");
		if ( ! c_value ) {
			return error_json(-32000,"Missing value argument",id);
		}
		if ( c_value->type != cJSON_String ) {
			return error_json(-32000,"Expecting string type in value argument to echo",id);
		}
		return jsonStringResult(c_value->valuestring,id);
	}
	if (meth == "looping_on") {
		_palette->SetAllLooping(true,DEFAULT_LOOPFADE);
		return jsonIntResult(0,id);
	}
	if (meth == "looping_off") {
		_palette->SetAllLooping(false,-1);
		return jsonIntResult(0,id);
	}
	if (meth == "ANO") {
		_scheduler->ANO();
		return jsonIntResult(0,id);
	}
	if (meth == "clear_all") {
		palette()->SetAllLooping(false,DEFAULT_LOOPFADE);
		palette()->ClearAllLoops(true);
		scheduler()->ANO();
		return jsonIntResult(0,id);
	}
	if (meth == "range_full") {
		palette()->SetAllFullRange(true);
		return jsonIntResult(0,id);
	}
	if (meth == "range_normal") {
		palette()->SetAllFullRange(false);
		return jsonIntResult(0,id);
	}
	if (meth == "quantize_on") {
		NosuchDebug("doquantize is set to true");
		palette()->params.doquantize = true;
		return jsonIntResult(0,id);
	}
	if (meth == "quantize_off") {
		NosuchDebug("doquantize is set to false");
		palette()->params.doquantize = false;
		return jsonIntResult(0,id);
	}
	if (meth == "minmove_zero") {
		NosuchDebug("minmove is set to 0.0");
		for ( size_t i=0; i<palette()->_regions.size(); i++ ) {
			Region* region = palette()->_regions[i];
			region->params.minmove = 0.0;
			region->params.minmovedepth = 0.0;
		}
		return jsonIntResult(0,id);
	}
	if (meth == "minmove_default") {
		NosuchDebug("minmove is set to 0.05");
		for ( size_t i=0; i<palette()->_regions.size(); i++ ) {
			Region* region = palette()->_regions[i];
			region->params.minmove = 0.025;
			region->params.minmovedepth = 0.025;
		}
		return jsonIntResult(0,id);
	}
	if (meth == "tempo_slow") {
		NosuchScheduler::SetTempoFactor(2.0);
		return jsonIntResult(0,id);
	}
	if (meth == "tempo_fast") {
		NosuchScheduler::SetTempoFactor(1.0);
		return jsonIntResult(0,id);
	}
	if (meth == "tonic_change") {
		MusicBehaviour::tonic_change(_palette);
		return jsonIntResult(0,id);
	}
	if (meth == "tonic_reset") {
		MusicBehaviour::tonic_reset(_palette);
		return jsonIntResult(0,id);
	}
	if (meth == "randvisual") {
		int eset = rand() % NumEffectSet();
		LoadEffectSet(eset);
		return jsonIntResult(0,id);
	}
	if (meth == "randgraphic") {
		std::string name;
		std::string err = _palette->ConfigLoadRand(name);
		if ( err != "" ) {
			throw NosuchException("Error in ConfigLoadRand - %s",err.c_str());
		}
		return jsonIntResult(0,id);
	}
	if (meth == "config_load") {
		std::string name = needString(meth,params,"name");
		int r = needInt(meth,params,"region");
		if ( r != -1 ) {
			throw NosuchException("Can't handle config for for non-global region (%d)",r);
		}
		std::string err = _palette->ConfigLoad(name);
		if ( err != "" ) {
			throw NosuchException("Error in ConfigLoad for name=%s, err=%s",name.c_str(),err.c_str());
		}
		return jsonConfigResult(name,id);
	}
	if (meth == "config_next" || meth == "config_prev") {
		std::string name = needString(meth,params,"name");
		int r = needInt(meth,params,"region");
		if ( r != -1 ) {
			throw NosuchException("Can't handle config for for non-global region (%d)",r);
		}
		int dir = ((meth == "config_next") ? 1 : -1);
		std::string cn = _palette->ConfigNext(name,dir);
		std::string err = _palette->ConfigLoad(cn);
		if ( err != "" ) {
			throw NosuchException("Error in ConfigLoad for cn=%s err=%s",cn.c_str(),err.c_str());
		}
		return jsonConfigResult(cn,id);
	}
	if (meth == "config_rand") {
		int r = needInt(meth,params,"region");
		if ( r != -1 ) {
			throw NosuchException("Can't handle config for for non-global region (%d)",r);
		}
		std::string name;
		std::string err = _palette->ConfigLoadRand(name);
		if ( err != "" ) {
			throw NosuchException("Error in ConfigLoadRand, err=%s",err.c_str());
		}
		return jsonConfigResult(name,id);
	}
	if (meth == "config_overwrite" || meth == "config_savenew")  {
		std::string name = needString(meth,params,"name");
		int r = needInt(meth,params,"region");
		if ( r != -1 ) {
			throw NosuchException("Can't handle config for for non-global region (%d)",r);
		}
		if ( meth == "config_savenew" ) {
			name = _palette->ConfigNew(name);
			NosuchDebug("CONFIGNEW returned name=%s",name.c_str());
		}
		NosuchDebug("Config is saving name=%s",name.c_str());
		_palette->ConfigSave(name);
		return jsonConfigResult(name,id);
	}
	if (meth == "button") {
		needParams(meth,params);
		std::string name = needString(meth,params,"name");
		Region* r = _palette->GetRegionNamed(name);
		r->buttonDown(name);
		r->buttonUp(name);
		return jsonIntResult(0,id);
	}
	if (meth == "set") {
		needParams(meth,params);
		std::string name = needString(meth,params,"name");
		std::string val = needString(meth,params,"value");
		int idnum;
		std::string idtype = needRegionOrMidiNum(meth,params, &idnum);
		NosuchDebug("SET name=%s val=%s idnum=%d",name.c_str(),val.c_str(),idnum);

		std::string s;

		if ( idnum == MAGIC_VAL_FOR_PALETTE_PARAMS ) {
				_palette->params.Set(name,val);
				s = _palette->params.Get(name);
		} else if ( idtype == "region" ) {
			int r = idnum;
			if ( r == MAGIC_VAL_FOR_OVERRIDE_PARAMS ) {
				if ( val == Palette::UNSET_STRING ) {
					_palette->regionOverrideFlags.Set(name,false);
					s = val;
				} else {
					_palette->regionOverrideFlags.Set(name,true);
					_palette->regionOverrideParams.Set(name,val);
					s = _palette->regionOverrideParams.Get(name);
				}
				// This figures out the current region parameters after
				// a change has been made to the overridden stuff
				_palette->ResetRegionParams();
			} else if ( r >= 0 && r < (int)_palette->_regions.size() ) {
				_palette->_regions[r]->regionSpecificParams.Set(name,val);
				s = _palette->_regions[r]->regionSpecificParams.Get(name);
				_palette->ResetRegionParams();
			} else {
				throw NosuchException("increment/decrement method - bad %s parameter: %d",idtype.c_str(),r);
			}
		} else {
			int ch = idnum - 1;
			if ( idnum == MAGIC_VAL_FOR_OVERRIDE_PARAMS ) {
				if ( val == Palette::UNSET_STRING ) {
					_palette->channelOverrideFlags.Set(name,false);
					s = val;
				} else {
					_palette->channelOverrideFlags.Set(name,true);
					_palette->channelOverrideParams.Set(name,val);
					s = _palette->channelOverrideParams.Get(name);
				}
				// This figures out the current channel parameters after
				// a change has been made to the overridden stuff
				_palette->ResetChannelParams();
			} else if ( ch >= 0 && ch < (int)_palette->_channels.size() ) {
				_palette->_channels[ch]->channelSpecificParams.Set(name,val);
				s = _palette->_channels[ch]->channelSpecificParams.Get(name);
				_palette->ResetRegionParams();
			} else {
				throw NosuchException("increment/decrement method - bad %s parameter: %d",idtype.c_str(),ch);
			}
		}
		return jsonStringResult(s,id);
	}
	if (meth == "toggleoverride") {
		needParams(meth,params);
		std::string name = needString(meth,params,"name");
		// int r = needInt(meth,params,"region");
		int idnum;
		std::string idtype = needRegionOrMidiNum(meth,params, &idnum);

		NosuchDebug("TOGGLEOVERRIDE name=%s idnum=%d",name.c_str(),idnum);
		std::string s;
		if ( idnum != MAGIC_VAL_FOR_OVERRIDE_PARAMS ) {
			throw NosuchException("toggleoverride method only works on MAGIC_VAL_FOR_OVERRIDE_PARAMS");
		}
		if ( idtype == "region" ) {
			bool flag = _palette->regionOverrideFlags.GetBool(name);
			flag = !flag;
			_palette->regionOverrideFlags.Set(name,flag);
			if ( flag ) {
				s = _palette->regionOverrideParams.Get(name);
			} else {
				s = Palette::UNSET_STRING;
			}
			// This figures out the current region parameters after
			// a change has been made to the overridden stuff
			_palette->ResetRegionParams();
		} else {
			bool flag = _palette->channelOverrideFlags.GetBool(name);
			flag = !flag;
			_palette->channelOverrideFlags.Set(name,flag);
			if ( flag ) {
				s = _palette->channelOverrideParams.Get(name);
			} else {
				s = Palette::UNSET_STRING;
			}
			// This figures out the current channel parameters after
			// a change has been made to the overridden stuff
			_palette->ResetChannelParams();
		}
		return jsonStringResult(s,id);
	}

	if (meth == "increment" || meth == "decrement" ) {
		needParams(meth,params);
		std::string name = needString(meth,params,"name");
		double amount = needDouble(meth,params,"amount");
		// int r = needInt(meth,params,"region");

		int idnum;
		std::string idtype = needRegionOrMidiNum(meth,params, &idnum);

		amount = amount * ((meth == "decrement") ? -1 : 1);

		std::string s;

		if ( idnum == MAGIC_VAL_FOR_PALETTE_PARAMS ) {
				_palette->params.Increment(name,amount);
				s = _palette->params.Get(name);
		} else if ( idtype == "region" ) {
			int r = idnum;
			if ( r == MAGIC_VAL_FOR_OVERRIDE_PARAMS ) {
				_palette->regionOverrideParams.Increment(name,amount);
				_palette->regionOverrideFlags.Set(name,true);
				s = _palette->regionOverrideParams.Get(name);
				_palette->ResetRegionParams();
			} else if ( r >= 0 && r < (int)_palette->_regions.size() ) {
				_palette->_regions[r]->regionSpecificParams.Increment(name,amount);
				s = _palette->_regions[r]->regionSpecificParams.Get(name);
				_palette->ResetRegionParams();
			} else {
				throw NosuchException("increment/decrement method - bad %s parameter: %d",idtype.c_str(),r);
			}
		} else {
			int ch = idnum - 1;
			if ( idnum == MAGIC_VAL_FOR_OVERRIDE_PARAMS ) {
				_palette->channelOverrideParams.Increment(name,amount);
				_palette->channelOverrideFlags.Set(name,true);
				s = _palette->channelOverrideParams.Get(name);
				_palette->ResetChannelParams();
			} else if ( ch >= 0 && ch < (int)_palette->_channels.size() ) {
				_palette->_channels[ch]->channelSpecificParams.Increment(name,amount);
				s = _palette->_channels[ch]->channelSpecificParams.Get(name);
				_palette->ResetChannelParams();
			} else {
				throw NosuchException("increment/decrement method - bad %s parameter: %d",idtype.c_str(),ch);
			}
		}
		return jsonStringResult(s,id);
	}
	if (meth == "toggle" ) {
		needParams(meth,params);
		std::string name = needString(meth,params,"name");
		// int r = needInt(meth,params,"region");
		int idnum;
		std::string idtype = needRegionOrMidiNum(meth,params, &idnum);

		std::string s;

		if ( idnum == MAGIC_VAL_FOR_PALETTE_PARAMS ) {
			_palette->params.Toggle(name);
			s = _palette->params.Get(name);
		} else if ( idtype == "region" ) {
			int r = idnum;
			if ( r == MAGIC_VAL_FOR_OVERRIDE_PARAMS ) {
				_palette->regionOverrideFlags.Set(name,true);
				_palette->regionOverrideParams.Toggle(name);
				_palette->ResetRegionParams();
				s = _palette->regionOverrideParams.Get(name);
			} else if ( r >= 0 && r < (int)_palette->_regions.size() ) {
				_palette->_regions[r]->regionSpecificParams.Toggle(name);
				_palette->ResetRegionParams();
				s = _palette->_regions[r]->regionSpecificParams.Get(name);
			} else {
				throw NosuchException("toggle method - bad %s parameter: %d",idtype.c_str(),r);
			}
		} else {
			int ch = idnum - 1;
			if ( idnum == MAGIC_VAL_FOR_OVERRIDE_PARAMS ) {
				_palette->channelOverrideFlags.Set(name,true);
				_palette->channelOverrideParams.Toggle(name);
				_palette->ResetChannelParams();
				s = _palette->channelOverrideParams.Get(name);
			} else if ( ch >= 0 && ch < (int)_palette->_channels.size() ) {
				_palette->_channels[ch]->channelSpecificParams.Toggle(name);
				_palette->ResetChannelParams();
				s = _palette->_channels[ch]->channelSpecificParams.Get(name);
			} else {
				throw NosuchException("toggle method - bad %s parameter: %d",idtype.c_str(),ch);
			}
		}
		return jsonStringResult(s,id);
	}
	if (meth == "get") {
		needParams(meth,params);
		std::string name = needString(meth,params,"name");

		// int r = needInt(meth,params,"region");
		int idnum;
		std::string idtype = needRegionOrMidiNum(meth,params, &idnum);
		
		std::string s;
		// Really should have a different idtype for palette params...
		if ( idnum == MAGIC_VAL_FOR_PALETTE_PARAMS ) {
				s = _palette->params.Get(name);
		} else if ( idtype == "region" ) {
			int r = idnum;
			if ( r == MAGIC_VAL_FOR_OVERRIDE_PARAMS ) {
				if ( _palette->regionOverrideFlags.GetBool(name) ) {
					s = _palette->regionOverrideParams.Get(name);
				} else {
					s = Palette::UNSET_STRING;
				}
			} else if ( r >= 0 && r < (int)_palette->_regions.size() ) {
				s = _palette->_regions[r]->regionSpecificParams.Get(name);
				NosuchDebug(1,"JSON executed, get of r=%d name=%s s=%s",r,name.c_str(),s.c_str());
			} else {
				throw NosuchException("get method - bad %s parameter: %d",idtype.c_str(),r);
			}
		} else {
			int ch = idnum - 1;
			if ( idnum == MAGIC_VAL_FOR_OVERRIDE_PARAMS ) {
				if ( _palette->channelOverrideFlags.GetBool(name) ) {
					s = _palette->channelOverrideParams.Get(name);
				} else {
					s = Palette::UNSET_STRING;
				}
			} else if ( ch >= 0 && ch < (int)_palette->_channels.size() ) {
				s = _palette->_channels[ch]->channelSpecificParams.Get(name);
				NosuchDebug(1,"JSON executed, get of ch=%d name=%s s=%s",ch,name.c_str(),s.c_str());
			} else {
				throw NosuchException("get method - bad %s parameter: %d",idtype.c_str(),ch);
			}
		}
		return jsonStringResult(s,id);
	}

	errstr = NosuchSnprintf("Unrecognized method name - %s",meth.c_str());
	return error_json(-32000,errstr.c_str(),id);
}

bool
PaletteHost::checkAddrPattern(const char *addr, char *patt)
{
	return ( strncmp(addr,patt,strlen(patt)) == 0 );
}

int
ArgAsInt32(const osc::ReceivedMessage& m, unsigned int n)
{
    osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
	const char *types = m.TypeTags();
	if ( n >= strlen(types) )  {
		DebugOscMessage("ArgAsInt32 ",m);
		throw NosuchException("Attempt to get argument n=%d, but not that many arguments on addr=%s\n",n,m.AddressPattern());
	}
	if ( types[n] != 'i' ) {
		DebugOscMessage("ArgAsInt32 ",m);
		throw NosuchException("Expected argument n=%d to be an int(i), but it is (%c)\n",n,types[n]);
	}
	for ( unsigned i=0; i<n; i++ )
		arg++;
    return arg->AsInt32();
}

float
ArgAsFloat(const osc::ReceivedMessage& m, unsigned int n)
{
    osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
	const char *types = m.TypeTags();
	if ( n >= strlen(types) )  {
		DebugOscMessage("ArgAsFloat ",m);
		throw NosuchException("Attempt to get argument n=%d, but not that many arguments on addr=%s\n",n,m.AddressPattern());
	}
	if ( types[n] != 'f' ) {
		DebugOscMessage("ArgAsFloat ",m);
		throw NosuchException("Expected argument n=%d to be a double(f), but it is (%c)\n",n,types[n]);
	}
	for ( unsigned i=0; i<n; i++ )
		arg++;
    return arg->AsFloat();
}

std::string
ArgAsString(const osc::ReceivedMessage& m, unsigned n)
{
    osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
	const char *types = m.TypeTags();
	if ( n < 0 || n >= strlen(types) )  {
		DebugOscMessage("ArgAsString ",m);
		throw NosuchException("Attempt to get argument n=%d, but not that many arguments on addr=%s\n",n,m.AddressPattern());
	}
	if ( types[n] != 's' ) {
		DebugOscMessage("ArgAsString ",m);
		throw NosuchException("Expected argument n=%d to be a string(s), but it is (%c)\n",n,types[n]);
	}
	for ( unsigned i=0; i<n; i++ )
		arg++;
	return std::string(arg->AsString());
}

static void
xyz_adjust(double expand, bool switchyz, double& x, double& y, double& z) {
	if ( switchyz ) {
		double t = y;
		y = z;
		z = t;
		z = 1.0 - z;
	}
	// The values we get from the Palette don't go all the way to
	// 0.0 or 1.0, so we expand
	// the range a bit so people can draw all the way to the edges.
	x = ((x - 0.5f) * expand) + 0.5f;
	y = ((y - 0.5f) * expand) + 0.5f;
	if (x < 0.0)
		x = 0.0f;
	else if (x > 1.0)
		x = 1.0f;
	if (y < 0.0)
		y = 0.0f;
	else if (y > 1.0)
		y = 1.0f;
}

#if 0
std::string
sidString(int sidnum, const char* source)
{
	if ( strcmp(source,"") == 0 ) {
		return NosuchSnprintf("%d",sidnum);
	} else {
		return NosuchSnprintf("%d/%s",sidnum,source);  // The source has an @ in it already
	}
}
#endif

void
PaletteHost::TouchCursorSid(int sidnum, const char* sidsource, int millinow)
{
	// std::string sid = sidString(sidnum,source);
	Region* r = _palette->RegionForSid(sidnum);
	if ( r ) {
		r->touchCursor(sidnum, sidsource, millinow);
	} else {
		NosuchErrorOutput("Unable to find region (A) for sid=%d/%s",sidnum,sidsource);
	}
}

void
PaletteHost::CheckCursorUp(int millinow)
{
	_palette->checkCursorUp(millinow);
}

Cursor*
PaletteHost::SetCursorSid(int sidnum, const char* sidsource, int millinow, NosuchVector point, double depth, double tuio_f, OutlineMem* om)
{
	// std::string sid = sidString(sidnum,source);
	Region* r = _palette->RegionForSid(sidnum);
	if ( r ) {
		NosuchDebug(1,"Calling r->setCursor for region=%d",r->id);
		return r->setCursor(sidnum, sidsource, millinow, point, depth, tuio_f, om);
	} else {
		NosuchDebug("Unable to find region (B) for sid=%d/%s",sidnum,sidsource);
		NosuchErrorOutput("Unable to find region (B) for sid=%d/%s",sidnum,sidsource);
		return NULL;
	}
}

void PaletteHost::ProcessOscMessage( const char *source, const osc::ReceivedMessage& m) {
	static int Nprocessed = 0;
	try{
		// DebugOscMessage("ProcessOscMessage ",m);
	    const char *types = m.TypeTags();
		const char *addr = m.AddressPattern();
		int millinow = Pt_Time();
		Nprocessed++;
		NosuchDebug(1,"ProcessOscMessage source=%s millinow=%d addr=%s",
			source==NULL?"NULL?":source,millinow,addr);

		if (checkAddrPattern(addr,"/tuio/25Dblb")) {

			std::string cmd = ArgAsString(m,0);
			int na = (int)strlen(types);
			// NosuchDebug("/tuio/25Dblb cmd=%s\n",cmd.c_str());
			if (cmd == "alive") {
				// if ( na > 1 )
				NosuchDebug(2,"25Dblb alive types=%s na=%d\n",types,na);
				for (int i = 1; i < na; i++) {
					int sidnum = ArgAsInt32(m,i);
					TouchCursorSid(sidnum,source,millinow);
					// NosuchDebug("25Dblb gl_frame=%d alive sid= %d",gl_frame,sidnum);
				}
				CheckCursorUp(millinow);
			} else if (cmd == "fseq") {
				// int seq = ArgAsInt32(m,1);
			} else if (cmd == "set") {
				// NosuchDebug("25Dblb set na=%d\n",na);
				int sidnum = ArgAsInt32(m,1);
				double x = ArgAsFloat(m,2);
				double y = ArgAsFloat(m,3);
				double z = ArgAsFloat(m,4);
				double tuio_a = ArgAsFloat(m,5);   // Angle
				double tuio_w = ArgAsFloat(m,6);
				double tuio_h = ArgAsFloat(m,7);
				double tuio_f = ArgAsFloat(m,8);   // Area
				// y = 1.0f - y;
				Region* r = _palette->RegionForSid(sidnum);
				bool switchyz = r->palette->params.switchyz;
				xyz_adjust(1.3f, switchyz, x, y, z);

				// NosuchDebug("25Dblb gl_frame=%d set xy= %.4f %.4f",gl_frame,x,y);
				SetCursorSid(sidnum, source, millinow, NosuchVector(x,y), z, tuio_f, NULL);
			}
			return;

		} else if (checkAddrPattern(addr,"/tuio/2Dcur")) {

			std::string cmd = ArgAsString(m,0);
			int na = (int)strlen(types);
			if (cmd == "alive") {
				for (int i = 1; i < na; i++) {
					int sidnum = ArgAsInt32(m,i);
					// std::string sid = sidString(sidnum,source);
					Region* r = _palette->RegionForSid(sidnum);
					if ( !r ) {
						NosuchErrorOutput("Unable to find region (C) for sid=%d/%s",sidnum,source);
						return;
					}
					r->touchCursor(sidnum, source, millinow);
				}
				_palette->checkCursorUp(millinow);
			} else if (cmd == "fseq") {
				// int seq = ArgAsInt32(m,1);
			} else if (cmd == "set") {
				// It looks like "/tuio/2Dcur set s x y X Y m"
				int sidnum = ArgAsInt32(m,1);
				// std::string sid = sidString(sidnum,source);
				Region* r = _palette->RegionForSid(sidnum);
				if ( !r ) {
					NosuchErrorOutput("Unable to find region (D) for sid=%d/%s",sidnum,source);
					return;
				}
				double x = ArgAsFloat(m,2);
				double y = ArgAsFloat(m,3);
				double z = r->palette->params.depth2d;
				double tuio_f = r->palette->params.area2d;

				y = 1.0f - y;

				NosuchDebug(1,"/tuio/2Dcur processed=%d millinow=%d xy=%.3f,%.3f",Nprocessed,millinow,x,y);

				bool switchyz = r->palette->params.switchyz;
				xyz_adjust(1.3f, switchyz, x, y, z);  // Should I do this for non-SpacePalette TUIO?  Maybe less, like 1.1?

				r->setCursor(sidnum, source, millinow, NosuchVector(x,y), z, tuio_f, NULL);
			}
			return;
		}
		// NosuchDebug("Time=%ld GOT OSC addr=%s\n",timeGetTime(),addr);
		if ( strncmp(addr,"/tuio/",6) == 0 ) {
		}
		// First do things that have no arguments
		if ( strcmp(addr,"/clear") == 0 ) {
		} else if ( strcmp(addr,"/list") == 0 ) {
		} else if ( strcmp(addr,"/run") == 0 ) {
		} else if ( strcmp(addr,"/stop") == 0 ) {
		}

		NosuchDebug("PaletteOscInput - NO HANDLER FOR addr=%s",m.AddressPattern());
	} catch( osc::Exception& e ){
		// any parsing errors such as unexpected argument types, or 
		// missing arguments get thrown as exceptions.
		NosuchDebug("ProcessOscMessage error while parsing message: %s : %s",m.AddressPattern(),e.what());
	} catch (NosuchException& e) {
		NosuchDebug("ProcessOscMessage, NosuchException: %s",e.message());
	} catch (...) {
		// This doesn't seem to work - it doesn't seem to catch other exceptions...
		NosuchDebug("ProcessOscMessage, some other kind of exception occured during !?");
	}
}

std::string PaletteHost::RespondToJson(std::string method, cJSON *params, const char *id) {

	// We want JSON requests to be interpreted in the main thread of the FFGL plugin,
	// so we stuff the request into json_* variables and wait for the main thread to
	// pick it up (in ProcessOpenGL)
	// NosuchDebug("About to Lock json B");
	NosuchLock(&json_mutex,"json");
	// NosuchDebug("After Lock json B");

	json_pending = true;
	json_method = std::string(method);
	json_params = params;
	json_id = id;

	bool err = false;
	while ( json_pending ) {
		NosuchDebug(2,"####### Waiting for json_cond! millinow=%d",Palette::now);
		int e = pthread_cond_wait(&json_cond, &json_mutex);
		if ( e ) {
			NosuchDebug(2,"####### ERROR from pthread_cond_wait e=%d now=%d",e,Palette::now);
			err = true;
			break;
		}
	}
	std::string result;
	if ( err ) {
		result = error_json(-32000,"Error waiting for json!?");
	} else {
		result = json_result;
	}

	// NosuchDebug("About to UnLock json B");
	NosuchUnlock(&json_mutex,"json");

	return result;
}

