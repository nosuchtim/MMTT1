#include "NosuchUtil.h"
#include "NosuchException.h"
#include "NosuchScheduler.h"
#include "NosuchLooper.h"

#define TIME_PROC ((int32_t (*)(void *)) Pt_Time)
#define TIME_INFO NULL
#define TIME_START Pt_Start(1, 0, 0) /* timer started w/millisecond accuracy */

#define QUIT_MSG 1000

click_t GlobalClick = -1;
int GlobalPitchOffset = 0;
bool NoMultiNotes = true;
bool DoControllers = true;
bool LoopCursors = true;

void midi_callback( PtTimestamp timestamp, void *userData ) {
	NosuchScheduler* ms = (NosuchScheduler*)userData;
	NosuchAssert(ms);

	// NosuchDebug("midi_callback userData=%d ms=%d",(int)userData,(int)ms);
	try {
		CATCH_NULL_POINTERS;
		ms->Callback(timestamp);
	} catch (NosuchException& e) {
		NosuchDebug("NosuchException: %s",e.message());
	}
}

static bool
schedevent_compare (SchedEvent* first, SchedEvent* second)
{
	if (first->click<second->click)
		return true;
	else
		return false;
}

void NosuchScheduler::SortEvents(SchedEventList* sl) {
	// NosuchDebug("SortEvents before:\n%s",DebugString().c_str());
	stable_sort (sl->begin(), sl->end(),schedevent_compare);
	// NosuchDebug("SortEvents after:\n%s",DebugString().c_str());
}

void NosuchScheduler::ANO(int ch) {
	NosuchAssert(ch!=0);
	if ( ch < 0 ) {
		for ( int ch=1; ch<=16; ch++ ) {
			ANO(ch);
		}
	} else {
		NosuchDebug(1,"ANO on channel %d",ch);
		PmMessage pm = Pm_Message((ch-1) + 0xb0, 0x7b, 0x00 );
		SendPmMessage(pm,ORPHAN_SID);
	}
}

void NosuchScheduler::IncomingNoteOff(click_t clk, int ch, int pitch, int vel, int sidnum) {
	NosuchDebug(2,"NosuchScheduler::IncomingNoteOff clk=%d ch=%d pitch=%d sid=%d",clk,ch,pitch,sidnum);
	IncomingMidiMsg(MidiNoteOff::make(ch,pitch,vel),clk,sidnum);
}

void NosuchScheduler::IncomingMidiMsg(MidiMsg* m, click_t clk, int sidnum) {
	// NosuchDebug(2,"NosuchScheduler::IncomingMsg clk=%d sid=%d m=%s",clk,sidnum,m->DebugString().c_str());
	SchedEvent* e = new SchedEvent(m,clk,sidnum);
	if ( ! AddEvent(e) ) {
		delete e;
	}
}

void NosuchScheduler::IncomingSessionEnd(click_t clk, int sidnum) {
	NosuchDebug(1,"NosuchScheduler::IncomingSessionEnd clk=%d sid=%d _currentclick=%d",clk,sidnum,_currentclick);
	SchedEvent* e = new SchedEvent(clk,sidnum,1);
	if ( ! AddEvent(e) ) {
		delete e;
	}
}


bool
NosuchScheduler::AddEvent(SchedEvent* e) {
	// NosuchDebug("cc=%d AddEvent clk=%d sid=%d %s",CurrentClick(),e->click,e->sidnum,e->midimsg->DebugString().c_str());
	// NosuchDebug(1,"AddEvent clk=%d sid=%d %s",e->click,e->sidnum,e->midimsg->DebugString().c_str());
	Lock();
	SchedEventList* sl = ScheduleOf(e->sidnum);

	// Look to see if there's an identical event already there.
	SchedEventIterator it = sl->begin();
	bool found_identical = false;
	for ( ; it != sl->end(); it++ ) {
		SchedEvent* ep = *it;
		NosuchAssert(ep);
		if ( ep->click > e->click ) {
			// don't bother traversing the entire list
			break;
		}
		if ( ep->click == e->click ) {
			if (e->isSameAs(ep) ) {
				found_identical = true;
				break;
			}
#if 0
			if ( NoMultiNotes && e->isSameChanAs(ep) ) {
				NosuchDebug("Omitting multi simultaneous notes");
				found_identical = true;
				break;
			}
#endif
		}
	}
	bool added = false;
	if ( ! found_identical ) {
		sl->push_back(e);
		SortEvents(sl);
		// NosuchDebug("    End of AddEvent, sched= %s",NosuchScheduler::DebugString().c_str());
		added = true;
	} else {
		// NosuchDebug("    End of AddEvent, found identical, sched= %s",NosuchScheduler::DebugString().c_str());
	}
	Unlock();
	return added;
}

