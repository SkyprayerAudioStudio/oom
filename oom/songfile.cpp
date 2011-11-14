//=========================================================
//  OOMidi
//  OpenOctave Midi and Audio Editor
//  $Id: songfile.cpp,v 1.25.2.12 2009/11/04 15:06:07 spamatica Exp $
//
//  (C) Copyright 1999/2000 Werner Schweer (ws@seh.de)
//=========================================================

#include <assert.h>
#include <uuid/uuid.h>

#include "app.h"
#include "song.h"
#include "Composer.h"
#include "cobject.h"
//#include "drumedit.h"
#include "Performer.h"
#include "globals.h"
#include "xml.h"
#include "drummap.h"
#include "event.h"
#include "marker/marker.h"
#include "midiport.h"
#include "audio.h"
#include "mitplugin.h"
#include "wave.h"
#include "midictrl.h"
#include "AudioMixer.h"
#include "conf.h"
#include "driver/jackmidi.h"
#include "trackview.h"

//---------------------------------------------------------
//   ClonePart
//---------------------------------------------------------

ClonePart::ClonePart(const Part* p, int i)
{
	cp = p;
	id = i;
	uuid_generate(uuid);
}

CloneList cloneList;

/*
//---------------------------------------------------------
//   updateCloneList
//---------------------------------------------------------

void updateCloneList(Part* oPart, Part* nPart)
{
  for(iClone i = cloneList.begin(); i != cloneList.end(); ++i) 
  {
	if(i->cp == oPart)
	{
	  i->cp = nPart;
	  break;
	}
  }
}

void updateCloneList(PartList* oParts, PartList* nParts)
{
  for(iPart ip = oParts->begin(); ip != oParts->end(); ++ip) 
  {
	for(iClone i = cloneList.begin(); i != cloneList.end(); ++i)
	{
	  if(i->cp == oPart)
	  {
		i->cp = nPart;
		break;
	  }
	}
  }
}

//---------------------------------------------------------
//   clearClipboardAndCloneList
//---------------------------------------------------------

void clearClipboardAndCloneList()
{
  //QApplication::clipboard()->clear(QClipboard::Clipboard);
  cloneList.clear();
}
 */

//---------------------------------------------------------
//   NKey::write
//---------------------------------------------------------

void NKey::write(int level, Xml& xml) const
{
	xml.intTag(level, "key", val);
}

//---------------------------------------------------------
//   NKey::read
//---------------------------------------------------------

void NKey::read(Xml& xml)
{
	for (;;)
	{
		Xml::Token token = xml.parse();
		switch (token)
		{
			case Xml::Error:
			case Xml::End:
				return;
			case Xml::Text:
				val = xml.s1().toInt();
				break;
			case Xml::TagEnd:
				if (xml.s1() == "key")
					return;
			default:
				break;
		}
	}
}

//---------------------------------------------------------
//   Scale::write
//---------------------------------------------------------

void Scale::write(int level, Xml& xml) const
{
	xml.intTag(level, "scale", val);
}

//---------------------------------------------------------
//   Scale::read
//---------------------------------------------------------

void Scale::read(Xml& xml)
{
	for (;;)
	{
		Xml::Token token = xml.parse();
		switch (token)
		{
			case Xml::Error:
			case Xml::End:
				return;
			case Xml::Text:
				val = xml.s1().toInt();
				break;
			case Xml::TagEnd:
				if (xml.s1() == "scale")
					return;
			default:
				break;
		}
	}
}

//---------------------------------------------------------
//   readXmlPart
//---------------------------------------------------------

