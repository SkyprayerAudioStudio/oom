//===========================================================
//  OOMidi
//  OpenOctave Midi and Audio Editor
//  (C) Copyright 2011 Andrew Williams & Christopher Cherrett
//===========================================================

#include "CreateTrackDialog.h"
#include "globals.h"
#include "gconfig.h"
#include "track.h"
#include "song.h"
#include "app.h"
#include "mididev.h"
#include "midiport.h"
#include "minstrument.h"
#include "synth.h"
#include "audio.h"
#include "midiseq.h"
#include "driver/jackaudio.h"
#include "driver/jackmidi.h"
#include "driver/alsamidi.h"
#include "network/lsclient.h"
#include "icons.h"
#include "midimonitor.h"
#include "plugin.h"


CreateTrackDialog::CreateTrackDialog(int type, int pos, QWidget* parent, bool templateMode)
: QDialog(parent),
m_insertType(type),
m_insertPosition(pos),
m_templateMode(templateMode)
{
	setupUi(this);
	m_createMidiInputDevice = false;
	m_createMidiOutputDevice = false;
	
	m_midiSameIO = false;
	m_createTrackOnly = false;
	m_instrumentLoaded = false;
	m_showJackAliases = -1;
	
	m_midiInPort = -1;
	m_midiOutPort = -1;

    m_height = 290;
	m_width = 450;

	m_allChannelBit = (1 << MIDI_CHANNELS) - 1;

	m_lastTrack = 0;

	cmbType->addItem(*addMidiIcon, tr("Midi"), Track::MIDI);
	cmbType->addItem(*addAudioIcon, tr("Audio"), Track::WAVE);
	cmbType->addItem(*addOutputIcon, tr("Output"), Track::AUDIO_OUTPUT);
	cmbType->addItem(*addInputIcon, tr("Input"), Track::AUDIO_INPUT);
	cmbType->addItem(*addBussIcon, tr("Buss"), Track::AUDIO_BUSS);
	cmbType->addItem(*addAuxIcon, tr("Aux Send"), Track::AUDIO_AUX);

	cmbInChannel->addItem(tr("All"), m_allChannelBit);
	for(int i = 0; i < 16; ++i)
	{
		cmbInChannel->addItem(QString(tr("Chan ")).append(QString::number(i+1)), 1 << i);
	}
	cmbInChannel->setCurrentIndex(1);
	
	//cmbOutChannel->addItem(tr("All"), m_allChannelBit);
	for(int i = 0; i < 16; ++i)
	{
		cmbOutChannel->addItem(QString(tr("Chan ")).append(QString::number(i+1)), i);
	}
	cmbOutChannel->setCurrentIndex(0);
	
	int row = cmbType->findData(m_insertType);
	cmbType->setCurrentIndex(row);

	connect(chkInput, SIGNAL(toggled(bool)), this, SLOT(updateInputSelected(bool)));
	connect(chkOutput, SIGNAL(toggled(bool)), this, SLOT(updateOutputSelected(bool)));
	connect(chkBuss, SIGNAL(toggled(bool)), this, SLOT(updateBussSelected(bool)));
	connect(cmbType, SIGNAL(currentIndexChanged(int)), this, SLOT(trackTypeChanged(int)));
	//connect(cmbInstrument, SIGNAL(currentIndexChanged(int)), this, SLOT(updateInstrument(int)));
	connect(cmbInstrument, SIGNAL(activated(int)), this, SLOT(updateInstrument(int)));
	connect(btnAdd, SIGNAL(clicked()), this, SLOT(addTrack()));
	connect(txtName, SIGNAL(textEdited(QString)), this, SLOT(trackNameEdited()));
	connect(btnCancel, SIGNAL(clicked()), this, SLOT(cancelSelected()));
	txtName->setFocus(Qt::OtherFocusReason);
}