bool
NosuchScheduler::GetMidiMsg(int* pmsg)
{
	return Pm_Dequeue(_midi_to_main,pmsg) ? true : false;
}

void NosuchScheduler::Callback(PtTimestamp timestamp) {

	if ( m_running == false ) {
		return;
	}

	// We don't want to collect a whole bunch of blocked callbacks,
	// so if we can't get the lock, we just give up.
	int err = TryLock();
	if ( err != 0 ) {
		// NosuchDebug("NosuchScheduler::Callback timestamp=%d - TryLock failed",timestamp);
		return;
	}
	// NosuchDebug("NosuchScheduler::Callback timestamp=%d",timestamp);

	AdvanceTimeAndDoEvents(timestamp);

#if 0
	if ( _nowplaying_note.size() > 0 ) {
		NosuchDebug("After DoEvents, _nowplaying_note = {");
		std::map<int,MidiMsg*>::iterator it = _nowplaying_note.begin();
		for ( ; it!=_nowplaying_note.end(); it++ ) {
			int sid = it->first;
			MidiMsg* m = it->second;
			NosuchDebug("sid=%s msg=%s",sid.c_str(),m->DebugString().c_str());
		}
		NosuchDebug("   }");
	}
#endif

	PmError result;

    /* check for messages */
    do { 
		int msg;
        result = Pm_Dequeue(_main_to_midi, &msg); 
        if (result) {
            if (msg == QUIT_MSG) {
				NosuchDebug("MIDI Callback has gotten QUIT_MSG!\n");
                Pm_Enqueue(_midi_to_main, &msg);
                m_running = false;
                goto getout;
			} else {
				NosuchDebug("Unexpected msg value in _main_to_midi! msg=%d\n",msg);
            }
        }
    } while (result);         

	static int lastcallback = 0;
	// NosuchDebug("Midi Callback Time=%8d dt=%8d  POLL!\n",timestamp,(timestamp-lastcallback));
	lastcallback = timestamp;

	static int lastdump = 0;
	// NosuchDebug messages aren't flushed to the log right away, to avoid
	// screwed up timing continuously.  I.e. we only screw up the timing every 5 seconds
	if ( NosuchDebugAutoFlush==false && (timestamp - lastdump) > 5000 ) {
		lastdump = timestamp;
		NosuchDebugDumpLog();
	}

	static int lasttime = 0;

	if ( _midi_input != NULL ) {
		do {
			result = Pm_Poll(_midi_input);
			if (result) {
				PmEvent buffer;
		        int rslt = Pm_Read(_midi_input, &buffer, 1);
				if ( rslt == 1 ) {
					int msg = buffer.message;
					// NosuchDebug("GOT MIDI INPUT! Time=%8d  msgtime=%8d   dt=%8d  msg=%02x %02x %02x\n",
					// 	Pt_Time(),buffer.timestamp,(timestamp-lasttime),
					// 		Pm_MessageStatus(msg),
					// 		Pm_MessageData1(msg),
					// 		Pm_MessageData2(msg));
					lasttime = timestamp;
					Pm_Enqueue(_midi_to_main,&msg);
				} else if (rslt == pmBufferOverflow) {
					NosuchDebug("Input Buffer Overflow!?\n");
				} else {
					NosuchDebug("Unexpected Pm_Read rslt=%d\n",rslt);
				}
			}
		} while (result);
	}

getout:
	Unlock();

	return;
}

static char easytolower(char in){
  if(in<='Z' && in>='A')
    return in-('Z'-'z');
  return in;
} 

static std::string lowercase(std::string s) {
	std::string lc = s;
	std::transform(lc.begin(), lc.end(), lc.begin(), easytolower);
	return lc;
}

