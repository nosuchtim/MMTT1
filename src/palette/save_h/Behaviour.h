#ifndef _BEHAVIOUR_H
#define _BEHAVIOUR_H

class Behaviour {

public:
	Behaviour(Region* r);
	Palette* palette();
	PaletteHost* paletteHost();
	Region* region();
	std::list<Cursor*>& cursors();
	int SelectionNumber(Cursor* c);
	bool isButtonDown(std::string bn);
	virtual bool isMyButton(std::string bn) { return true; }

private:
	Region* _region;
	Palette* _palette;
	PaletteHost* _paletteHost;
};

#endif