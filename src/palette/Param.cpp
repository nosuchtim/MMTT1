#include <pthread.h>
#include <iostream>
#include <fstream>

#include  <io.h>
#include  <stdlib.h>

#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>

#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <vector>
#include <list>
#include <string>
#include <iostream>

#include "NosuchGraphics.h"
#include "NosuchMidi.h"
#include "NosuchScheduler.h"
#include "Sprite.h"
#include "Region.h"

#if 0

SpriteParams RegionSpriteParams[MAX_REGION_ID+1];

#include "PaletteHost.h"
#include "NosuchUtil.h"
#include "Palette.h"
const float Param::UNSET_FLOAT = -999.0f;
const std::string Param::UNSET_STRING = "UNSET";

std::vector<std::string> Param::noValues;

Param::Param(std::string n, int t, bool e) {
	NosuchDebug(2,"Param::Param constructor 0, this=%d",this);
	_name = n;
	_type = t;
	_editable = e;
	_deletable = false;
}

Param::Param(std::string n, int t) {
	NosuchDebug(2,"Param::Param constructor 1, this=%d",this);
	_name = n;
	_type = t;
	_editable = true;
	_deletable = false;
}

Param::~Param() {
	NosuchDebug(2,"Param destructor p=%d",this);
}
	
bool Param::isUnset() {
	if ( _type == FLOAT ) {
		return (floatValue() == UNSET_FLOAT);
	}
	if ( _type == STRING ) {
		return (stringValue() == UNSET_STRING);
	}
	return false;
}

Param* Param::make(std::string nm, std::string val, int type) {
	NosuchDebug(2,"Param::make type=%d",type);
	if ( type == Param::STRING )
		return new ParamString(nm,val);
	if ( type == Param::FLOAT ) {
		float f = (float) atof(val.c_str());
		return new ParamFloat(nm,f);
	}
	throw NosuchException("Param::make can't handle type=%d for param named %s",type,nm.c_str());
}

std::string Param::toJSON() {
	std::string valstr = "";
	if (type() == Param::FLOAT) {
		valstr = NosuchSnprintf("%f",floatValue());
	} else if (type() == Param::STRING) {
		valstr = "\"" + stringValue() + "\"";
	} else if (type() == Param::VECTOR) {
		NosuchVector pv = vectorValue();
		valstr = NosuchSnprintf("{ \"x\":%f, \"y\":%f }",
			pv.x, pv.y);
	} else {
		throw NosuchException("Unexpected parameter type (%d) for param=%s\n",
			type(),name().c_str());
	}
	return valstr;
}

bool Param::isEditable() {
	return _editable;
}

void Param::setEditable(bool b) {
	_editable = b;
}

Param* Param::clone() {
	throw NosuchException("Hey, Param::clone called!?\n");
	return NULL;
}

std::string Param::name() {
	return _name;
}

int Param::type() {
	return _type;
}

void Param::advanceTo(int tm) {
	// no action for constants
}

int Param::intValue() {
	return (int)(floatValue());
}

bool Param::boolValue() {
	if (floatValue() == 0.0)
		return false;
	else
		return true;
}

float Param::floatValue() {
	throw NosuchException("Invalid call to floatValue for Param named %s",_name.c_str());
}

float Param::floatMinValue() {
	throw NosuchException("Invalid call to floatMinValue for Param named %s",_name.c_str());
}

float Param::floatMaxValue() {
	throw NosuchException("Invalid call to floatMaxValue for Param named %s",_name.c_str());
}

float Param::setFloatValue(float f) {
	throw NosuchException("Invalid call to setFloatValue for Param named %s",_name.c_str());
}

std::string Param::stringValue() {
	throw NosuchException("Invalid call to stringValue for Param named %s",_name.c_str());
}

std::vector<std::string> Param::stringValues() {
	throw NosuchException("Invalid call to stringValues for Param named %s",_name.c_str());
}

std::string Param::setStringValue(std::string s) {
	throw NosuchException("Invalid call to setStringValue for Param named %s",_name.c_str());
}

std::string Param::nextStringValue(int direction) {
	throw NosuchException("Invalid call to nextStringValue for Param named %s",_name.c_str());
}

NosuchVector Param::vectorValue() {
	throw NosuchException("Invalid call to vectorValue for Param named %s",_name.c_str());
}

NosuchVector Param::setVectorValue(NosuchVector v) {
	throw NosuchException("Invalid call to setVectorValue for Param named %s",_name.c_str());
}