Part* readXmlPart(Xml& xml, Track* track, bool doClone, bool toTrack)
{
	int id = -1;
	Part* npart = 0;
	uuid_t uuid;
	uuid_clear(uuid);
	bool uuidvalid = false;
	bool clone = true;
	bool wave = false;
	bool isclone = false;

	for (;;)
	{
		Xml::Token token = xml.parse();
		const QString& tag = xml.s1();
		switch (token)
		{
			case Xml::Error:
			case Xml::End:
				return npart;
			case Xml::TagStart:
				// If the part has not been created yet...
				if (!npart)
				{
					// If an id was found...
					if (id != -1)
					{
						for (iClone i = cloneList.begin(); i != cloneList.end(); ++i)
						{
							// Is a matching part found in the clone list?
							if (i->id == id)
							{
								// This makes a clone, chains the part, and increases ref counts.
								npart = track->newPart((Part*) i->cp, true);
								break;
							}
						}
					}
					else
						// If a uuid was found...
						if (uuidvalid)
					{
						for (iClone i = cloneList.begin(); i != cloneList.end(); ++i)
						{
							// Is a matching part found in the clone list?
							if (uuid_compare(uuid, i->uuid) == 0)
							{
								Track* cpt = i->cp->track();
								// If we want to paste to the given track...
								if (toTrack)
								{
									// If the given track type is not the same as the part's
									//  original track type, we can't continue. Just return.
									if (!track || cpt->type() != track->type())
									{
										xml.skip("part");
										return 0;
									}
								}
								else
									// ...else we want to paste to the part's original track.
								{
									// Make sure the track exists (has not been deleted).
									if ((cpt->isMidiTrack() && song->midis()->find(cpt) != song->midis()->end()) ||
											(cpt->type() == Track::WAVE && song->waves()->find(cpt) != song->waves()->end()))
										track = cpt;
									else
										// Track was not found. Try pasting to the given track, as above...
									{
										if (!track || cpt->type() != track->type())
										{
											// No luck. Just return.
											xml.skip("part");
											return 0;
										}
									}
								}

								// If it's a regular paste (not paste clone), and the original part is
								//  not a clone, defer so that a new copy is created in TagStart above.
								//if(!doClone && i->cp->cevents()->arefCount() <= 1)
								if (!doClone && !isclone)
									break;

								// This makes a clone, chains the part, and increases ref counts.
								npart = track->newPart((Part*) i->cp, true);
								break;
							}
						}
					}

					// If the part still has not been created yet...
					if (!npart)
					{
						// A clone was not created from any matching part. Create a non-clone part now.
						if (!track)
						{
							xml.skip("part");
							return 0;
						}
						// If we're pasting to selected track and the 'wave'
						//  variable is valid, check for mismatch...
						if (toTrack && uuidvalid)
						{
							// If both the part and track are not midi or wave...
							if ((wave && track->isMidiTrack()) ||
									(!wave && track->type() == Track::WAVE))
							{
								xml.skip("part");
								return 0;
							}
						}

						if (track->isMidiTrack())
							npart = new MidiPart((MidiTrack*) track);
						else if (track->type() == Track::WAVE)
							npart = new WavePart((WaveTrack*) track);
						else
						{
							xml.skip("part");
							return 0;
						}

						// Signify a new non-clone part was created.
						// Even if the original part was itself a clone, clear this because the
						//  attribute section did not create a clone from any matching part.
						clone = false;

						// If an id or uuid was found, add the part to the clone list
						//  so that subsequent parts can look it up and clone from it...
						if (id != -1)
						{
							ClonePart ncp(npart, id);
							cloneList.push_back(ncp);
						}
						else
							if (uuidvalid)
						{
							ClonePart ncp(npart);
							// New ClonePart creates its own uuid, but we need to replace it.
							uuid_copy(ncp.uuid, uuid);
							cloneList.push_back(ncp);
						}
					}
				}

				if (tag == "name")
					npart->setName(xml.parse1());
				else if (tag == "poslen")
				{
					((PosLen*) npart)->read(xml, "poslen");
				}
				else if (tag == "pos")
				{
					Pos pos;
					pos.read(xml, "pos"); // obsolete
					npart->setTick(pos.tick());
				}
				else if (tag == "len")
				{
					Pos len;
					len.read(xml, "len"); // obsolete
					npart->setLenTick(len.tick());
				}
				else if (tag == "selected")
					npart->setSelected(xml.parseInt());
				else if (tag == "color")
					npart->setColorIndex(xml.parseInt());
				else if (tag == "mute")
					npart->setMute(xml.parseInt());
				else if(tag == "zIndex")
					npart->setZIndex(xml.parseInt());
				else if(tag == "rightClip")
					npart->setRightClip((unsigned)xml.parseInt());
				else if(tag == "leftClip")
					npart->setLeftClip((unsigned)xml.parseInt());
				else if (tag == "fadeIn")
				{
					if (npart) {
						WavePart* wp = (WavePart*) npart;
						wp->fadeIn()->setWidth((long)xml.parseInt());
					}
				}
				else if (tag == "fadeOut")
				{
					if (npart) {
						WavePart* wp = (WavePart*) npart;
						wp->fadeOut()->setWidth((long)xml.parseInt());
					}
				}
				else if (tag == "event")
				{
					// If a new non-clone part was created, accept the events...
					if (!clone)
					{
						EventType type = Wave;
						if (track->isMidiTrack())
							type = Note;
						Event e(type);
						e.read(xml);
						// stored tickpos for event has absolute value. However internally
						// tickpos is relative to start of part, we substract tick().
						// TODO: better handling for wave event
						e.move(-npart->tick());
						int tick = e.tick();

						// Do not discard events belonging to clone parts,
						//  at least not yet. A later clone might have a longer,
						//  fully accommodating part length!
						// No way to tell at the moment whether there will be clones referencing this...
						// No choice but to accept all events past 0.
						if (tick < 0)
						{
							//printf("readClone: warning: event not in part: %d - %d -%d, discarded\n",
							printf("readClone: warning: event at tick:%d not in part:%s, discarded\n",
									tick, npart->name().toLatin1().constData());
						}
						else
						{
							npart->events()->add(e);
						}
					}
					else
						// ...Otherwise a clone was created, so we don't need the events.
						xml.skip(tag);
				}
				else
					xml.unknown("readXmlPart");
				break;
			case Xml::Attribut:
				if (tag == "type")
				{
					if (xml.s2() == "wave")
						wave = true;
				}
				else if (tag == "cloneId")
				{
					id = xml.s2().toInt();
				}
				else if (tag == "uuid")
				{
					uuid_parse(xml.s2().toLatin1().constData(), uuid);
					if (!uuid_is_null(uuid))
					{
						uuidvalid = true;
					}
				}
				else if (tag == "isclone")
					isclone = xml.s2().toInt();
				break;
			case Xml::TagEnd:
				if (tag == "part")
					return npart;
			default:
				break;
		}
	}
	return npart;
}

