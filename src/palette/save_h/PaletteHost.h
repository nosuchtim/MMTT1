#ifndef _PALETTEHOST_H
#define _PALETTEHOST_H

#ifdef _WIN32
#include <windows.h>
#include <gl/gl.h>
#endif

#ifndef FFGL_ALREADY_DEFINED

#define FF_SUCCESS					0
#define FF_FAIL					0xFFFFFFFF

// The following typedefs are in FFGL.h, but I don't want to pull that entire file in... 

//FFGLViewportStruct (for InstantiateGL)
typedef struct FFGLViewportStructTag
{
  GLuint x,y,width,height;
} FFGLViewportStruct;

//FFGLTextureStruct (for ProcessOpenGLStruct)
typedef struct FFGLTextureStructTag
{
  DWORD Width, Height;
  DWORD HardwareWidth, HardwareHeight;
  GLuint Handle; //the actual texture handle, from glGenTextures()
} FFGLTextureStruct;

// ProcessOpenGLStruct
// ProcessOpenGLStruct
typedef struct ProcessOpenGLStructTag {
  DWORD numInputTextures;
  FFGLTextureStruct **inputTextures;
  
  //if the host calls ProcessOpenGL with a framebuffer object actively bound
  //(as is the case when the host is capturing the plugins output to an offscreen texture)
  //the host must provide the GL handle to its EXT_framebuffer_object
  //so that the plugin can restore that binding if the plugin
  //makes use of its own FBO's for intermediate rendering
  GLuint HostFBO; 
} ProcessOpenGLStruct;

#endif

#include "ManifoldPython.h"
#include "osc/OscOutboundPacketStream.h"
#include "NosuchOscInput.h"
#include "PaletteOscInput.h"
#include "NosuchOscInput.h"
#include "NosuchLooper.h"
#include "NosuchScheduler.h"
#include "NosuchColor.h"
#include "NosuchHttp.h"

class PaletteHost;
class Palette;
class PaletteHttp;
class Cursor;
class UT_SharedMem;
class MMTT_SharedMemHeader;
class Region;
class GraphicBehaviour;
class MusicBehaviour;

struct PointMem;
struct OutlineMem;

#define DEFAULT_RESOLUME_PORT 7000
#define DEFAULT_PYFFLE_PORT 9876
#define DEFAULT_OSC_INPUT_PORT 3333
#define DEFAULT_OSC_INPUT_HOST "127.0.0.1"
#define DEFAULT_HTTP_INPUT_PORT 4445

#define REGIONID_FOR_OVERRIDE_PARAMS -2
#define REGIONID_FOR_PALETTE_PARAMS -1

extern PaletteHost* RealPaletteHost;

class PaletteDaemon {
public:
	PaletteDaemon(PaletteHost* mf, int osc_input_port, std::string osc_input_host, int http_port);
	~PaletteDaemon();
	void *network_input_threadfunc(void *arg);
	void Check();
private:
	bool _network_thread_created;
	bool daemon_shutting_down;
	pthread_t _network_thread;
	PaletteHost* _paletteHost;
	PaletteOscInput* _oscinput;
	PaletteHttp* _http;
};

class PaletteHost : public NosuchOscMessageProcessor, public NosuchSchedulerClient
{
public:
	PaletteHost(std::string defaultsfile);
	virtual ~PaletteHost();

	///////////////////////////////////////////////////
	// FreeFrame plugin methods
	///////////////////////////////////////////////////
	
	DWORD PaletteHostProcessOpenGL(ProcessOpenGLStruct *pGL);

	virtual DWORD InitGL(const FFGLViewportStruct *vp) {
		NosuchDebug(1,"Hi from PaletteHost::InitGL!");
		return FF_SUCCESS;
	}
	virtual DWORD DeInitGL() {
		NosuchDebug(1,"Hi from PaletteHost::DeInitGL!");
		return FF_SUCCESS;
	}

	void test_stuff();
	bool initStuff();
	void lock_paletteHost();
	void unlock_paletteHost();
	void lock_python();
	void unlock_python();

	void openSharedMemOutlines();
	UT_SharedMem* _sharedmem_outlines;
	long _sharedmem_last_attempt;
	MMTT_SharedMemHeader* outlines_memory();

	std::string _global_defaults_file;
	bool disable_on_exception;
	bool disabled;

	void LoadGlobalDefaults();
	void LoadConfigDefaultsJson(std::string jstr);

	std::string HtmlDir();
	std::string ParamConfigDir();
	// std::string PatchFilename(std::string name);
	std::string ConfigFileName(std::string name);

	std::string RespondToJson(std::string method, cJSON *params, const char *id);
	std::string ExecuteJson(std::string meth, cJSON *params, const char *id);
	std::string ExecuteJsonAndCatchExceptions(std::string meth, cJSON *params, const char *id);

	// int SendToResolume(osc::OutboundPacketStream& p);

	int _textEraseTime;
	int NumEffectSet();
	void LoadEffectSet(int effectset);
	void LoadEffectSetRandom();

	void ProcessOscMessage(const char *source, const osc::ReceivedMessage& m);

	void TouchCursorSid(int sidnum, const char* source, int millinow);
	Cursor* SetCursorSid(int sidnum, const char* source, int millinow, NosuchVector point, double depth, double tuio_f, OutlineMem* om );
	void CheckCursorUp(int millinow);