ParamEnv::ParamEnv(std::string name, double start, double end, int dur) : Param(name,Param::FLOAT) {
	setStart(Palette::now);
	_tm = _start_time;
	_startval = (float) start;
	_endval = (float) end;
	_duration = dur;
	// NosuchDebug("ParamEnv constructor name=%s start=%f end=%f dur=%d",name.c_str(),start,end,dur);
}
	
void ParamEnv::setStart(int start) {
	_start_time = start;
}
	
ParamEnv* ParamEnv::clone() {
	NosuchDebug(2,"ParamEnv::clone");
	return new ParamEnv(name(), _startval, _endval, _duration);
}

float ParamEnv::floatValue() {
	float dt = (float)(_tm - _start_time);
	if ( dt > _duration ) {
		return _endval;
	}
	if ( dt <= 0.0 ) {
		return _startval;
	}
	float v = _startval + ( _endval- _startval ) * ( dt / _duration );
	// NosuchDebug("ParamEnv::floatvalue name=%s v=%f",name().c_str(),v);
	return v;
}

void ParamEnv::advanceTo(int tm) {
	_tm = tm;
	// NosuchDebug("ParamEnv advance name=%s tm=%d",name().c_str(),tm);
}

ParamVector::ParamVector(std::string name, NosuchVector v) : Param(name,Param::VECTOR) {
	_value = v;
}

NosuchVector ParamVector::vectorValue() {
	return _value;
}

NosuchVector ParamVector::setVectorValue(NosuchVector vec) {
	if ( ! isEditable() ) {
		throw NosuchException("Can't change constant ParamVector: %s",name().c_str());
	} else {
		_value = vec;
	}
	return _value;
}

ParamString::ParamString(std::string name, std::string v, std::vector<std::string> values) : Param(name,Param::STRING) {
	_value = v;
	_values = values;
}
	
ParamString::ParamString(std::string name, std::string v) : Param(name,Param::STRING) {
	_value = v;
	ParamString* d = (ParamString*) Palette::globalDefaults->get(name);
	if ( d == NULL ) {
		throw NosuchException("There is no globalDefaults parameter named: %s",name.c_str());
	} else {
		_values = d->_values;
	}
}

ParamString* ParamString::clone() {
	NosuchDebug(2,"ParamString::clone");
	return new ParamString(name(), _value, _values);
}

std::string ParamString::stringValue() {
	return _value;
}

std::string ParamString::setStringValue(std::string s) {
	if ( ! isEditable() ) {
		throw NosuchException("Can't change constant Param: %s",name().c_str());
	}
	if ( _values.size() == 0 ) {
		_value = s;
		return _value;
	}
	if ( s == Param::UNSET_STRING ) {
		_value = s;
		return _value;
	}
	// Make sure the value is one of the valid values
	for ( std::vector<std::string>::iterator i=_values.begin(); i != _values.end(); i++ ) {
		std::string v = *i;
		if ( v == s ) {
			_value = s;
			return _value;
		}
	}
	throw NosuchException("Invalid value (%s) for ParamString: %s",s.c_str(),name().c_str());
}

std::string ParamString::nextStringValue(int direction) {
	int i;
	if ( _value == Param::UNSET_STRING ) {
		i = 0;
	} else {
		i = -1;
		int n = 0;
		for ( std::vector<std::string>::iterator s=_values.begin(); s != _values.end(); s++,n++ ) {
			if ( _value == *s ) {
				i = n;
				break;
			}
		}
	}
	if ( i < 0 ) {
		throw NosuchException("Invalid value (%s) for ParamString named %s",_value.c_str(),name().c_str());
	}
	i += (direction>0?1:-1);
	int sz = (int)_values.size();
	if ( i < 0 ) {
		i = 0;
	} else if ( i >= sz ) {
		i = sz - 1;
	}
	int n = 0;
	for ( std::vector<std::string>::iterator s=_values.begin(); s != _values.end(); s++,n++ ) {
		if ( n == i ) {
			_value = *s;
			break;
		}
	}
	return _value;
}

ParamFloat::ParamFloat(std::string name, double v, double min, double max) : Param(name,Param::FLOAT) {
	_value = (float) v;
	_minvalue = (float) min;
	_maxvalue = (float) max;
}

ParamFloat::ParamFloat(std::string name, double v) : Param(name,Param::FLOAT) {
	_value = (float) v;
	Param* d = Palette::globalDefaults->get(name);
	if ( d == NULL ) {
		throw NosuchException("There is no globalDefaults parameter named %s",name.c_str());
	} else {
		_minvalue = d->floatMinValue();
		_maxvalue = d->floatMaxValue();
	}
}