//---------------------------------------------------------
//   Part::write
//   If isCopy is true, write the xml differently so that 
//    we can have 'Paste Clone' feature. 
//---------------------------------------------------------


void Part::write(int level, Xml& xml, bool isCopy, bool forceWavePaths) const
{
	const EventList* el = cevents();
	int id = -1;
	uuid_t uuid;
	uuid_clear(uuid);
	bool dumpEvents = true;
	bool wave = _track->type() == Track::WAVE;

	if (isCopy)
	{
		for (iClone i = cloneList.begin(); i != cloneList.end(); ++i)
		{
			if (i->cp->cevents() == el)
			{
				uuid_copy(uuid, i->uuid);
				dumpEvents = false;
				break;
			}
		}
		if (uuid_is_null(uuid))
		{
			ClonePart cp(this);
			uuid_copy(uuid, cp.uuid);
			cloneList.push_back(cp);
		}
	}
	else
	{
		if (el->arefCount() > 1)
		{
			for (iClone i = cloneList.begin(); i != cloneList.end(); ++i)
			{
				if (i->cp->cevents() == el)
				{
					id = i->id;
					dumpEvents = false;
					break;
				}
			}
			if (id == -1)
			{
				id = cloneList.size();
				ClonePart cp(this, id);
				cloneList.push_back(cp);
			}
		}
	}

	// Special markers if this is a copy operation and the
	//  part is a clone.
	if (isCopy)
	{
		char sid[40]; // uuid string is 36 chars. Try 40 for good luck.
		sid[0] = 0;
		uuid_unparse_lower(uuid, sid);
		if (wave)
			xml.nput(level, "<part type=\"wave\" uuid=\"%s\"", sid);
		else
			xml.nput(level, "<part uuid=\"%s\"", sid);

		if (el->arefCount() > 1)
			xml.nput(" isclone=\"1\"");
		xml.put(">");
		level++;
	}
	else
		if (id != -1)
	{
		xml.tag(level++, "part cloneId=\"%d\"", id);
	}
	else
		xml.tag(level++, "part");

	xml.strTag(level, "name", _name);

	PosLen::write(level, xml, "poslen");
	xml.intTag(level, "selected", _selected);
	xml.intTag(level, "color", _colorIndex);
	xml.intTag(level, "zIndex", m_zIndex);
	xml.intTag(level, "rightClip", m_rightClip);
	xml.intTag(level, "leftClip", m_leftClip);
	if (wave)
	{
		WavePart* wp = (WavePart*) this;
		xml.intTag(level, "fadeIn", wp->fadeIn()->width());
		xml.intTag(level, "fadeOut", wp->fadeOut()->width());
	}
	if (_mute)
		xml.intTag(level, "mute", _mute);
	if (dumpEvents)
	{
		for (ciEvent e = el->begin(); e != el->end(); ++e)
			e->second.write(level, xml, *this, forceWavePaths);
	}
	xml.etag(level, "part");
}