int findDevice(std::string nm, bool findinput, std::string& found_name)
{
	nm = lowercase(nm);
	int cnt = Pm_CountDevices();
	NosuchDebug(2,"findDevice findinput=%d looking for (%s)",findinput,nm.c_str());
	for ( int n=0; n<cnt; n++ ) {
		const PmDeviceInfo* info = Pm_GetDeviceInfo(n);
		NosuchDebug(2,"Looking at MIDI device: i=%d o=%d name=(%s)",info->input,info->output,info->name);
		if ( findinput == true && info->input == false )
			continue;
		if ( findinput == false && info->output == false )
			continue;

		std::string lowername = lowercase(std::string(info->name));
		NosuchDebug(1,"info->name=%s  lowername=%s  nm=%s",info->name,lowername.c_str(),nm.c_str());
		if ( lowername.find(nm) != lowername.npos ) {
			found_name = std::string(info->name);
			return n;
		}
	}
	return -1;
}

int findOutputDevice(std::string nm, std::string& found_name)
{
	return findDevice(nm,0,found_name);
}

int findInputDevice(std::string nm, std::string& found_name)
{
	return findDevice(nm,1,found_name);
}

bool NosuchScheduler::StartMidi(std::string midi_input, std::string midi_output) {
	if ( m_running )
		return true;

	const char* midi_in_name = midi_input.c_str();
	const char* midi_out_name = midi_output.c_str();

	NosuchDebug("Looking for MIDI output: %s",midi_out_name);

	std::string found_output_name;
	int outputId = findOutputDevice(midi_output,found_output_name);
	if ( outputId < 0 ) {
		NosuchErrorOutput("Unable to find MIDI output: %s",midi_out_name);
	} else {
		NosuchDebug("Opened MIDI output name=%s",found_output_name.c_str());
	}

	NosuchDebug("Looking for MIDI input: %s",midi_in_name);

	std::string found_input_name;
	int inputId = findInputDevice(midi_input,found_input_name);
	if ( inputId < 0 ) {
		NosuchErrorOutput("Unable to find MIDI input: %s",midi_in_name);
	} else {
		NosuchDebug("Opened MIDI input name=%s",found_input_name.c_str());
	}

	_midi_to_main = Pm_QueueCreate(32, sizeof(int32_t));
	_main_to_midi = Pm_QueueCreate(32, sizeof(int32_t));

	m_time0 = 0;
	Pt_Start(5, midi_callback, (void *)this);   // maybe should be 10?  or 1?

	/* use zero latency because we want output to be immediate */
	PmError e;
	if ( outputId >= 0 ) {
	    e = Pm_OpenOutput(&_midi_output, 
	                  outputId, 
	                  NULL /* driver info */,
	                  OUT_QUEUE_SIZE,
	                  NULL, /* timeproc */
	                  NULL /* time info */,
	                  0 /* Latency */);
		if ( e != pmNoError ) {
			NosuchDebug("Error when opening MIDI Output : %d\n",e);
			_midi_output = NULL;
		}
	}
	if ( inputId >= 0 ) {
		e = Pm_OpenInput(&_midi_input, 
	                 inputId, 
	                 NULL /* driver info */,
	                 0 /* use default input size */,
	                 NULL,
	                 NULL /* time info */);
		if ( e != pmNoError ) {
			NosuchDebug("Error when opening MIDI Input : %d\n",e);
			_midi_input = NULL;
		}
	}

	m_running = true;
	return true;
}

SchedEventList* NosuchScheduler::ScheduleOf(int sidnum) {
	std::map<int,SchedEventList*>::iterator it = _scheduled.find(sidnum);
	if ( it == _scheduled.end() ) {
		NosuchDebug(1,"CREATING NEW SchedEventList SCHEDULE for sid = %d",sidnum);
		_scheduled[sidnum] = new SchedEventList();
		return _scheduled[sidnum];
		// throw NosuchException("Unable to find schedule for sid=%d !?",sidnum);
	} else {
		return it->second;
	}
}

void NosuchScheduler::Stop() {
	if ( m_running == TRUE ) {
		Pt_Stop();
		if ( _midi_input ) {
			Pm_Close(_midi_input);
			_midi_input = NULL;
		}
		if ( _midi_output ) {
			Pm_Close(_midi_output);
			_midi_output = NULL;
		}
		Pm_Terminate();
		m_running = false;
	}
}