//Add button slot
void CreateTrackDialog::addTrack()/*{{{*/
{
	if(txtName->text().isEmpty())
		return;
	
	int inputIndex = cmbInput->currentIndex();
	int outputIndex = cmbOutput->currentIndex();
	int instrumentIndex = cmbInstrument->currentIndex();
	int monitorIndex = cmbMonitor->currentIndex();
	int inChanIndex = cmbInChannel->currentIndex();
	int outChanIndex = cmbOutChannel->currentIndex();
	int bussIndex = cmbBuss->currentIndex();
	bool valid = true;

	OOVirtualTrack vtrack;
	vtrack.name = txtName->text();
	vtrack.type = m_insertType;
	Track::TrackType type = (Track::TrackType)m_insertType;
	switch(type)
	{
		case Track::MIDI:
		case Track::DRUM:
		{/*{{{*/
			vtrack.autoCreateInstrument = chkAutoCreate->isChecked();
			//Process Input connections
			if(inputIndex >= 0 && chkInput->isChecked())
			{
				vtrack.useInput = true;
				QString devname = cmbInput->itemText(inputIndex);
				int inDevType = cmbInput->itemData(inputIndex).toInt();
				int chanbit = cmbInChannel->itemData(inChanIndex).toInt();
				if(m_currentMidiInputList.isEmpty())
				{
					m_createMidiInputDevice = true;
				}
				else
				{
					m_createMidiInputDevice = !(m_currentMidiInputList.contains(inputIndex) && m_currentMidiInputList.value(inputIndex) == devname);
				}
				vtrack.createMidiInputDevice = m_createMidiInputDevice;
				vtrack.inputConfig = qMakePair(inDevType, devname);
				vtrack.inputChannel = chanbit;
			}
			
			//Process Output connections
			if(outputIndex >= 0 && chkOutput->isChecked())
			{
				vtrack.useOutput = true;
				QString devname = cmbOutput->itemText(outputIndex);
				int devtype = cmbOutput->itemData(outputIndex).toInt();
				int outChan = cmbOutChannel->itemData(outChanIndex).toInt();
				if(m_currentMidiOutputList.isEmpty())
				{
					m_createMidiOutputDevice = true;
				}
				else
				{
					m_createMidiOutputDevice = !(m_currentMidiOutputList.contains(outputIndex) && m_currentMidiOutputList.value(outputIndex) == devname);
				}
				vtrack.createMidiOutputDevice = m_createMidiOutputDevice;
				vtrack.outputConfig = qMakePair(devtype, devname);
				vtrack.outputChannel = outChan;
				if(m_createMidiOutputDevice)
				{
					QString instrumentName = cmbInstrument->itemText(instrumentIndex);
					vtrack.instrumentName = instrumentName;
				}
			}

			if(monitorIndex >= 0 && midiBox->isChecked())
			{
				int iBuss = cmbBuss->itemData(bussIndex).toInt();
				QString selectedInput = cmbMonitor->itemText(monitorIndex);
				QString selectedBuss = cmbBuss->itemText(bussIndex);
				vtrack.useMonitor = true;
				vtrack.monitorConfig = qMakePair(0, selectedInput);
				if(chkBuss->isChecked())
				{
					vtrack.useBuss = true;
					vtrack.bussConfig = qMakePair(iBuss, selectedBuss);
				}
			}
		}/*}}}*/
		break;
		case Track::WAVE:
		{
			if(inputIndex >= 0 && chkInput->isChecked())
			{
				QString selectedInput = cmbInput->itemText(inputIndex);
				int addNewRoute = cmbInput->itemData(inputIndex).toInt();
				vtrack.useInput = true;
				vtrack.inputConfig = qMakePair(addNewRoute, selectedInput);
			}
			if(outputIndex >= 0 && chkOutput->isChecked())
			{
				//Route to the Output or Buss
				QString selectedOutput = cmbOutput->itemText(outputIndex);
				vtrack.useOutput = true;
				vtrack.outputConfig = qMakePair(0, selectedOutput);
			}
		}
		break;
		case Track::AUDIO_OUTPUT:
		{
			if(inputIndex >= 0 && chkInput->isChecked())
			{
				QString selectedInput = cmbInput->itemText(inputIndex);
				vtrack.useInput = true;
				vtrack.inputConfig = qMakePair(0, selectedInput);
			}

			if(outputIndex >= 0 && chkOutput->isChecked())
			{
				QString jackPlayback("system:playback");
				QString selectedOutput = cmbOutput->itemText(outputIndex);
				vtrack.useOutput = true;
				vtrack.outputConfig = qMakePair(0, selectedOutput);
			}
		}
		break;
		case Track::AUDIO_INPUT:
		{
			if(inputIndex >= 0 && chkInput->isChecked())
			{
				QString selectedInput = cmbInput->itemText(inputIndex);
				vtrack.useInput = true;
				vtrack.inputConfig = qMakePair(0, selectedInput);
			}
			if(outputIndex >= 0 && chkOutput->isChecked())
			{
				QString selectedOutput = cmbOutput->itemText(outputIndex);
				vtrack.useOutput = true;
				vtrack.outputConfig = qMakePair(0, selectedOutput);
			}
		}
		break;
		case Track::AUDIO_BUSS:
		{
			if(inputIndex >= 0 && chkInput->isChecked())
			{
				QString selectedInput = cmbInput->itemText(inputIndex);
				vtrack.useInput = true;
				vtrack.inputConfig = qMakePair(0, selectedInput);
			}
			if(outputIndex >= 0 && chkOutput->isChecked())
			{
				QString selectedOutput = cmbOutput->itemText(outputIndex);
				vtrack.useOutput = true;
				vtrack.outputConfig = qMakePair(0, selectedOutput);
			}
		}
		break;
		case Track::AUDIO_AUX:
			//Just add the track type and rename it
		break;
		case Track::AUDIO_SOFTSYNTH:
			valid = false;
        break;
		default:
			valid = false;
	}
	if(valid)
	{
		cleanup();
		if(m_templateMode)
		{
			m_lastTrack = &vtrack;
			emit trackReady(true);
			hide();
		}
		else
		{
			OOTrackManager* tman = new OOTrackManager();
			tman->setPosition(m_insertPosition);
			connect(tman, SIGNAL(trackAdded(QString)), this, SIGNAL(trackAdded(QString)));
			if(tman->addTrack(&vtrack))
			{
				qDebug("Sucessfully added track");
				done(0);
			}
		}
	}
}/*}}}*/