//---------------------------------------------------------
//   writeFont
//---------------------------------------------------------

void Song::writeFont(int level, Xml& xml, const char* name,
		const QFont& font) const
{
	xml.nput(level, "<%s family=\"%s\" size=\"%d\"",
			name, Xml::xmlString(font.family()).toLatin1().constData(), font.pointSize());
	if (font.weight() != QFont::Normal)
		xml.nput(" weight=\"%d\"", font.weight());
	if (font.italic())
		xml.nput(" italic=\"1\"");
	xml.nput(" />\n");
}

//---------------------------------------------------------
//   readFont
//---------------------------------------------------------

QFont Song::readFont(Xml& xml, const char* name)
{
	QFont f;
	for (;;)
	{
		Xml::Token token = xml.parse();
		switch (token)
		{
			case Xml::Error:
			case Xml::End:
				return f;
			case Xml::TagStart:
				xml.unknown("readFont");
				break;
			case Xml::Attribut:
				if (xml.s1() == "family")
					f.setFamily(xml.s2());
				else if (xml.s1() == "size")
					f.setPointSize(xml.s2().toInt());
				else if (xml.s1() == "weight")
					f.setWeight(xml.s2().toInt());
				else if (xml.s1() == "italic")
					f.setItalic(xml.s2().toInt());
				break;
			case Xml::TagEnd:
				if (xml.s1() == name)
					return f;
			default:
				break;
		}
	}
	return f;
}

//---------------------------------------------------------
//   readPart
//---------------------------------------------------------

Part* OOMidi::readPart(Xml& xml)
{
	Part* part = 0;
	for (;;)
	{
		Xml::Token token = xml.parse();
		const QString& tag = xml.s1();
		switch (token)
		{
			case Xml::Error:
			case Xml::End:
				return part;
			case Xml::Text:
			{
				int trackIdx, partIdx;
				sscanf(tag.toLatin1().constData(), "%d:%d", &trackIdx, &partIdx);
				Track* track = song->artracks()->index(trackIdx);
				if (track)
					part = track->parts()->find(partIdx);
			}
				break;
			case Xml::TagStart:
				xml.unknown("readPart");
				break;
			case Xml::TagEnd:
				if (tag == "part")
					return part;
			default:
				break;
		}
	}
}

//---------------------------------------------------------
//   readToplevels
//---------------------------------------------------------

void OOMidi::readToplevels(Xml& xml)
{
	printf("OOMidi::readToplevels\n");
	PartList* pl = new PartList;
	for (;;)
	{
		Xml::Token token = xml.parse();
		const QString& tag = xml.s1();
		switch (token)
		{
			case Xml::Error:
			case Xml::End:
				return;
			case Xml::TagStart:
				if (tag == "part")
				{
					Part* part = readPart(xml);
					if (part)
					{
						printf("Found part in track lists\n");
						pl->add(part);
					}
				}
                                else if (tag == "pianoroll" || tag == "performer")
				{
					// p3.3.34
					// Do not open if there are no parts.
					// Had bogus '-1' part index for list edit in oom file,
					//  causing list edit to segfault on song load.
					// Somehow that -1 was put there on write, because the
					//  current part didn't exist anymore, so no index number
					//  could be found for it on write. Watching... may be fixed.
					// But for now be safe for all the top levels...
					if (!pl->empty())
					{

						startPerformer(pl);
						//FIXME: When started from here in a song loaded from the commandline
						//closing the PR window segfaults OOM no doing it untill I trace this down
						//toplevels.back().cobject()->readStatus(xml);
						pl = new PartList;
					}
				}
				else if (tag == "drumedit")
				{
					if (!pl->empty())
					{
						startDrumEditor(pl);
						//toplevels.back().cobject()->readStatus(xml);
						pl = new PartList;
					}
				}
				else if (tag == "listeditor")
				{
					if (!pl->empty())
					{
						startListEditor(pl);
						//toplevels.back().cobject()->readStatus(xml);
						pl = new PartList;
					}
				}
				else if (tag == "master")
				{
					startMasterEditor();
					//toplevels.back().cobject()->readStatus(xml);
				}
				else if (tag == "lmaster")
				{
					startLMasterEditor();
					//toplevels.back().cobject()->readStatus(xml);
				}
				else if (tag == "marker")
				{
					showMarker(true);
					//toplevels.back().cobject()->readStatus(xml);
				}
				else if (tag == "waveedit")
				{
					if (!pl->empty())
					{
						startWaveEditor(pl);
						//toplevels.back().cobject()->readStatus(xml);
						pl = new PartList;
					}
				}
				else if (tag == "cliplist")
				{
					startClipList(true);
					//toplevels.back().cobject()->readStatus(xml);
				}
				else
					xml.unknown("OOMidi");
				break;
			case Xml::Attribut:
				break;
			case Xml::TagEnd:
				if (tag == "toplevels")
				{
					delete pl;
					return;
				}
			default:
				break;
		}
	}
}