void NosuchScheduler::AdvanceTimeAndDoEvents(PtTimestamp timestamp) {

	LastTimeStamp = timestamp;
	NosuchAssert(m_running==true);

	// NosuchDebug("AdvanceTimeAndDoEvents B _click_client %d   this=%d",(int)_click_client,(int)this);
	int timesofar = timestamp - m_time0;
	int clickssofar = (int)(0.5 + timesofar * ClicksPerMillisecond);
	// NosuchDebug("Scheduler::AdvanceTimeAndDoEvents timestamp=%d timesofar=%d clickssofar=%d\n",timestamp,timesofar,clickssofar);

	// NosuchDebug("AdvanceTimeAndDoEvents timestamp=%d clickssofar=%d",timestamp,clickssofar);

	if ( clickssofar <= _currentclick ) {
		// NosuchDebug("clickssofar<=currentclick returning in AdvanceTimeAndDoEvents");
		return;
	}
	_currentclick = clickssofar;
	// NosuchDebug("Using _click_client %d   this=%d",(int)m_click_client,(int)this);
	if ( _click_client ) {
		// NosuchDebug("Calling _click_client->AdvanceClickTo pthread=%d",(int)pthread_self().p);
		_click_client->AdvanceClickTo(_currentclick,this);
		// NosuchDebug("After _click_client->AdvanceClickTo pthread=%d",(int)pthread_self().p);
	}
	GlobalClick = _currentclick;

	int nevents = 0;
	std::map<int,SchedEventList*>::iterator itsid = _scheduled.begin();
	for (; itsid != _scheduled.end(); itsid++ ) {
		int sidnum = itsid->first;
		SchedEventList* sl = itsid->second;
		NosuchAssert(sl);
		while ( sl->size() > 0 ) {
			SchedEvent* ev = sl->front();
			nevents++;
			int clk = ev->click;
			if ( clk > _currentclick ) {
				// NosuchDebug("Time: %.3f  Click: %d   NOT Playing: %s\n",timestamp/1000.0,_currentclick,ev->DebugString().c_str());
				break;
			}
			// This happens occasionally, at least 1 click diff
			if ( _currentclick != ev->click && _currentclick != (ev->click+1) ) {
				NosuchDebug("Hmmm, _currentclick (%d) != ev->click (%d) !?",_currentclick,ev->click);
			}
			sl->pop_front();
			// We assume ev is still valid, right?  (since the SchedEventList is
			// a list of pointers)
			DoEventAndDelete(ev,sidnum);
		}
	}
	// We send an ANO every once in a while, if there's been no events,
	// to deal with stuck notes.
	static int last_ano_or_event = 0;
	if ( nevents == 0 ) {
		// NosuchDebug("Nevents = %d! timestamp=%d",nevents,timestamp);
		if ( (timestamp - last_ano_or_event) > 5000 ) {
			// NosuchDebug("Sending ANO!");
			ANO(-1);
			last_ano_or_event = timestamp;
		}
	} else {
			last_ano_or_event = timestamp;
	}
}

int isSidCursor(int sidnum) {
	return ( sidnum >= 0 && sidnum < LOOPID_BASE );
}

// SendPmMessage IS BEING PHASED OUT - ONLY ANO STILL USES IT
void NosuchScheduler::SendPmMessage(PmMessage pm, int sidnum) {

	PmEvent ev[1];
	ev[0].timestamp = TIME_PROC(TIME_INFO);
	ev[0].message = pm;
	if ( _midi_output ) {
		Pm_Write(_midi_output,ev,1);
	} else {
		NosuchDebug("SendPmMessage: No MIDI output device?");
	}
	if ( NosuchDebugMidiAll ) {
		NosuchDebug("MIDI OUTPUT PM bytes=%02x %02x %02x",
			Pm_MessageStatus(pm),
			Pm_MessageData1(pm),
			Pm_MessageData2(pm));
	}
}

