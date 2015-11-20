#ifndef _NOSUCHSCHEDULER_H
#define _NOSUCHSCHEDULER_H

#include "NosuchUtil.h"
#include "NosuchMidi.h"
#include "porttime.h"
#include "portmidi.h"
#include "pmutil.h"
#include "NosuchGraphics.h"
#include <pthread.h>
#include <list>
#include <map>
#include <algorithm>

#define IN_QUEUE_SIZE 1024
#define OUT_QUEUE_SIZE 1024

// Session IDs (TUIO cursor sessions) and Loop IDs need to be distinct,
// since these IDs are attached to scheduled events so we can know
// who was responsible for creating the events.

// Loop IDs are session IDs that are >= LOOPID_BASE.
// Each region has one loop.
#define LOOPID_BASE 1000000
#define LOOPID_OF_REGION(id) (LOOPID_BASE+id)
#define ORPHAN_SID -1
#define NOSUCH_SID -2

typedef int click_t;

int isSidCursor(int sidnum);

extern click_t GlobalClick;
extern int GlobalPitchOffset;
extern bool NoMultiNotes;
extern bool DoControllers;
extern bool LoopCursors;

class NosuchScheduler;
class NoteBehaviour;

class NosuchClickClient {
public:
	virtual void AdvanceClickTo(int current_click, NosuchScheduler* sched) = 0;
};

class NosuchSchedulerClient {
public:
	virtual void OutputNotificationMidiMsg(MidiMsg* mm, int sidnum) = 0;
};

class NosuchCursorMotion {
public:
	NosuchCursorMotion(int downdragup, NosuchVector pos, float depth) {
		_downdragup = downdragup;
		_pos = pos;
		_depth = depth;
	}
	int _downdragup;
	NosuchVector _pos;
	float _depth;
};

class SchedEvent {
public:
	enum Type {
		MIDI,
		CURSORMOTION,
		SESSION
	};
	SchedEvent(click_t c, int sidnum, int ended) {
		_eventtype = SchedEvent::SESSION;
		midimsg = NULL;
		_cursormotion = NULL;
		click = c;
		sidnum = sidnum;
		_ended = ended;
		_created = GlobalClick;
	}
	SchedEvent(MidiMsg* midimsg, click_t c, int sidnum) {
		construct(SchedEvent::MIDI,midimsg,c,sidnum);
	}
	SchedEvent(NosuchCursorMotion* cm,click_t c,int sidnum_) {
		_eventtype = SchedEvent::CURSORMOTION;
		midimsg = NULL;
		_cursormotion = cm;
		click = c;
		sidnum = sidnum_;
		_created = GlobalClick;
	}
	std::string DebugString();
	bool isSameAs(SchedEvent* ep);
	bool isSameChanAs(SchedEvent* ep);

	click_t click;	// relative when in loops, absolute elsewhere
	MidiMsg* midimsg;
	NosuchCursorMotion* _cursormotion;

	int sidnum;	// TUIO session id or loop id that caused it

	void construct(int t, MidiMsg* m, click_t c, int sidnum_) {
		_eventtype = t;
		midimsg = m;
		click = c;
		sidnum = sidnum_;
		_created = GlobalClick;
	}
	int eventtype() { return _eventtype; }
	click_t created() { return _created; }
private:
	int _eventtype;
	int _ended;  // if 1, it's a SessionEnd event
	click_t _created;	// absolute
};

typedef std::list<SchedEvent*> SchedEventList;
typedef std::list<SchedEvent*>::iterator SchedEventIterator;