OOVirtualTrack* CreateTrackDialog::getLastTrack()
{
	if(m_lastTrack)
		return m_lastTrack;
	else
		return 0;
}

void CreateTrackDialog::cancelSelected()
{
	cleanup();
	reject();
}

void CreateTrackDialog::cleanup()/*{{{*/
{
	if(m_instrumentLoaded)
	{
		if(!lsClient)
		{
			lsClient = new LSClient(config.lsClientHost, config.lsClientPort);
			lsClientStarted = lsClient->startClient();
		}
		else if(!lsClientStarted)
		{
			lsClientStarted = lsClient->startClient();
		}
		if(lsClientStarted)
		{
			lsClient->removeLastChannel();
			m_instrumentLoaded = false;
		}
	}
}/*}}}*/

void CreateTrackDialog::updateInstrument(int index)
{
	QString instrumentName = cmbInstrument->itemText(index);
	QString trackName = txtName->text();
	for (iMidiInstrument i = midiInstruments.begin(); i != midiInstruments.end(); ++i)
	{
		if ((*i)->iname() == instrumentName && (*i)->isOOMInstrument())
		{
			if(m_instrumentLoaded)
			{//unload the last one
				cleanup();
			}
			if(!lsClient)
			{
				lsClient = new LSClient(config.lsClientHost, config.lsClientPort);
				lsClientStarted = lsClient->startClient();
				if(config.lsClientResetOnStart && lsClientStarted)
				{
					lsClient->resetSampler();
				}
			}
			else if(!lsClientStarted)
			{
				lsClientStarted = lsClient->startClient();
			}
			if(lsClientStarted)
			{
				qDebug("Loading Instrument to LinuxSampler");
				if(lsClient->loadInstrument(*i))
				{
					qDebug("Instrument Map Loaded");
					if(chkAutoCreate->isChecked())
					{
						int map = lsClient->findMidiMap((*i)->iname().toUtf8().constData());
						Patch* p = (*i)->getDefaultPatch();
						if(p && map >= 0)
						{
							if(lsClient->createInstrumentChannel(txtName->text().toUtf8().constData(), p->engine.toUtf8().constData(), p->filename.toUtf8().constData(), p->index, map))
							{
								qDebug("Create Channel for track");
								QString prefix("LinuxSampler:");
								QString postfix("-audio");
								QString audio(QString(prefix).append(trackName).append(postfix));
								QString midi(QString(prefix).append(trackName));
								//reload input/output list and select the coresponding ports respectively
								updateVisibleElements();
								//populateInputList();
								populateOutputList();
								populateMonitorList();
								cmbOutput->setCurrentIndex(cmbOutput->findText(midi));
								cmbMonitor->setCurrentIndex(cmbMonitor->findText(audio));
								m_instrumentLoaded = true;
							}
						}
					}
				}
			}
			break;
		}
	}
}