//---------------------------------------------------------
//   readCtrl
//---------------------------------------------------------

void OOMidi::readCtrl(Xml&, int /*prt*/, int /*channel*/)
{
#if 0
	ChannelState* iState = midiPorts[prt].iState(channel);

	int idx = 0;
	int val = -1;

	for (;;)
	{
		Xml::Token token = xml.parse();
		switch (token)
		{
			case Xml::Error:
			case Xml::End:
				return;
			case Xml::TagStart:
				xml.unknown("readCtrl");
				break;
			case Xml::Attribut:
				if (xml.s1() == "idx")
					idx = xml.s2().toInt();
				else if (xml.s1() == "val")
					val = xml.s2().toInt();
				break;
			case Xml::TagEnd:
				if (xml.s1() == "ctrl")
				{
					iState->controller[idx] = val;
					// printf("%d %d ctrl %d val %d\n", prt, channel, idx, val);
					return;
				}
			default:
				break;
		}
	}
#endif
}

//---------------------------------------------------------
//   readMidichannel
//---------------------------------------------------------

void OOMidi::readMidichannel(Xml& xml, int prt)
{
	int channel = 0;
	//      MidiPort* port = &midiPorts[prt];

	for (;;)
	{
		Xml::Token token = xml.parse();
		const QString& tag = xml.s1();
		switch (token)
		{
			case Xml::Error:
			case Xml::End:
				return;
			case Xml::TagStart:
				if (tag == "pitch")
				{
					//TODO                              port->setCtrl(channel, 0, CTRL_PITCH, xml.parseInt());
				}
				else if (tag == "program")
				{
					//TODO                              port->setCtrl(channel, 0, CTRL_PROGRAM, xml.parseInt());
				}
				else if (tag == "ctrl")
					readCtrl(xml, prt, channel);
				else
				{
					xml.unknown("readMidichannel");
				}
				break;
			case Xml::Attribut:
				if (tag == "ch")
				{
					channel = xml.s2().toInt();
				}
				break;
			case Xml::TagEnd:
				if (tag == "midichannel")
					return;
			default:
				break;
		}
	}
}

//---------------------------------------------------------
//   readMidiport
//---------------------------------------------------------

void OOMidi::readMidiport(Xml& xml)
{
	int port = 0;
	for (;;)
	{
		Xml::Token token = xml.parse();
		const QString& tag = xml.s1();
		switch (token)
		{
			case Xml::Error:
			case Xml::End:
				return;
			case Xml::TagStart:
				if (tag == "midichannel")
					readMidichannel(xml, port);
				else
				{
					xml.unknown("readMidiport");
				}
				break;
			case Xml::Attribut:
				if (tag == "port")
				{
					port = xml.s2().toInt();
				}
				break;
			case Xml::TagEnd:
				if (tag == "midiport")
				{
					return;
				}
			default:
				break;
		}
	}
}

//---------------------------------------------------------
//   readMarker
//---------------------------------------------------------

void Song::readMarker(Xml& xml)
{
	Marker m;
	m.read(xml);
	_markerList->add(m);
}

//---------------------------------------------------------
//   read
//---------------------------------------------------------