// The sid (session-id) indicates who sent it.  It can either be a
// TUIO session id, a Loop ID, or -1.
// The reason we pass in MidiMsg* is so we can use it for the Notification call.
void NosuchScheduler::SendMidiMsg(MidiMsg* msg, int sidnum) {
	MidiMsg* origmsg = msg;
	MidiMsg* mm = msg;

	MidiMsg* newmm = NULL;
	if ( GlobalPitchOffset != 0 && mm->Channel() != 10 && (mm->MidiType() == MIDI_NOTE_ON || mm->MidiType() == MIDI_NOTE_OFF) ) {
		// XXX - need to do this without new/delete
		newmm = mm->clone();
		newmm->Transpose(GlobalPitchOffset);

#if 0
		if ( mm->MidiType() == MIDI_NOTE_ON ) {
			newmm = MidiNoteOn::make(mm->Channel(),mm->Pitch()+GlobalPitchOffset,mm->Velocity());
		} else if ( mm->MidiType() == MIDI_NOTE_OFF ) {
			newmm = MidiNoteOff::make(mm->Channel(),mm->Pitch()+GlobalPitchOffset,mm->Velocity());
		}
#endif

	}
	if ( newmm != NULL ) {
		mm = newmm;
	}
	MidiMsg* nextmm;

	for ( ; mm != NULL; mm = nextmm ) {

		nextmm = mm->next;
		// nextmm = NULL;  // FAKE - temporary thing to ignore multiple midimsgs

		// XXX - need to have map of channels that don't do pitch offset
		bool isnoteon = (mm->MidiType()==MIDI_NOTE_ON);
		bool isnoteoff = (mm->MidiType()==MIDI_NOTE_OFF);

		PtTimestamp tm = TIME_PROC(TIME_INFO);
		PmMessage pm = mm->PortMidiMessage();
		PmEvent ev[1];
		ev[0].timestamp = tm;
		ev[0].message = pm;
		if ( _midi_output ) {
			Pm_Write(_midi_output,ev,1);
		} else {
			NosuchDebug("SendMidiMsg: No MIDI output device?");
		}
		if ( NosuchDebugMidiAll ) {
			NosuchDebug("MIDI OUTPUT MM bytes=%02x %02x %02x",
				Pm_MessageStatus(pm),
				Pm_MessageData1(pm),
				Pm_MessageData2(pm));
		} else if ((isnoteon||isnoteoff) && NosuchDebugMidiNotes) {
			int pitch = mm->Pitch();
			NosuchDebug("MIDI OUTPUT %s ch=%d v=%d pitch=%s/p%d",
				isnoteon?"NoteOn":"NoteOff",
				mm->Channel(),mm->Velocity(),ReadableMidiPitch(pitch),pitch);
		}
	}

	if ( newmm != NULL ) {
		delete newmm;
	}

	if ( _sched_client ) {
		_sched_client->OutputNotificationMidiMsg(origmsg,sidnum);
	}
}

void NosuchScheduler::DoEventAndDelete(SchedEvent* e, int sidnum) {

	if (e->eventtype() == SchedEvent::MIDI ) {
		MidiMsg* m = e->midimsg;
		NosuchAssert(m);
		int mt = m->MidiType();
		NosuchAssert(mt==MIDI_NOTE_ON || mt==MIDI_NOTE_OFF);
		DoNoteEvent(m,sidnum);
	} else if (e->eventtype() == SchedEvent::SESSION ) {
		// DoSessionEnd(e->sid,NULL);
	} else {
		NosuchDebug("Hey, DoEvent can't handle event type %d!?",e->eventtype());
	}
	// NosuchDebug(1,"DoEvent and delete end, before delete e (e=%d)",(int)e);
	delete e;
}