//Input raw slot
void CreateTrackDialog::updateInputSelected(bool raw)/*{{{*/
{
	cmbInput->setEnabled(raw);
	cmbInChannel->setEnabled(raw);
}/*}}}*/

//Output raw slot
void CreateTrackDialog::updateOutputSelected(bool raw)/*{{{*/
{
	cmbOutput->setEnabled(raw);
	cmbOutChannel->setEnabled(raw);
}/*}}}*/

void CreateTrackDialog::updateBussSelected(bool raw)/*{{{*/
{
	cmbBuss->setEnabled(raw);
}/*}}}*/

//Track type combo slot
void CreateTrackDialog::trackTypeChanged(int type)
{
	m_insertType = cmbType->itemData(type).toInt();
	updateVisibleElements();
	populateInputList();
	populateOutputList();
	populateInstrumentList();
	populateMonitorList();
	populateBussList();
}

void CreateTrackDialog::trackNameEdited()
{
	Track::TrackType type = (Track::TrackType)m_insertType;
	if(type == Track::MIDI)
	{
		bool enabled = false;
		if(!txtName->text().isEmpty())
		{
			Track* t = song->findTrack(txtName->text());
			if(!t)
				enabled = true;
		}
		cmbInstrument->setEnabled(enabled);
	}
}

//Populate input combo based on type
void CreateTrackDialog::populateInputList()/*{{{*/
{
	while(cmbInput->count())
		cmbInput->removeItem(cmbInput->count()-1);
	Track::TrackType type = (Track::TrackType)m_insertType;
	switch(type)
	{
		case Track::MIDI:
		case Track::DRUM:
		{
			m_currentMidiInputList.clear();
			for (int i = 0; i < MIDI_PORTS; ++i)
			{
				MidiPort* mp = &midiPorts[i];
				MidiDevice* md = mp->device();
				
				if (!md)
					continue;

				if ((md->openFlags() & 2))
				{
					QString mdname(md->name());
					if(md->deviceType() == MidiDevice::ALSA_MIDI)
					{
						mdname = QString("(OOMidi) ").append(mdname);
					}
					cmbInput->addItem(mdname, i);
					m_currentMidiInputList.insert(cmbInput->count()-1, mdname);
				}
			}
			populateNewInputList();

			if (!cmbInput->count())
			{
				chkInput->setChecked(false);
				chkInput->setEnabled(false);
			}
		}
		break;
		case Track::WAVE:
		{
			for(iTrack it = song->inputs()->begin(); it != song->inputs()->end(); ++it)
			{
				cmbInput->addItem((*it)->name(), 0);
			}
			importOutputs();
			if (!cmbInput->count())
			{//TODO: Not sure what we could do here except notify the user
				chkInput->setChecked(false);
				chkInput->setEnabled(false);
			}
		}
		break;
		case Track::AUDIO_OUTPUT:
		{
			for(iTrack t = song->tracks()->begin(); t != song->tracks()->end(); ++t)
			{
				if((*t)->isMidiTrack() || (*t)->type() == Track::AUDIO_OUTPUT)
					continue;
				AudioTrack* track = (AudioTrack*) (*t);
				Route r(track, -1);
				cmbInput->addItem(r.name());
			}

			if (!cmbInput->count())
			{//TODO: Not sure what we could do here except notify the user
				chkInput->setChecked(false);
				chkInput->setEnabled(false);
			}
		}
		break;
		case Track::AUDIO_INPUT:
		{
			importOutputs();
			if (!cmbInput->count())
			{//TODO: Not sure what we could do here except notify the user
				chkInput->setChecked(false);
				chkInput->setEnabled(false);
			}
		}
		break;
		case Track::AUDIO_BUSS:
		{
			for(iTrack t = song->tracks()->begin(); t != song->tracks()->end(); ++t)
			{
				if((*t)->isMidiTrack())
					continue;
				AudioTrack* track = (AudioTrack*) (*t);
				if(track->type() == Track::AUDIO_INPUT || track->type() == Track::WAVE || track->type() == Track::AUDIO_SOFTSYNTH)
				{
					cmbInput->addItem(Route(track, -1).name());
				}
			}
			if (!cmbInput->count())
			{//TODO: Not sure what we could do here except notify the user
				chkInput->setChecked(false);
				chkInput->setEnabled(false);
			}
		}
		break;
		case Track::AUDIO_AUX:
		case Track::AUDIO_SOFTSYNTH:
		break;
	}
}/*}}}*/

