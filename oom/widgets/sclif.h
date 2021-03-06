//=========================================================
//  OOMidi
//  OpenOctave Midi and Audio Editor
//    $Id: sclif.h,v 1.1.1.1 2003/10/27 18:54:33 wschweer Exp $

//    Copyright (C) 1997  Josef Wilgen
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License, version 2,
//	as published by	the Free Software Foundation.
//
//    (C) Copyright 2000 Werner Schweer (ws@seh.de)
//=========================================================

#ifndef __SCALE_IF_H__
#define __SCALE_IF_H__

#include "scldraw.h"

//---------------------------------------------------------
//   ScaleIf
//---------------------------------------------------------

class ScaleIf
{
    bool d_userScale;

protected:
    ScaleDraw d_scale;
    int d_maxMajor;
    int d_maxMinor;

    bool hasUserScale()
    {
        return d_userScale;
    }
    virtual void scaleChange() = 0;

public:
    ScaleIf();

    virtual ~ScaleIf()
    {
    };

    void setScale(double vmin, double vmax, int logarithmic = 0);
    void setScale(double vmin, double vmax, double step, int logarithmic = 0);
    void setScale(const ScaleDiv &s);
    void setScaleMaxMajor(int ticks);
    void setScaleMaxMinor(int ticks);
    void autoScale();

    int scaleMaxMinor() const
    {
        return d_maxMinor;
    }

    int scaleMaxMajor() const
    {
        return d_maxMinor;
    }
};

#endif

