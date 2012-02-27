#include <QtGui>
#include "CanvasNavigator.h"
#include "ComposerCanvas.h"
#include "config.h"
#include "globals.h"
#include "gconfig.h"
#include "song.h"
#include "track.h"
#include "part.h"

static const int MIN_PART_HEIGHT = 1;
static const double TICK_PER_PIXEL = 81.37;

CanvasNavigator::CanvasNavigator(QWidget* parent)
: QWidget(parent)
{
	QHBoxLayout* layout = new QHBoxLayout(this);
	layout->setContentsMargins(0,0,0,0);
	layout->setSpacing(0);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	setFixedHeight(80);

	m_scene = new QGraphicsScene(0, 0, 400, 48);
	m_scene->setBackgroundBrush(QColor(63, 63, 63));
	
	m_view = new QGraphicsView(m_scene);
	m_view->setRenderHints(QPainter::Antialiasing);
	m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	m_view->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
	layout->addWidget(m_view);
}

void CanvasNavigator::setCanvas(ComposerCanvas* c)
{
	m_canvas = c;
	//TODO: Update all parts
}

void CanvasNavigator::updateParts()
{
	m_tracks.clear();
	m_scene->clear();
	int partHeight = 2;
	//if(partHeight < MIN_PART_HEIGHT)
	//	partHeight = MIN_PART_HEIGHT;
	int index = 0;
	TrackList* tl = song->visibletracks();
	for(ciTrack ci = tl->begin(); ci != tl->end(); ci++)
	{
		if((*ci)->type() == Track::WAVE || (*ci)->type() == Track::MIDI)
		{
			m_tracks.append((*ci)->id());

			Track* track = *ci;
			if(track)
			{
				PartList* parts = track->parts();
				if(parts)
				{
					for(iPart p = parts->begin(); p != parts->end(); p++)
					{
						Part *part =  p->second;

						int tick, len;
						if(part && track->isMidiTrack())
						{
							tick = part->tick();
							len = part->lenTick();
						}
						else
						{
							tick = tempomap.frame2tick(part->frame());
							len = tempomap.frame2tick(part->lenFrame());
						}
						double w = calcSize(len);//m_canvas->mapx(len)+m_canvas->xOffsetDev();
						double pos = calcSize(tick);//m_canvas->mapx(tick)+m_canvas->xOffsetDev();
					//	qDebug("CanvasNavigator::updateParts: tick: %d, len: %d , pos: %d, w: %d", tick, len, pos, w);
						QGraphicsRectItem* item = m_scene->addRect(pos, index*partHeight, w, partHeight);
						item->setBrush(config.partColors[part->colorIndex()]);
						item->setPen(Qt::NoPen);
					}
				}
				++index;
			}
		}
	}
	updateSpacing();
}

double CanvasNavigator::calcSize(int val)
{
	double rv = 0.0;
	rv = ((val / TICK_PER_PIXEL) * 8)/100;
	return rv;
}

void CanvasNavigator::resizeEvent(QResizeEvent*)
{
	updateSpacing();
}

void CanvasNavigator::updateSpacing()
{
	QRectF bounds = m_scene->itemsBoundingRect();
	//m_view->fitInView(bounds, Qt::IgnoreAspectRatio);
	m_view->fitInView(bounds, Qt::KeepAspectRatio);
	//m_view->fitInView(bounds, Qt::KeepAspectRatioByExpanding);
}

/*QSize CanvasNavigator::sizeHint() const
{
	return QSize(400, 80);
}

QSize CanvasNavigator::minimumSizeHint() const
{
	return sizeHint();
}*/