void CreateTrackDialog::populateOutputList()/*{{{*/
{
	while(cmbOutput->count())
		cmbOutput->removeItem(cmbOutput->count()-1);
	Track::TrackType type = (Track::TrackType)m_insertType;
	switch(type)
	{
		case Track::MIDI:
		case Track::DRUM:
		{
			m_currentMidiOutputList.clear();
			for (int i = 0; i < MIDI_PORTS; ++i)
			{
				MidiPort* mp = &midiPorts[i];
				MidiDevice* md = mp->device();
				
				if (!md)
					continue;

				if((md->openFlags() & 1))
				{
					QString mdname(md->name());
					if(md->deviceType() == MidiDevice::ALSA_MIDI)
					{
						mdname = QString("(OOMidi) ").append(mdname);
					}
					cmbOutput->addItem(mdname, i);
					m_currentMidiOutputList.insert(cmbOutput->count()-1, mdname);
				}
			}

			populateNewOutputList();
			if (!cmbOutput->count())
			{
				chkOutput->setChecked(false);
				chkOutput->setEnabled(false);
			}
		}
		break;
		case Track::WAVE:
		{
			for(iTrack t = song->tracks()->begin(); t != song->tracks()->end(); ++t)
			{
				if((*t)->isMidiTrack())
					continue;
				AudioTrack* track = (AudioTrack*) (*t);
				if(track->type() == Track::AUDIO_OUTPUT || track->type() == Track::AUDIO_BUSS)
				{
					cmbOutput->addItem(Route(track, -1).name());
				}
			}
			if (!cmbOutput->count())
			{
				chkOutput->setChecked(false);
				chkOutput->setEnabled(false);
			}
		}
		break;
		case Track::AUDIO_OUTPUT:
		{
			importInputs();
			if (!cmbOutput->count())
			{
				chkOutput->setChecked(false);
				chkOutput->setEnabled(false);
			}
		}
		break;
		case Track::AUDIO_INPUT:
		{
			for(iTrack t = song->tracks()->begin(); t != song->tracks()->end(); ++t)
			{
				if((*t)->isMidiTrack())
					continue;
				AudioTrack* track = (AudioTrack*) (*t);
				switch(track->type())
				{
					case Track::AUDIO_OUTPUT:
					case Track::AUDIO_BUSS:
					case Track::WAVE:
						cmbOutput->addItem(Route(track, -1).name());
					break;
					default:
					break;
				}
			}
			if (!cmbOutput->count())
			{
				chkOutput->setChecked(false);
				chkOutput->setEnabled(false);
			}
		}
		break;
		case Track::AUDIO_BUSS:
		{
			for(iTrack t = song->outputs()->begin(); t != song->outputs()->end(); ++t)
			{
				AudioTrack* track = (AudioTrack*) (*t);
				cmbOutput->addItem(Route(track, -1).name());
			}
			if (!cmbOutput->count())
			{
				chkOutput->setChecked(false);
				chkOutput->setEnabled(false);
			}
		}
		break;
		case Track::AUDIO_AUX:
		case Track::AUDIO_SOFTSYNTH:
		break;
	}
}/*}}}*/