void Song::read(Xml& xml)
{
	cloneList.clear();
	associatedRoute = "";
	for (;;)
	{
		Xml::Token token;
		token = xml.parse();
		const QString& tag = xml.s1();
		switch (token)
		{
			case Xml::Error:
			case Xml::End:
				return;
			case Xml::TagStart:
				if (tag == "master")
					setMasterFlag(xml.parseInt());
				else if (tag == "info")
					songInfoStr = xml.parse1();
				else if (tag == "associatedRoute")
					associatedRoute = xml.parse1();
				else if (tag == "loop")
					setLoop(xml.parseInt());
				else if (tag == "punchin")
					setPunchin(xml.parseInt());
				else if (tag == "punchout")
					setPunchout(xml.parseInt());
				else if (tag == "record")
					setRecord(xml.parseInt());
				else if (tag == "solo")
					soloFlag = xml.parseInt();
				else if (tag == "type")
					_mtype = MType(xml.parseInt());
				else if (tag == "recmode")
					_recMode = xml.parseInt();
				else if (tag == "cycle")
					_cycleMode = xml.parseInt();
				else if (tag == "click")
					setClick(xml.parseInt());
				else if (tag == "quantize")
					_quantize = xml.parseInt();
				else if (tag == "len")
					_len = xml.parseInt();
				else if (tag == "follow")
					_follow = FollowMode(xml.parseInt());
				else if (tag == "tempolist")
				{
					tempomap.read(xml);
				}
				else if (tag == "siglist")
					///sigmap.read(xml);
					AL::sigmap.read(xml);
				else if (tag == "miditrack")
				{
					MidiTrack* track = new MidiTrack();
					track->read(xml);
					insertTrack0(track, -1);
				}
				else if (tag == "drumtrack")
				{
					MidiTrack* track = new MidiTrack();
					track->setType(Track::DRUM);
					track->read(xml);
					insertTrack0(track, -1);
				}
				else if (tag == "wavetrack")
				{
					WaveTrack* track = new WaveTrack();
					track->read(xml);
					insertTrack0(track, -1);
					// Now that the track has been added to the lists in insertTrack2(),
					//  OSC can find the track and its plugins, and start their native guis if required...
					track->showPendingPluginNativeGuis();
				}
				else if (tag == "AudioInput")
				{
					AudioInput* track = new AudioInput();
					track->read(xml);
					insertTrack0(track, -1);
					track->showPendingPluginNativeGuis();
				}
				else if (tag == "AudioOutput")
				{
					AudioOutput* track = new AudioOutput();
					track->read(xml);
					insertTrack0(track, -1);
					track->showPendingPluginNativeGuis();
				}
				else if (tag == "AudioBuss" || tag == "AudioGroup")
				{
					AudioBuss* track = new AudioBuss();
					track->read(xml);
					insertTrack0(track, -1);
					track->showPendingPluginNativeGuis();
				}
				else if (tag == "AudioAux")
				{
					AudioAux* track = new AudioAux();
					track->read(xml);
					insertTrack0(track, -1);
					track->showPendingPluginNativeGuis();
				}
				else if (tag == "SynthI")
				{
					SynthI* track = new SynthI();
					track->read(xml);
					// Done in SynthI::read()
					// insertTrack(track,-1);
					//track->showPendingPluginNativeGuis();
				}
				else if (tag == "Route")
				{
					readRoute(xml);
				}
				else if (tag == "marker")
					readMarker(xml);
				else if (tag == "globalPitchShift")
					_globalPitchShift = xml.parseInt();
				else if (tag == "automation")
					automation = xml.parseInt();
				else if (tag == "cpos")
				{
					int pos = xml.parseInt();
					Pos p(pos, true);
					setPos(Song::CPOS, p, false, false, false);
				}
				else if (tag == "lpos")
				{
					int pos = xml.parseInt();
					Pos p(pos, true);
					setPos(Song::LPOS, p, false, false, false);
				}
				else if (tag == "rpos")
				{
					int pos = xml.parseInt();
					Pos p(pos, true);
					setPos(Song::RPOS, p, false, false, false);
				}
				else if (tag == "drummap")
					xml.skip(tag);
					//readDrumMap(xml, false);
				else if (tag == "trackview")
				{//Read in our trackviews
					//printf("Song::read() found track view\n");
					TrackView* tv = new TrackView();
					tv->read(xml);
					if(tv->selected())
					{
						TrackList* tvl = tv->tracks();
						for(ciTrack it = tvl->begin(); it != tvl->end(); ++it)
						{
							_viewtracks.push_back((*it));
						}
					}
					insertTrackView(tv, -1);
				}
				else
					xml.unknown("Song");
				break;
			case Xml::Attribut:
				break;
			case Xml::TagEnd:
				if (tag == "song")
				{
					//Call song->updateTrackViews1() to update the canvas and headers
					updateTrackViews1();
					//Call to update the track view menu
					update(SC_VIEW_CHANGED);
					return;
				}
			default:
				break;
		}
	}
	dirty = false;

	// Since cloneList is also used for copy/paste operations,
	//  clear the copy clone list again.
	cloneList.clear();
}

//---------------------------------------------------------
//   read
//    read song
//---------------------------------------------------------