class NosuchScheduler {
public:
	NosuchScheduler() {
		m_running = false;
		_currentclick = 0;
		_nowplaying_note.clear();
		// _nowplaying_controller.clear();

		// ClicksPerSecond = 192;
		// ClicksPerMillisecond = ClicksPerSecond / 1000.0;

		SetClicksPerSecond(192);

		m_clicks_per_clock = 4;
		NosuchLockInit(&_sched_mutex,"sched");
		_sched_client = NULL;
		_midi_input = NULL;
		_midi_output = NULL;
		_midi_to_main = NULL;
		_main_to_midi = NULL;
		_click_client = NULL;
	}
	~NosuchScheduler() {
		NosuchDebug(1,"NosuchScheduler destructor!");
	}
	static void SetClicksPerSecond(int clkpersec);
	static void SetTempoFactor(float f);
	static int GetClicksPerSecond();
	SchedEventList* ScheduleOf(int sidnum);
	void SetClickClient(NosuchClickClient* client) {
		_click_client = client;
	}
	void SetSchedulerClient(NosuchSchedulerClient* client) {
		_sched_client = client;
	}
	bool StartMidi(std::string midi_input, std::string midi_output);
	void Stop();
	void AdvanceTimeAndDoEvents(PtTimestamp timestamp);
	void Callback(PtTimestamp timestamp);
	bool GetMidiMsg(int* pmsg);
	std::string DebugString();

	int CurrentClick() { return _currentclick; }
	// int NumberScheduled(click_t minclicks, click_t maxclicks, std::string sid);
	bool AnythingPlayingAt(click_t clk, int sidnum);
	void IncomingNoteOff(click_t clk, int ch, int pitch, int vel, int sidnum);
	void IncomingMidiMsg(MidiMsg* m, click_t clk, int sidnum);
	void IncomingSessionEnd(click_t clk, int sidnum);
	void SendPmMessage(PmMessage pm, int sidnum);
	void SendMidiMsg(MidiMsg* mm, int sidnum);
	void SendControllerMsg(MidiMsg* m, int sidnum, bool smooth);  // gives ownership of m away
	void SendPitchBendMsg(MidiMsg* m, int sidnum, bool smooth);  // gives ownership of m away
	void ANO(int ch = -1);

	static int ClicksPerSecond;
	static double ClicksPerMillisecond;
	static int _currentclick;
	static int m_time0;
	static int LastTimeStamp;
	PmStream *midi_input() { return _midi_input; }
	PmQueue *midi_to_main() { return _midi_to_main; }

	void SendNoteOffsForNowPlaying(int sidnum);

private:
	int TryLock() {
		return NosuchTryLock(&_sched_mutex,"sched");
	}
	void Lock() {
		NosuchLock(&_sched_mutex,"sched");
	}
	void Unlock() {
		NosuchUnlock(&_sched_mutex,"sched");
	}
	bool AddEvent(SchedEvent* e);  // returns false if not added, i.e. means caller should free it
	void SortEvents(SchedEventList* sl);
	void DoEventAndDelete(SchedEvent* e, int sidnum);
	void DoNoteEvent(MidiMsg* m, int sidnum);
	bool m_running;
	int m_clicks_per_clock;

	// Per-SID notestates, scheduledEvents
	std::map<int,SchedEventList*> _scheduled;
	// This is a mapping of session id (either TUIO session id or Looping id)
	// to whatever MIDI message (i.e. notes) are currently active (and need a
	// noteoff if we change).
	std::map<int,MidiMsg*> _nowplaying_note;

	// This is a mapping of session id (either TUIO session id or Looping id)
	// to whatever the last MIDI controllers were.  The
	// map inside the map is a mapping of controller numbers
	// to the messages.
	std::map<int,std::map<int,MidiMsg*>> _nowplaying_controller;

	// This is a mapping of session id (either TUIO session id or Looping id)
	// to whatever the last pitchbend was.
	std::map<int,MidiMsg*> _nowplaying_pitchbend;

	PmStream *_midi_input;
	PmStream *_midi_output;
	PmQueue *_midi_to_main;
	PmQueue *_main_to_midi;
	NosuchClickClient* _click_client;
	NosuchSchedulerClient*	_sched_client;
	pthread_mutex_t _sched_mutex;
	std::string _midi_input_name;
	std::string _midi_output_name;
};

#endif