// NOTE: this routine takes ownership of the MidiMsg pointed to by m,
// so the caller shouldn't delete it or even try to access it afterward.
void
NosuchScheduler::DoNoteEvent(MidiMsg* m, int sidnum)
{
	NosuchAssert(m);
	if ( _nowplaying_note.find(sidnum) != _nowplaying_note.end() ) {
		NosuchDebug(2,"DoEvent, found _nowplaying_note for sid=%d",sidnum);
		MidiMsg* nowplaying = _nowplaying_note[sidnum];
		NosuchAssert(nowplaying);
		if ( nowplaying == m ) {
			NosuchDebug("Hey, DoEvent called with m==nowplaying?");
		}

		// If the event we're doing is a noteoff, and nowplaying is
		// the same channel/pitch, then we just play the event and
		// get rid of _nowplaying_note
		if ( m->MidiType() == MIDI_NOTE_OFF ) {
			// NosuchDebug("DO %s\n",e->DebugString().c_str());
			SendMidiMsg(m,sidnum);
			if ( m->Channel() == nowplaying->Channel() && m->Pitch() == nowplaying->Pitch() ) {
				// NosuchDebug("Removing currentlyplaying after noteoff");
				_nowplaying_note.erase(sidnum);
				delete nowplaying;
			}
			delete m;
			return;
		}

		// Controller messages here
		if ( m->MidiType() != MIDI_NOTE_ON ) {
			// NosuchDebug("DO non-note %s\n",e->DebugString().c_str());
			SendMidiMsg(m,sidnum);
			delete m;
			return;
		}

		// We want a NoteOff equivalent of nowplaying (which is a NoteOn)
		MidiNoteOn* nowon = (MidiNoteOn*)nowplaying;
		NosuchAssert(nowon);
		// NosuchDebug(1,"nowplaying nowon = %s, generating noteoff",nowon->DebugString().c_str());
		// MidiMsg* nowoff = MidiNoteOff::make(nowplaying->Channel(),nowplaying->Pitch(),0);
		while ( nowon != NULL ) {
			MidiNoteOff* nowoff = nowon->makenoteoff();
			SendMidiMsg(nowoff,sidnum);
			delete nowoff;
			if ( nowon->next ) {
				NosuchAssert(nowon->next->MidiType()==MIDI_NOTE_ON);
			}
			nowon = (MidiNoteOn*)(nowon->next);
		}

		_nowplaying_note.erase(sidnum);
		delete nowplaying;
	} else {
		NosuchDebug(1,"DoEvent, DID NOT FIND _nowplaying_note for sid=%d",sidnum);
	}

	SendMidiMsg(m,sidnum);

	if ( m->MidiType() == MIDI_NOTE_ON ) {
		_nowplaying_note[sidnum] = m;
	} else {
		delete m;
	}
}

// SendControllerMsg takes ownership of MidiMsg pointed-to by m.
void
NosuchScheduler::SendControllerMsg(MidiMsg* m, int sidnum, bool smooth)
{
	MidiMsg* nowplaying = NULL;
	int mc = m->Controller();

	std::map<int,std::map<int,MidiMsg*>>::iterator it = _nowplaying_controller.find(sidnum);
	if ( it != _nowplaying_controller.end() ) {
		NosuchDebug(1,"SendControllerMsg, found _nowplaying_controller for sid=%d",sidnum);

		std::map<int,MidiMsg*>& ctrlmap = it->second;
		// look for the controller we're going to put out

		std::map<int,MidiMsg*>::iterator it2 = ctrlmap.find(mc);
		if ( it2 != ctrlmap.end() ) {

			nowplaying = it2->second;
			NosuchAssert(nowplaying);
			NosuchAssert(nowplaying->MidiType() == MIDI_CONTROL);
			NosuchAssert(nowplaying != m );

			ctrlmap.erase(mc);

			// NosuchDebug(1,"nowplaying controller= %s",nowplaying->DebugString().c_str());

			// int currctrl = nowplaying->Controller();
			// NosuchAssert(currctrl==1);  // currently, we only handle controller 1 (modulation)

			if ( smooth ) {
				int currval = nowplaying->Value();
				int newval = (currval + m->Value())/2;
				NosuchDebug(1,"SendControllerMsg, smoothing controller value=%d  new value=%d  avg val=%d\n",currval,m->Value(),newval);
				m->Value(newval);
			} else {
				NosuchDebug(1,"SendControllerMsg, NO smoothing controller value=%d",m->Value());
			}
		}
	}
	if ( nowplaying )
		delete nowplaying;
	SendMidiMsg(m,sidnum);
	_nowplaying_controller[sidnum][mc] = m;
}

// SendPitchBendMsg takes ownership of MidiMsg pointed-to by m.
void
NosuchScheduler::SendPitchBendMsg(MidiMsg* m, int sidnum, bool smooth)
{
	MidiMsg* nowplaying = NULL;

	std::map<int,MidiMsg*>::iterator it = _nowplaying_pitchbend.find(sidnum);
	if ( it != _nowplaying_pitchbend.end() ) {
		NosuchDebug(1,"SendPitchBendMsg, found _nowplaying_pitchbend for sid=%d",sidnum);

		nowplaying = it->second;
		NosuchAssert(nowplaying);
		NosuchAssert(nowplaying->MidiType() == MIDI_PITCHBEND);
		NosuchAssert(nowplaying != m );

		_nowplaying_pitchbend.erase(sidnum);

		// NosuchDebug(1,"nowplaying pitchbend= %s",nowplaying->DebugString().c_str());

		if ( smooth ) {
			int currval = nowplaying->Value();
			int newval = (currval + m->Value())/2;
			NosuchDebug(1,"SendPitchBendMsg, smoothing pitchbend value=%d  new value=%d  avg val=%d\n",currval,m->Value(),newval);
			m->Value(newval);
		} else {
			NosuchDebug(1,"SendPitchBendMsg, NO smoothing pitchbend value=%d",m->Value());
		}
	}
	if ( nowplaying )
		delete nowplaying;
	// NosuchDebug(1,"SendPitchBendMsg, calling sendmidimsg for m=%s",m->DebugString().c_str());
	SendMidiMsg(m,sidnum);
	_nowplaying_pitchbend[sidnum] = m;
}