void OOMidi::read(Xml& xml, bool skipConfig)
{
	bool skipmode = true;
	for (;;)
	{
		Xml::Token token = xml.parse();
		const QString& tag = xml.s1();
		switch (token)
		{
			case Xml::Error:
			case Xml::End:
				return;
			case Xml::TagStart:
				if (skipmode && (tag == "oom" || tag == "muse"))
					skipmode = false;
				else if (skipmode)
					break;
				else if (tag == "configuration")
					if (skipConfig)
						//xml.skip(tag);
						readConfiguration(xml, true /* only read sequencer settings */);
					else
						readConfiguration(xml, false);
				else if (tag == "song")
				{
					song->read(xml);
					audio->msgUpdateSoloStates();
				}
				else if (tag == "midiport")
					readMidiport(xml);
				else if (tag == "Controller")
				{ // obsolete
					MidiController* ctrl = new MidiController;
					ctrl->read(xml);
					delete ctrl;
				}
				else if (tag == "mplugin")
					readStatusMidiInputTransformPlugin(xml);
				else if (tag == "toplevels")
				{
					//We no longer want to start top level windows with oom
					//Start with only the Composer and that's all.
					xml.skip(tag);
					//readToplevels(xml);
				}
				else
					xml.unknown("oom");
				break;
			case Xml::Attribut:
				if (tag == "version")
				{
					int major = xml.s2().section('.', 0, 0).toInt();
					int minor = xml.s2().section('.', 1, 1).toInt();
					xml.setVersion(major, minor);
				}
				break;
			case Xml::TagEnd:
				if (!skipmode && (tag == "oom" || tag == "muse"))
					return;
			default:
				break;
		}
	}
}

//---------------------------------------------------------
//   write
//---------------------------------------------------------

void Song::write(int level, Xml& xml) const
{
	xml.tag(level++, "song");
	xml.strTag(level, "info", songInfoStr);
	xml.strTag(level, "associatedRoute", associatedRoute);
	xml.intTag(level, "automation", automation);
	xml.intTag(level, "cpos", song->cpos());
	xml.intTag(level, "rpos", song->rpos());
	xml.intTag(level, "lpos", song->lpos());
	xml.intTag(level, "master", _masterFlag);
	xml.intTag(level, "loop", loopFlag);
	xml.intTag(level, "punchin", punchinFlag);
	xml.intTag(level, "punchout", punchoutFlag);
	xml.intTag(level, "record", recordFlag);
	xml.intTag(level, "solo", soloFlag);
	xml.intTag(level, "type", _mtype);
	xml.intTag(level, "recmode", _recMode);
	xml.intTag(level, "cycle", _cycleMode);
	xml.intTag(level, "click", _click);
	xml.intTag(level, "quantize", _quantize);
	xml.intTag(level, "len", _len);
	xml.intTag(level, "follow", _follow);
	if (_globalPitchShift)
		xml.intTag(level, "globalPitchShift", _globalPitchShift);

	// Make a backup of the current clone list, to retain any 'copy' items,
	//  so that pasting works properly after.
	CloneList copyCloneList = cloneList;
	cloneList.clear();

	// write Composer visible tracks first so we maintain sort order
	//for (ciTrack i = _artracks.begin(); i != _artracks.end(); ++i)
	//	(*i)->write(level, xml);

	QList<QString> added;
	for (ciTrack i = _artracks.begin(); i != _artracks.end(); ++i)
	{
		(*i)->write(level, xml);
		added.append((*i)->name());
	}

	for (ciTrack i = _tracks.begin(); i != _tracks.end(); ++i)
	{
		if(!added.contains((*i)->name()))
			(*i)->write(level, xml);
	}


	// write the buss type tracks
	/*for (ciTrack i = _inputs.begin(); i != _inputs.end(); ++i)
		(*i)->write(level, xml);

	for (ciTrack i = _outputs.begin(); i != _outputs.end(); ++i)
		(*i)->write(level, xml);

	for (ciTrack i = _groups.begin(); i != _groups.end(); ++i)
		(*i)->write(level, xml);

	for (ciTrack i = _auxs.begin(); i != _auxs.end(); ++i)
		(*i)->write(level, xml);*/

	// write track views
	for (ciTrackView i = _tviews.begin(); i != _tviews.end(); ++i)
	{
		(*i)->write(level, xml);
	}

	// write routing
	for (ciTrack i = _tracks.begin(); i != _tracks.end(); ++i)
	{

		// p3.3.38 Changed
		//if ((*i)->isMidiTrack())
		//      continue;
		//WaveTrack* track = (WaveTrack*)(*i);
		//track->writeRouting(level, xml);

		(*i)->writeRouting(level, xml);
	}

	// Write midi device routing.
	for (iMidiDevice i = midiDevices.begin(); i != midiDevices.end(); ++i)
	{
		//MidiJackDevice* mjd = dynamic_cast<MidiJackDevice*>(*i);
		//if (!mjd)
		//  continue;
		//mjd->writeRouting(level, xml);
		(*i)->writeRouting(level, xml);
	}

	// p3.3.49 Write midi port routing.
	for (int i = 0; i < MIDI_PORTS; ++i)
	{
		midiPorts[i].writeRouting(level, xml);
	}

	tempomap.write(level, xml);
	///sigmap.write(level, xml);
	AL::sigmap.write(level, xml);
	_markerList->write(level, xml);

	//writeDrumMap(level, xml, false);
	xml.tag(level, "/song");

	// Restore backup of the clone list, to retain any 'copy' items,
	//  so that pasting works properly after.
	cloneList.clear();
	cloneList = copyCloneList;
}

