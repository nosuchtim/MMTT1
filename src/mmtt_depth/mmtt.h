/*
	Space Manifold - a variety of tools for Kinect and FreeFrame

	Copyright (c) 2011-2012 Tim Thompson <me@timthompson.com>

	Permission is hereby granted, free of charge, to any person obtaining
	a copy of this software and associated documentation files
	(the "Software"), to deal in the Software without restriction,
	including without limitation the rights to use, copy, modify, merge,
	publish, distribute, sublicense, and/or sell copies of the Software,
	and to permit persons to whom the Software is furnished to do so,
	subject to the following conditions:

	The above copyright notice and this permission notice shall be
	included in all copies or substantial portions of the Software.

	Any person wishing to distribute modifications to the Software is
	requested to send the modifications to the original developer so that
	they can be incorporated into the canonical version.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
	IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
	ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
	CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
	WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef MMTT_H
#define MMTT_H

#include "mmtt_camera.h"

#include <vector>
#include <map>
#include <iostream>

#include "ip/NetworkingUtils.h"

#include "OscSender.h"
#include "OscMessage.h"
#include "BlobResult.h"
#include "blob.h"

#include "NosuchHttp.h"
#include "NosuchException.h"
#include "NosuchGraphics.h"

#ifdef _DEBUG
// We do this dance because we don't want Python.h to pull in python*_d.lib,
// since python*_d.lib is not part of the standard Python binary distribution.
#   undef _DEBUG
#   include "Python.h"
#   define _DEBUG
#else
#   include "Python.h"
#endif

// #include "Event.h"
// #include "Cursor.h"
class PyEvent;
class Cursor;

class UT_SharedMem;
class MMTT_SharedMemHeader;
class TOP_SharedMemHeader;

#include "mmtt_depth.h"

#define _USE_MATH_DEFINES // To get definition of M_PI
#include <math.h>

struct Globals
{
    HINSTANCE hInstance;    // window app instance
    HWND hwnd;      // handle for the window
    HDC   hdc;      // handle to device context
    HGLRC hglrc;    // handle to OpenGL rendering context
    int width, height;      // the desired width and
    // height of the CLIENT AREA
    // (DRAWABLE REGION in Window)
};

///////////////////////////
// GLOBALS
// declare one struct Globals called g;
// extern Globals g;

class MmttHttp;
class MmttServer;
class NosuchSocketConnection;

// hack to get function pointers, can be fixed by proper casting
extern MmttServer* ThisServer;

class MmttValue {
public:
	MmttValue() {
		MmttValue(0.0,0.0,0.0);
	}
	MmttValue(double vl,double mn,double mx,bool persist = true) {
		minvalue = mn;
		maxvalue = mx;
		internal_value = vl;
		external_value = (internal_value-mn) / (mx-mn);
		persistent = persist;
	}
	void set_persist(bool p) { persistent = p; }
	void set_internal_value(double v) {
		if ( v < minvalue )
			v = minvalue;
		else if ( v > maxvalue )
			v = maxvalue;
		internal_value = v;
		external_value = (internal_value - minvalue) / (maxvalue-minvalue);
	}
	void set_external_value(double v) {
		if ( v < 0.0 )
			v = 0.0;
		else if ( v > 1.0 )
			v = 1.0;
		external_value = v;
		internal_value = minvalue + v * (maxvalue-minvalue);
	}
	double internal_value;
	double external_value; // always 0.0 to 1.0
	double minvalue;
	double maxvalue;
	bool persistent;
};

class MmttSession {
public:
	MmttSession(CBlob* b, CvPoint center, int framenum) {
		_blob = b;
		_center = center;
		_depth_mm = 0;
		_depth_normalized = 0.0;
		_frame_born = framenum;
	}
	CBlob* _blob;
	CvPoint _center;
	int _depth_mm;
	float _depth_normalized;
	int _frame_born;
};

class MmttRegion {
public:
	MmttRegion(int i, int first_sid, CvRect r) {
		id = i;
		_first_sid = first_sid;
		_rect = r;
		name = NosuchSnprintf("Region%d",id);
		NosuchDebug(1,"==== NEW MmttRegion id=%d _first_sid=%d",id,_first_sid);
	}
	~MmttRegion() {
		NosuchDebug("MmttRegion DESTRUCTOR called! _id=%d\n",_first_sid);
	}
	int id;
	std::string name;
	int _first_sid;
	CvRect _rect;
	// std::vector<CBlob*> _blobs;
	// std::vector<int> _blobs_sid;  // session ids assigned to blobs
	std::map<int,MmttSession*> _sessions;    // map from session id to MmttSession

	int getAvailableSid();
};

class MmttServer {
 public:
	MmttServer(std::string defaultsfile);
	~MmttServer();

	static MmttServer* makeMmttServer(std::string configfile);
	static void ErrorPopup(LPCWSTR msg);
	static void ErrorPopup(const char* msg);

	bool		python_init();
	bool		python_recompileModule(const char *modulename);
	bool		python_getUtilValues();
	int			python_runfile(std::string filename);
	bool		python_reloadPyffleUtilModule();
	void		python_disable(std::string msg);
	std::string python_draw();
	bool		python_change_processor(std::string behavename);
	PyObject*	python_getProcessorObject(std::string btype);
	PyObject*	python_lock_and_call(PyObject* func, PyObject *pArgs);
	bool		python_events_disabled() { return false; }
	void		lock_python();
	void		unlock_python();

	int cameraIndex() { return _cameraIndex; };
	std::string cameraName() { return NosuchSnprintf("%s%d",_cameraType.c_str(),_cameraIndex); };

	void buttonDown(std::string bn);
	void buttonUp(std::string bn);
	void cursorDown(Cursor* c);
	void cursorDrag(Cursor* c);
	void cursorUp(Cursor* c);
	void addPyEvent(PyEvent* e);
	PyEvent* popPyEvent();

	PyObject *_recompileFunc;
	// PyObject *_processorObj;
	PyObject *_processorDrawFunc;
	PyObject *_getProcessorFunc;
	PyObject *_callBoundFunc;
    PyObject *_MmttUtilModule;

	std::list<PyEvent*> _pyevents;

	void InitOscClientLists();
	void SetOscClientList(std::string& clientlist,std::vector<OscSender*>& clientvector);
	void SendOscToClients();
	UT_SharedMem* setup_shmem_for_image();
	UT_SharedMem* setup_shmem_for_outlines();
	void shmem_lock_and_update_image(unsigned char* pix);
	void shmem_update_image(TOP_SharedMemHeader* h, unsigned char* pix);
	void shmem_lock_and_update_outlines(int nactive, int numblobs, std::vector<int> &blob_sid, std::vector<MmttRegion*> &blob_region, std::vector<CvPoint> &blob_center);
	void shmem_update_outlines(MMTT_SharedMemHeader* h,
		int nactive, int numblobs, std::vector<int> &blob_sid, std::vector<MmttRegion*> &blob_region, std::vector<CvPoint> &blob_center);
	void shutdown();
	void check_json_and_execute();
	void analyze_depth_images();
	void draw_depth_image();

	LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam ); 

	Globals g;

	void Update();
    int Run(HINSTANCE hInstance, int nCmdShow);
    LRESULT CALLBACK        DlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    void   SetStatusMessage(WCHAR* szMessage);

	void ReSizeGLScene();	// Resize And Initialize The GL Window

	void *mmtt_json_threadfunc(void *arg);

	CvScalar colorOfSession(int g);
	int regionOfColor(int r, int g, int b);

	std::string SavePatch(std::string prefix, const char* id);
	void deriveRegionsFromImage();
	void startNewRegions();
	void finishNewRegions();
	std::string LoadPatch(std::string prefix);
	std::string LoadPatchJson(std::string jstr);
	std::string startAlign();

	bool shutting_down;
	std::string status() { return _status; }

	std::string RespondToJson(const char *method, cJSON *params, const char *id);
	std::string ExecuteJson(const char *method, cJSON *params, const char *id);
	std::string AdjustValue(cJSON *params, const char *id, int direction);
	void SendOscToAllWebSocketClients(OscBundle& bundle);

	// void startHttpThread();
	void SendAllOscClients(OscBundle& bundle);
	void SendAllOscClients(OscBundle& bundle, std::vector<OscSender *> &oscClients);

	unsigned char *ffpixels() { return _ffpixels; }

	UT_SharedMem *_sharedmem_image;
	UT_SharedMem *_sharedmem_outlines;

	DepthCamera* camera;

	MmttValue val_debug;
	MmttValue val_showfps;
	MmttValue val_showrawdepth;
	MmttValue val_showregionhits;
	MmttValue val_showregionmap;
	MmttValue val_showregionrects;
	MmttValue val_showmask;
	MmttValue val_usemask;
	MmttValue val_tilt;
	MmttValue val_left;
	MmttValue val_right;
	MmttValue val_top;
	MmttValue val_bottom;
	MmttValue val_front; // mm
	MmttValue val_backtop;  // mm
	MmttValue val_backbottom; // mm
	MmttValue val_auto_window; // mm
	MmttValue val_blob_filter;
	MmttValue val_blob_param1;
	MmttValue val_blob_maxsize;
	MmttValue val_blob_minsize;
	MmttValue val_confidence;

	uint16_t *depthmm_mid;
	uint8_t *thresh_mid;
	uint8_t *depth_mid;

private:

	void init_values();
	void LoadGlobalDefaults();
	void LoadConfigDefaultsJson(std::string jstr);
	void doRegistration();
	void doDepthRegistration();
	void doAutoDepthRegistration();
	void doManualDepthRegistration();
	void doPokeRegistration();
	void registrationStart();
	void registrationContinueManual();
	void registrationPoke(CvPoint pt);
	void analyzePixels();
	void showMask();
	void showRegionHits();
	void showRegionRects();
	void showBlobSessions();
	void clearImage(IplImage* image);
	void removeMaskFrom(uint8_t* pixels);
	void copyRegionsToColorImage(IplImage* regions, unsigned char* pixels, bool overwriteBackground, bool reverseColor, bool reverseX );
	void copyColorImageToRegionsAndMask(unsigned char* pixels, IplImage* regions, IplImage* mask, bool reverseColor, bool reverseX );
	void copyRegionRectsToRegionsImage(IplImage* regions, bool reverseColor, bool reverseX);
	void doTuio1_25D( int nactive, int numblobs, std::vector<int> &blob_sid, std::vector<MmttRegion*> &blob_region, std::vector<CvPoint> &blob_center);
	void doTuio1_2D( int nactive, int numblobs, std::vector<int> &blob_sid, std::vector<MmttRegion*> &blob_region, std::vector<CvPoint> &blob_center);
	void doTuio2( int nactive, int numblobs, std::vector<int> &blob_sid, std::vector<MmttRegion*> &blob_region, std::vector<CvPoint> &blob_center);

	void _updateValue(std::string nm, MmttValue* v);
	void _toggleValue(MmttValue* v);
	void _stop_registration();

	void camera_setup(std::string camname = "");

	bool			mFirstDraw;

	bool first_client_option;
	std::string _Tuio1_2_OscClientList;
	std::string _Tuio1_25_OscClientList;
	std::string _Tuio2_OscClientList;
	std::vector<OscSender *> _Tuio1_2_Clients;
	std::vector<OscSender *> _Tuio1_25_Clients;
	std::vector<OscSender *> _Tuio2_Clients;

	std::string _global_defaults_file;
	bool _do_sharedmem;
	bool _do_tuio;
	bool _do_initialalign;
	bool _python_disabled;
	bool _do_python;
	std::string _status;

	int _jsonport;
	std::string _patchFile;
	std::string _patchDir;
	std::string _cameraType;
	int _cameraIndex;
	std::string _tempDir;
	int	_camWidth;
	int	_camHeight;
	int _camBytesPerPixel;
	int _fpscount;
	int _framenum;
	bool _regionsfilled;
	bool _regionsDefinedByPatch;
	bool _showrawdepth;
	bool _showregionrects;
	CvSize _camSize;
	IplImage* _ffImage;
	IplImage* _tmpGray;
	IplImage* _maskImage;
	IplImage* _regionsImage;
	IplImage* _tmpRegionsColor;
	IplImage* _tmpThresh;
	// CvMemStorage* _tmpStorage;

	unsigned char *_ffpixels;
	size_t _ffpixelsz;

	MmttHttp*	_httpserver;

	int _tuio_fseq;
	int _tuio_last_sent;

	bool do_setnextregion;
	std::map<std::string, MmttValue*> mmtt_values;

	CBlobResult* _newblobresult;
	CBlobResult* _oldblobresult;
	std::vector<MmttRegion*> _curr_regions;
	std::vector<MmttRegion*> _new_regions;
	std::vector<CvPoint> _savedpokes;

	double lastFpsTime;
	bool doBlob;
	bool doBW;
	bool doSmooth;
	bool autoDepth;
	// CvPoint crosshairPoint;
	int currentRegionValue;
	int registrationState;  // if 0, registration is not currently underway
	bool continuousAlign;
	bool continuousAlignStop;
	int continuousAlignOkayCount;
	int continuousAlignOkayEnough;
	
	uint8_t *depth_copy;
	uint8_t *depth_front;
	uint16_t *rawdepth_mid, *rawdepth_back, *rawdepth_front;
	uint16_t *depthmm_front;
	uint8_t *thresh_front;
	// uint16_t *depth_mm;
	// pthread_mutex_t gl_mutex;
	// pthread_mutex_t gl_backbuf_mutex;
	// pthread_cond_t gl_frame_cond;

	pthread_mutex_t json_mutex;
	pthread_cond_t json_cond;

	bool json_pending;
	const char *json_method;
	cJSON* json_params;
	const char *json_id;
	std::string json_result;

};

class MmttHttp: public NosuchHttp {
public:
	MmttHttp(MmttServer *app, int port, std::string htmldir, int timeout) : NosuchHttp(port, htmldir, timeout) {
		_server = app;
	}
	~MmttHttp() {
	}
	std::string RespondToJson(const char *method, cJSON *params, const char *id) {
		return _server->RespondToJson(method, params, id);
	}
private:
	MmttServer* _server;
};

// MmttServer* mmtt_server_create();
std::string MmttForwardSlash(std::string s);
bool isFullPath(std::string p);

#endif