void CreateTrackDialog::importOutputs()/*{{{*/
{
	if (checkAudioDevice()) 
	{
		std::list<QString> sl = audioDevice->outputPorts();
		for (std::list<QString>::iterator i = sl.begin(); i != sl.end(); ++i) {
			cmbInput->addItem(*i, 1);
		}
	}
}/*}}}*/

void CreateTrackDialog::importInputs()/*{{{*/
{
	if (checkAudioDevice()) 
	{
		std::list<QString> sl = audioDevice->inputPorts();
		for (std::list<QString>::iterator i = sl.begin(); i != sl.end(); ++i) {
			cmbOutput->addItem(*i, 1);
		}
	}
}/*}}}*/

void CreateTrackDialog::populateMonitorList()/*{{{*/
{
	while(cmbMonitor->count())
		cmbMonitor->removeItem(cmbMonitor->count()-1);
	if (checkAudioDevice()) 
	{
		std::list<QString> sl = audioDevice->outputPorts();
		for (std::list<QString>::iterator i = sl.begin(); i != sl.end(); ++i) {
			cmbMonitor->addItem(*i, 1);
		}
	}
}/*}}}*/

void CreateTrackDialog::populateBussList()/*{{{*/
{
	while(cmbBuss->count())
		cmbBuss->removeItem(cmbBuss->count()-1);
	cmbBuss->addItem(tr("Add New Buss"), 1);
	for(iTrack it = song->groups()->begin(); it != song->groups()->end(); ++it)
	{
		cmbBuss->addItem((*it)->name(), 0);
	}
}/*}}}*/

void CreateTrackDialog::populateNewInputList()/*{{{*/
{
	//while(cmbInput->count())
	//	cmbInput->removeItem(cmbInput->count()-1);
	//m_createMidiInputDevice = true;
	for (iMidiDevice i = midiDevices.begin(); i != midiDevices.end(); ++i)
	{
		if ((*i)->deviceType() == MidiDevice::ALSA_MIDI)
		{
			cmbInput->addItem((*i)->name(), MidiDevice::ALSA_MIDI);
		}
	}
	if(audioDevice->deviceType() != AudioDevice::JACK_AUDIO)
		return;
	std::list<QString> sl = audioDevice->outputPorts(true, m_showJackAliases);
	for (std::list<QString>::iterator ip = sl.begin(); ip != sl.end(); ++ip)
	{
		cmbInput->addItem(*ip, MidiDevice::JACK_MIDI);
	}
}/*}}}*/

void CreateTrackDialog::populateNewOutputList()/*{{{*/
{
	//while(cmbOutput->count())
	//	cmbOutput->removeItem(cmbOutput->count()-1);
	//m_createMidiOutputDevice = true;
	if(audioDevice->deviceType() != AudioDevice::JACK_AUDIO)
		return;
	std::list<QString> sl = audioDevice->inputPorts(true, m_showJackAliases);
	for (std::list<QString>::iterator ip = sl.begin(); ip != sl.end(); ++ip)
	{
		cmbOutput->addItem(*ip, MidiDevice::JACK_MIDI);
	}
}/*}}}*/

void CreateTrackDialog::populateInstrumentList()/*{{{*/
{
    cmbInstrument->clear();

    if (m_insertType == Track::MIDI)
    {
        for (iMidiInstrument i = midiInstruments.begin(); i != midiInstruments.end(); ++i)
        {
            //SynthI* si = dynamic_cast<SynthI*> (*i);
            //if (!si)
                cmbInstrument->addItem((*i)->iname());
        }
        //Default to the GM instrument
        int gm = cmbInstrument->findText("GM");
        if(gm >= 0)
            cmbInstrument->setCurrentIndex(gm);
    }
}/*}}}*/

int CreateTrackDialog::getFreeMidiPort()/*{{{*/
{
	int rv = -1;
	for (int i = 0; i < MIDI_PORTS; ++i)
	{
		MidiPort* mp = &midiPorts[i];
		MidiDevice* md = mp->device();
		
		//Use the first unconfigured port
		if (!md)
		{
			rv = i;
			break;
		}
	}
	return rv;
}/*}}}*/