ParamFloat* ParamFloat::clone() {
	NosuchDebug(2,"ParamFloat::clone");
	return new ParamFloat(name(), _value, _minvalue, _maxvalue);
}

float ParamFloat::floatValue() {
	return _value;
}
float ParamFloat::floatMinValue() {
	return _minvalue;
}
float ParamFloat::floatMaxValue() {
	return _maxvalue;
}
float ParamFloat::setFloatValue(float f) {
	if ( ! isEditable() ) {
		throw NosuchException("Can't change constant Param named %s",name().c_str());
	}
	if ( f < _minvalue )
		f = _minvalue;
	else if ( f > _maxvalue )
		f = _maxvalue;
	_value = f;
	return _value;
}

ParamList::ParamList(ParamList* dflts) {
	// params = new std::list<Param*>();
	defaults = dflts;
	overrides = NULL;
}

ParamList::ParamList(ParamList* dflts, ParamList* overs) {
	// params = new std::list<Param*>();
	defaults = dflts;
	overrides = overs;
}

void ParamList::addDeletable(Param* p) {
	set(p, true);
	p->setDeletable();
	NosuchDebug(2,"ADD DELETABLE param p=%d",p);
}

void ParamList::add(Param* p) {
	set(p, true);
}

void ParamList::set(Param* p) {
	set(p, false);
}

void ParamList::set(Param* p, bool complainIfPresent) {

	for ( std::list<Param*>::iterator i=params.begin(); i != params.end(); i++ ) {
		Param* pi = *i;
		NosuchAssert(pi);
		if (pi->name() == p->name()) {
			if (complainIfPresent) {
				throw NosuchException("Unexpected adding of existing param named %s",pi->name().c_str());
			}
			params.erase(i);
			break;
		}
	}
	params.push_back(p);
}

void ParamList::remove(Param* p) {
	for ( std::list<Param*>::iterator i=params.begin(); i != params.end(); i++ ) {
		Param* pi = *i;
		NosuchAssert(pi);
		if (pi->name() == p->name()) {
			NosuchDebug(2,"REMOVING Param p=%s from list!",p->name().c_str());
			params.erase(i);
			return;
		}
	}
	throw NosuchException("Unexpected removal of non-existent param named %s",p->name().c_str());
}

void ParamList::clear() {
	params.clear();
}

Param* ParamList::get(std::string name) {
	return get(name, false);
}

Param* ParamList::getOrCreate(std::string name) {
	return get(name, true);
}

Param* ParamList::getWithOverride(std::string name) {
	NosuchDebug("getWithOverride name=%s",name.c_str());
	return get(name, false, true, true);
}

Param* ParamList::getLocalWithOverride(std::string name) {
	return get(name, true, true, true);
}

Param* ParamList::get(std::string name, bool createIfMissing) {
	return get(name, createIfMissing, false, true);
}

Param* ParamList::get(std::string name, bool createIfMissing, bool allowOverride, bool exceptionIfMissing) {
	if (allowOverride == true) {
		if (overrides != NULL) {
			for ( std::list<Param*>::iterator i=overrides->params.begin(); i != overrides->params.end(); i++ ) {
				Param* pi = *i;
				NosuchAssert(pi);
				if (pi->name() == name && ! pi->isUnset() ) {
					return pi;
				}
			}
		}
	}
	for ( std::list<Param*>::iterator i=params.begin(); i != params.end(); i++ ) {
		Param* pi = *i;
		NosuchAssert(pi);
		if (pi->name() == name) {
			return pi;
		}
	}
	// it wasn't found. First find the default value, and then
	// we either create a new local Parameter (if createIfMissing is true)
	// or return the default value.
	Param* defp;
	if (defaults == NULL) {
		defp = NULL;
	} else {
		defp = defaults->get(name);
	}
	if (!createIfMissing) {
		if (defp == NULL && exceptionIfMissing ) {
			// throw NosuchException("Unable to find parameter named %s",name.c_str());G
			NosuchDebug(ERROR_OUTPUT,"Unable to find parameter named %s",name.c_str());
		}
		return defp;
	}
	if (defp == NULL) {
		throw NosuchException("Unable to get/clone default parameter named %s",name.c_str());
	}
	Param* p = (Param*) defp->clone();
	if ( p == NULL ) {
		throw NosuchException("Hey!  Unable to clone parameter named %s",name.c_str());
	}
	add(p);
	return p;
}

void ParamList::advanceTo(int tm) {
	for ( std::list<Param*>::iterator i=params.begin(); i != params.end(); i++ ) {
		Param* pi = *i;	
		NosuchAssert(pi);
		pi->advanceTo(tm);
	}
}
#endif