	void ShowText(std::string text, int x, int y, int timeout);

	static bool checkAddrPattern(const char *addr, char *patt);

	std::string jsonDoubleResult(double r, const char* id);
	std::string jsonIntResult(int r, const char* id);
	std::string jsonStringResult(std::string r, const char* id);
	std::string jsonMethError(std::string e, const char* id);
	std::string jsonError(int code, std::string e, const char* id);
	std::string jsonConfigResult(std::string name, const char *id);

	// GRAPHICS ROUTINES
	double width;
	double height;

	bool m_filled;
	NosuchColor m_fill_color;
	double m_fill_alpha;
	bool m_stroked;
	NosuchColor m_stroke_color;
	double m_stroke_alpha;

	void fill(NosuchColor c, double alpha);
	void noFill();
	void stroke(NosuchColor c, double alpha);
	void noStroke();
	void strokeWeight(double w);
	void background(int);
	void rect(double x, double y, double width, double height);
	void pushMatrix();
	void popMatrix();
	void translate(double x, double y);
	void scale(double x, double y);
	void rotate(double degrees);
	void line(double x0, double y0, double x1, double y1);
	void triangle(double x0, double y0, double x1, double y1, double x2, double y2);
	void quad(double x0, double y0, double x1, double y1, double x2, double y2, double x3, double y3);
	void ellipse(double x0, double y0, double w, double h);
	void polygon(PointMem* p, int npoints);

	///////////////////////////////////////////////////
	// Factory method
	///////////////////////////////////////////////////

	PaletteDaemon* _daemon;

#if 0
	int NumberScheduled(click_t minclicks, click_t maxclicks, std::string sid) {
		return scheduler()->NumberScheduled(minclicks,maxclicks,sid);
	}
#endif

	int CurrentClick() {
		return scheduler()->CurrentClick();
	}

	NosuchScheduler* scheduler() {
		NosuchAssert(_scheduler);
		return _scheduler;
	}

	void AddLoop(NosuchLoop* loop) {
		_looper->AddLoop(loop);
	}

	void OutputNotificationMidiMsg(MidiMsg* mm, int sidnum);
	void CursorDownNotification(Cursor* c);
	void CursorLoopNotification(NosuchCursorMotion* cm, NosuchLoop* lp);
	Region* RegionOfSurfaceName(std::string s);
	Region* RegionOfButtonName(std::string s);
	Palette* palette() { return _palette; }
	bool python_events_disabled() { return _python_events_disabled; }

	GraphicBehaviour* makeGraphicBehaviour(Region* r);
	MusicBehaviour* makeMusicBehaviour(Region* r);

	int gl_frame;

protected:	

	// Parameters
	// double m_brightness;

	Palette* _palette;
	NosuchScheduler* _scheduler;
	NosuchLooper* _looper;
	
	pthread_mutex_t json_mutex;
	pthread_cond_t json_cond;

	pthread_mutex_t palette_mutex;
	pthread_mutex_t python_mutex;

	bool json_pending;
	std::string json_method;
	cJSON* json_params;
	const char *json_id;
	std::string json_result;

	bool gl_shutting_down;
	bool initialized;

	static bool StaticInitialized;
	static void StaticInitialization();

	bool python_recompileModule(char *modulename);
	bool python_init();
	int python_runfile(std::string filename);
	bool python_reloadPyffleUtilModule();
	void python_disable(std::string msg);
	std::string python_draw();
	std::string python_process_osc();
	void test_draw();
	bool python_change_processor(const char* behavename);
	PyObject* python_getProcessorObject(const char *btype);
	PyObject* python_lock_and_call(PyObject* func, PyObject *pArgs);

	PyObject *_recompileFunc;
	PyObject *_processorObj;
	PyObject *_processorDrawFunc;
	PyObject *_getProcessorFunc;
	PyObject *_callBoundFunc;
    PyObject *_nosuchUtilModule;
    PyObject *_PyffleUtilModule;

private:
	bool _python_enabled;
	bool _python_events_disabled;
	std::string _python_path; // not used yet
	bool _dotest;
	void read_config(std::ifstream& f);
	void config_and_initialize();
	std::string _midi_input;
	std::string _midi_output;
	int _osc_input_port;
	std::string _osc_input_host;
	int _http_input_port;
	bool _do_tuio;
	bool _do_sharedmem;
	std::string _configFile;
	std::string _patchFile;
	std::string _graphicbehaviour;
	std::string _musicbehaviour;

	int _resolume_output_port;  // This is the port we're sending output TO
	std::string _resolume_output_host;
	int _pyffle_output_port;  // This is the port we're sending output TO
	std::string _pyffle_output_host;

	void CheckText(int millinow);
	int EnableEffect(int effectnum, bool enabled);
	int SendToResolume(osc::OutboundPacketStream& p);

};

class PaletteHttp: public NosuchHttp {
public:
	PaletteHttp(PaletteHost* server, int port, int timeout) : NosuchHttp(port,server->HtmlDir(),timeout) {
		_server = server;
	}
	~PaletteHttp() {
	}
	std::string RespondToJson(const char *method, cJSON *params, const char *id) {
		return _server->RespondToJson(method, params, id);
	}
private:
	PaletteHost* _server;
};

#endif