void CreateTrackDialog::updateVisibleElements()/*{{{*/
{
	chkInput->setEnabled(true);
	chkOutput->setEnabled(true);
	chkInput->setChecked(true);
	chkOutput->setChecked(true);
	chkBuss->setChecked(true);
	chkAutoCreate->setChecked(true);

	Track::TrackType type = (Track::TrackType)m_insertType;
	switch (type)
	{
		case Track::MIDI:
		case Track::DRUM:
		{
			cmbInChannel->setVisible(true);
			cmbOutChannel->setVisible(true);
			lblInstrument->setVisible(true);
			cmbInstrument->setVisible(true);
			cmbInstrument->setEnabled(true);
			cmbMonitor->setVisible(true);
			cmbInput->setVisible(true);
			chkInput->setVisible(true);
			cmbOutput->setVisible(true);
			chkOutput->setVisible(true);
			midiBox->setVisible(true);
			chkAutoCreate->setVisible(true);
			trackNameEdited();

            m_height = 290;
			m_width = width();
		}
		break;
		case Track::AUDIO_OUTPUT:
		{
			cmbInChannel->setVisible(false);
			cmbOutChannel->setVisible(false);
			lblInstrument->setVisible(false);
			cmbInstrument->setVisible(false);
			cmbMonitor->setVisible(false);
			midiBox->setVisible(false);
			chkAutoCreate->setVisible(false);

			cmbInput->setVisible(true);
			chkInput->setVisible(true);
			cmbOutput->setVisible(true);
			chkOutput->setVisible(true);
			
			m_height = 160;
			m_width = width();
		}
		break;
		case Track::AUDIO_INPUT:
		{
			cmbInChannel->setVisible(false);
			cmbOutChannel->setVisible(false);
			lblInstrument->setVisible(false);
			cmbInstrument->setVisible(false);
			cmbMonitor->setVisible(false);
			midiBox->setVisible(false);
			chkAutoCreate->setVisible(false);

			cmbInput->setVisible(true);
			chkInput->setVisible(true);
			cmbOutput->setVisible(true);
			chkOutput->setVisible(true);
			
			m_height = 160;
			m_width = width();
		}
		break;
		case Track::WAVE:
		{
			cmbInChannel->setVisible(false);
			cmbOutChannel->setVisible(false);
			lblInstrument->setVisible(false);
			cmbInstrument->setVisible(false);
			cmbMonitor->setVisible(false);
			midiBox->setVisible(false);
			chkAutoCreate->setVisible(false);

			cmbInput->setVisible(true);
			chkInput->setVisible(true);
			cmbOutput->setVisible(true);
			chkOutput->setVisible(true);
			
			m_height = 160;
			m_width = width();
		}
		break;
		case Track::AUDIO_BUSS:
		{
			cmbInChannel->setVisible(false);
			cmbOutChannel->setVisible(false);
			lblInstrument->setVisible(false);
			cmbInstrument->setVisible(false);
			cmbMonitor->setVisible(false);
			midiBox->setVisible(false);
			chkAutoCreate->setVisible(false);

			cmbInput->setVisible(true);
			chkInput->setVisible(true);
			cmbOutput->setVisible(true);
			chkOutput->setVisible(true);
			
			m_height = 160;
			m_width = width();
		}
		break;
		case Track::AUDIO_AUX:
		{
			cmbInChannel->setVisible(false);
			cmbOutChannel->setVisible(false);
			lblInstrument->setVisible(false);
			cmbInstrument->setVisible(false);
			cmbMonitor->setVisible(false);
			chkAutoCreate->setVisible(false);
			
			midiBox->setVisible(false);
			
			cmbInput->setVisible(false);
			chkInput->setVisible(false);

			cmbOutput->setVisible(false);
			chkOutput->setVisible(false);
			
			m_height = 100;
			m_width = width();
		}
		default:
		break;
	}
	setFixedHeight(m_height);
	updateGeometry();
}/*}}}*/

void CreateTrackDialog::showEvent(QShowEvent*)
{
	qDebug("Inside CreateTrackDialog::showEvent trackType: %i, position: %i", m_insertType, m_insertPosition);
	updateVisibleElements();
	populateInputList();
	populateOutputList();
	populateInstrumentList();
	populateMonitorList();
	populateBussList();
}

/*QSize CreateTrackDialog::sizeHint()
{
	return QSize(m_width, m_height);
	//return QSize(450, 100);
}*/