#ifndef _EVENT_H
#define _EVENT_H

class PyEvent {
public:
	virtual std::string type() = 0;
	virtual PyObject* python_object() = 0;
};

class ButtonPyEvent : public PyEvent {
public:
	ButtonPyEvent(std::string bn) {
		_bn = bn;
	}
	virtual std::string type() = 0;
	PyObject* python_object() {
		return Py_BuildValue("{s:s,s:s}",
			"type",type().c_str(),
			"region",_bn.c_str());
	}
private:
	std::string _bn;
};

class ButtonDownPyEvent : public ButtonPyEvent {
public:
	ButtonDownPyEvent(std::string bn) : ButtonPyEvent(bn) {}
	std::string type() { return "ButtonDown"; }
};

class ButtonUpPyEvent : public ButtonPyEvent {
public:
	ButtonUpPyEvent(std::string bn) : ButtonPyEvent(bn) {}
	std::string type() { return "ButtonUp"; }
};

class CursorPyEvent : public PyEvent {
public:
	CursorPyEvent(std::string region_, int sid_, NosuchVector pos_, double depth_) {
		region = region_;
		sid = sid_;
		pos = pos_;
		depth = depth_;
	}
	virtual std::string type() = 0;
	PyObject* python_object() {
		return Py_BuildValue("{s:s,s:s,s:i,s:f,s:f,s:f}",
			"type",type().c_str(),
			"region",region.c_str(),
			"sid",sid,
			"x",pos.x,
			"y",pos.y,
			"z",depth);
	}
	std::string region;
	int sid;
	NosuchVector pos;
	double depth;
};

class CursorDownPyEvent : public CursorPyEvent {
public:
	CursorDownPyEvent(std::string region, int sid, NosuchVector pos, double depth) : CursorPyEvent(region,sid,pos,depth) { }
	std::string type() { return "CursorDown"; }
};

class CursorDragPyEvent : public CursorPyEvent {
public:
	CursorDragPyEvent(std::string region, int sid, NosuchVector pos, double depth) : CursorPyEvent(region,sid,pos,depth) { }
	std::string type() { return "CursorDrag"; }
};

class CursorUpPyEvent : public CursorPyEvent {
public:
	CursorUpPyEvent(std::string region, int sid, NosuchVector pos) : CursorPyEvent(region,sid,pos,0.0) { }
	std::string type() { return "CursorUp"; }
};

#endif