std::string
NosuchScheduler::DebugString() {

	std::string s;
	s = "NosuchScheduler (\n";

	std::map<int,SchedEventList*>::iterator itsid = _scheduled.begin();
	for (; itsid != _scheduled.end(); itsid++ ) {
		int sidnum = itsid->first;
		s += NosuchSnprintf("   SID=%d\n",sidnum);
		SchedEventList* sl = itsid->second;
		NosuchAssert(sl);
		SchedEventIterator it = sl->begin();
		for ( ; it != sl->end(); it++ ) {
			SchedEvent* ep = *it;
			NosuchAssert(ep);
			s += NosuchSnprintf("      Event %s\n",ep->DebugString().c_str());
		}
	}
	s += "   }";
	return s;
}

#if 0
int
NosuchScheduler::NumberScheduled(click_t minclicks, click_t maxclicks, std::string sid) {
	int count = 0;
	SchedEventList* sl = ScheduleOf(sid);
	for ( SchedEventIterator it = sl->begin(); it != sl->end(); it++) {
		SchedEvent* ep = *it;
		NosuchAssert(ep);
		if ( sid == "" || ep->sid == sid ) {
			if ( minclicks < 0 || ep->click >= minclicks ) {
				if ( maxclicks < 0 || ep->click <= maxclicks ) {
					count++;
				}
			}
		}
	}
	return count;
}
#endif

std::string
SchedEvent::DebugString() {
	std::string s;
	switch (_eventtype) {
	case SchedEvent::CURSORMOTION:
		NosuchAssert(_cursormotion != NULL);
		s = NosuchSnprintf("SchedEvent CursorMotion downdragup=%d pos=%.4f,%.4f depth=%.4f",_cursormotion->_downdragup,_cursormotion->_pos.x,_cursormotion->_pos.y,_cursormotion->_depth);
		break;
	case SchedEvent::MIDI:
		NosuchAssert(midimsg != NULL);
		s = NosuchSnprintf("SchedEvent MidiMsg %s",midimsg->DebugString().c_str());
		break;
	default:
		s = "Unknown eventtype !?";
		break;
	}
	return NosuchSnprintf("Ev click=%d %s",click,s.c_str());
}
bool
SchedEvent::isSameAs(SchedEvent* ep) {
	if ( click == ep->click && sidnum == ep->sidnum && midimsg->isSameAs(ep->midimsg) ) 
		return true;
	else
		return false;
}
bool
SchedEvent::isSameChanAs(SchedEvent* ep) {
	if ( click == ep->click && sidnum == ep->sidnum
		&& midimsg->MidiType() == MIDI_NOTE_ON
		&& midimsg->MidiType() == ep->midimsg->MidiType()
		&& midimsg->Channel() == ep->midimsg->Channel() ) 
		return true;
	else
		return false;
}

int NosuchScheduler::ClicksPerSecond = 192;
int NosuchScheduler::_currentclick;
int NosuchScheduler::m_time0;
double NosuchScheduler::ClicksPerMillisecond;
int NosuchScheduler::LastTimeStamp;

void NosuchScheduler::SetClicksPerSecond(int clkpersec) {
	NosuchDebug(1,"Setting ClicksPerSecond to %d",clkpersec);
	ClicksPerSecond = clkpersec;
	ClicksPerMillisecond = ClicksPerSecond / 1000.0;
	int timesofar = LastTimeStamp - m_time0;
	int clickssofar = (int)(0.5 + timesofar * ClicksPerMillisecond);
	_currentclick = clickssofar;
}

void NosuchScheduler::SetTempoFactor(float f) {
	QuarterNoteClicks = (int)(96 * f);
	NosuchDebug("Setting QuarterNoteClicks to %d",QuarterNoteClicks);
}

int NosuchScheduler::GetClicksPerSecond() {
	return ClicksPerSecond;
}
