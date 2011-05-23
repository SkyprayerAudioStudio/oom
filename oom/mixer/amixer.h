//=========================================================
//  OOMidi
//  OpenOctave Midi and Audio Editor
//  $Id: amixer.h,v 1.27.2.2 2009/10/18 06:13:00 terminator356 Exp $
//
//  (C) Copyright 2000-2004 Werner Schweer (ws@seh.de)
//=========================================================

#ifndef __AMIXER_H__
#define __AMIXER_H__

#include <QScrollArea>

#include "cobject.h"
#include "synth.h"
#include "node.h"

class QHBoxLayout;
class QLabel;
class QMenu;
class QToolButton;
class QWidget;
class QShowEvent;

class Xml;
class AudioTrack;
class Meter;
class Track;
class Slider;
class Knob;
class DoubleLabel;
class ComboBox;
class AudioPortConfig;
class Strip;

struct MixerConfig;

#define EFX_HEIGHT     16

typedef std::list<Strip*> StripList;

//---------------------------------------------------------
//   ScrollArea
//---------------------------------------------------------

class ScrollArea : public QScrollArea
{
    Q_OBJECT

signals:
    void layoutRequest();

protected:
    virtual bool viewportEvent(QEvent* event);

public:

    ScrollArea(QWidget* parent = 0) : QScrollArea(parent)
    {
    }
};

//---------------------------------------------------------
//   AudioMixerApp
//---------------------------------------------------------

class AudioMixerApp : public QMainWindow
{
    //QString name;
    MixerConfig* cfg;
    StripList stripList;
    QScrollArea* view;
    QWidget* central;
    QHBoxLayout* lbox;
    //Strip* master;
    QHBoxLayout* layout;
    QMenu* menuView;
    AudioPortConfig* routingDialog;
    QAction* routingId;
    int oldAuxsSize;

    QAction* showMidiTracksId;
    QAction* showDrumTracksId;
    QAction* showInputTracksId;
    QAction* showOutputTracksId;
    QAction* showWaveTracksId;
    QAction* showGroupTracksId;
    QAction* showAuxTracksId;
    QAction* showSyntiTracksId;
    QAction* toggleShowEffectsRackAction;

    Q_OBJECT

protected:
    virtual void closeEvent(QCloseEvent*);
	virtual void showEvent(QShowEvent*);

    void addStrip(Track*, int);
    void showAudioPortConfig(bool);

    enum UpdateAction
    {
        NO_UPDATE, UPDATE_ALL, UPDATE_MIDI, STRIP_INSERTED, STRIP_REMOVED
    };
    void updateMixer(UpdateAction);

signals:
    void closed();
    //void layoutRequest();

private slots:
    void songChanged(int);
    //void configChanged()    { songChanged(-1); }
    void configChanged();
    void setSizing();
    void toggleAudioPortConfig();
    void routingDialogClosed();
    //void showTracksChanged(QAction*);
    void showMidiTracksChanged(bool);
    void showDrumTracksChanged(bool);
    void showWaveTracksChanged(bool);
    void showInputTracksChanged(bool);
    void showOutputTracksChanged(bool);
    void showGroupTracksChanged(bool);
    void showAuxTracksChanged(bool);
    void showSyntiTracksChanged(bool);
    void toggleShowEffectsRack();

    //protected:
    //   virtual bool event(QEvent* event);

public:
    //AudioMixerApp(QWidget* parent);
    AudioMixerApp(QWidget* parent, MixerConfig* c);
    ~AudioMixerApp();
    //void write(Xml&, const char* name);
    //void write(int level, Xml& xml, const char* name);
    void write(int level, Xml& xml);
    void clear();
};

#endif

