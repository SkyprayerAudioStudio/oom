//===========================================================
//  OOMidi
//  OpenOctave Midi and Audio Editor
//  (C) Copyright 2011 Andrew Williams & Christopher Cherrett
//===========================================================

#include <QtGui>
#include "tracklistview.h"
#include "globals.h"
#include "gconfig.h"
#include "app.h"
#include "song.h"
#include "track.h"
#include "part.h"
#include "midieditor.h"

TrackListView::TrackListView(MidiEditor* editor, QWidget* parent)
: QFrame(parent)
{
	m_editor = editor;
	m_layout = new QVBoxLayout(this);
	m_layout->setContentsMargins(8, 2, 8, 2);
	m_model = new QStandardItemModel(0, 2, this);
	m_table = new QTableView(this);
	m_table->setObjectName("TrackListView");
	m_table->setModel(m_model);
	m_table->setAlternatingRowColors(true);
	m_table->setSelectionMode(QAbstractItemView::SingleSelection);
	m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_table->setCornerButtonEnabled(false);
	m_table->horizontalHeader()->setStretchLastSection(true);
	m_table->verticalHeader()->hide();
	m_layout->addWidget(m_table);
	songChanged(-1);
	connect(song, SIGNAL(songChanged(int)), this, SLOT(songChanged(int)));
	connect(m_model, SIGNAL(itemChanged(QStandardItem*)), this, SLOT(toggleTrackPart(QStandardItem*)));
}

TrackListView::~TrackListView()
{
}

void TrackListView::songChanged(int flags)/*{{{*/
{
	if(flags == -1 || flags & (SC_TRACK_INSERTED | SC_TRACK_REMOVED | SC_PART_INSERTED | SC_PART_REMOVED))
	{
		m_model->clear();
		for(iMidiTrack i = song->midis()->begin(); i != song->midis()->end(); ++i)
		{
			MidiTrack* track = (MidiTrack*)(*i);
			PartList* pl = track->parts();
			QList<QStandardItem*> trackRow;
			QStandardItem* chkTrack = new QStandardItem(true);
			chkTrack->setCheckable(true);
			chkTrack->setData(1, TrackRole);
			chkTrack->setData(track->name(), TrackNameRole);
			if(m_selected.contains(track->name()))
				chkTrack->setCheckState(Qt::Checked);
			trackRow.append(chkTrack);
			QStandardItem* trackName = new QStandardItem();
			trackName->setText(track->name());
			QFont font = trackName->font();
			font.setBold(true);
			trackName->setFont(font);
			trackRow.append(trackName);
			m_model->appendRow(trackRow);
			for(iPart ip = pl->begin(); ip != pl->end(); ++ip)
			{
				QList<QStandardItem*> partsRow;
				Part* part = ip->second;
				QStandardItem* chkPart = new QStandardItem(true);
				chkPart->setCheckable(true);
				chkPart->setData(part->sn(), PartRole);
				chkPart->setData(2, TrackRole);
				chkPart->setData(track->name(), TrackNameRole);
				chkPart->setData(part->tick(), TickRole);
				if(m_editor->hasPart(part->sn()))
				{
					chkPart->setCheckState(Qt::Checked);
				}
				QStandardItem* partName = new QStandardItem(part->name());
				partsRow.append(chkPart);
				partsRow.append(partName);
				m_model->appendRow(partsRow);
			}
		}
		m_table->setColumnWidth(0, 20);
	}
}/*}}}*/

void TrackListView::toggleTrackPart(QStandardItem* item)
{
	int type = item->data(TrackRole).toInt();
	bool checked = (item->checkState() == Qt::Checked);
	QString trackName = item->data(TrackNameRole).toString();
	Track* track = song->findTrack(trackName);
	if(!track || !m_editor)
		return;

	PartList* list = track->parts();
	if(list->empty())
		return;
	switch(type)
	{
		case 1: //Track
		{
			iPart i;
			if(checked)
			{
				m_editor->addParts(list);
				m_selected.append(trackName);
			}
			else
			{
				m_editor->removeParts(list);
				m_editor->updateCanvas();
				m_selected.removeAll(trackName);
				song->update(SC_SELECTION);
			}
			if(!list->empty())
			{
				if(checked)
					m_editor->setCurCanvasPart(list->begin()->second);
				m_model->blockSignals(true);
				songChanged(-1);
				m_model->blockSignals(false);
			}
		}
		break;
		case 2: //Part
		{
			int sn = item->data(PartRole).toInt();
			unsigned tick = item->data(TickRole).toInt();
			Part* part = list->find(tick, sn);
			if(part)
			{
				if(checked)
				{
					m_editor->addPart(part);
					m_editor->setCurCanvasPart(part);
				}
				else
				{
					m_editor->removePart(sn);
					m_editor->updateCanvas();
					m_selected.removeAll(trackName);
					m_model->blockSignals(true);
					songChanged(-1);
					m_model->blockSignals(false);
					song->update(SC_SELECTION);
				}
			}
		}
		break;
	}
	update();
}

void TrackListView::removePart(unsigned, QString)
{
}