//---------------------------------------------------------
//   TrackView::write
//---------------------------------------------------------

void TrackView::write(int level, Xml& xml) const /*{{{*/
{
	std::string tag = "trackview";

	xml.put(level, "<%s>", tag.c_str());//, _name.toStdString().c_str(), _selected, _type);
	level++;
	xml.strTag(level, "name", _name);
	xml.intTag(level, "selected", _selected);
	xml.intTag(level, "record", _recState);
	if(!_comment.isEmpty())
		xml.strTag(level, "comment", Xml::xmlString(_comment).toUtf8().constData());

	//for(iTrack* t = _tracks.begin(); t != _tracks.end(); ++t)
	for (ciTrack t = _tracks.begin(); t != _tracks.end(); ++t)
	{
		xml.strTag(level, "vtrack", (*t)->name());
	}
	QMap<QString, TrackSettings*>::ConstIterator ts;
	for(ts = _tSettings.begin(); ts != _tSettings.end(); ++ts)
	{
		//if((*ts).valid)
			(*ts)->write(level, xml);
	}
	xml.put(level--, "</%s>", tag.c_str());
}/*}}}*/

void TrackSettings::write(int level, Xml& xml) const
{
	std::string tag = "tracksettings";
	xml.put(level, "<%s>", tag.c_str());
	xml.strTag(level, "trackname", track->name().toUtf8().constData());
	xml.strTag(level, "pname", pname.toUtf8().constData());
	xml.intTag(level, "program", program);
	xml.intTag(level, "rec", rec);
	xml.intTag(level, "transpose", transpose);
	xml.put(level--, "</%s>", tag.c_str());
}

void TrackSettings::read(Xml& xml)
{
	program = -1;
	rec = 0;
	pname = QString("");
	track = 0;
	transpose = 0;
	for (;;)
	{
		Xml::Token token = xml.parse();
		const QString& tag = xml.s1();
		switch (token)
		{
			case Xml::Error:
			case Xml::End:
				return;
			case Xml::TagStart:
				if(tag == "pname")
				{
					pname = xml.parse1();
				}
				else if(tag == "trackname")
				{
					Track *t = song->findTrack(xml.parse1());
					if(t)
						track = t;
				}
				else if(tag == "rec")
				{
					rec = (bool)xml.parseInt();
				}
				else if(tag == "program")
				{
					program = (bool)xml.parseInt();
				}
				else if(tag == "transpose")
				{
					transpose = xml.parseInt();
				}
				break;
			case Xml::Attribut:
				break;
			case Xml::TagEnd:
				if(tag == "tracksettings")
					return;
			default:
				break;
		}
	}
}

//---------------------------------------------------------
//   write
//    write song
//---------------------------------------------------------

void OOMidi::write(Xml& xml) const
{
	xml.header();

	int level = 0;
	xml.tag(level++, "oom version=\"2.0\"");
	writeConfiguration(level, xml);

	writeStatusMidiInputTransformPlugins(level, xml);

	song->write(level, xml);

	/*if (!toplevels.empty())
	{
		xml.tag(level++, "toplevels");
		for (ciToplevel i = toplevels.begin(); i != toplevels.end(); ++i)
		{
			if (i->cobject()->isVisible())
				i->cobject()->writeStatus(level, xml);
		}
		xml.tag(level--, "/toplevels");
	}*/

	xml.tag(level, "/oom");
}

