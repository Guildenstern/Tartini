/***************************************************************************
                          freqdrawwidget.cpp  -  description
                             -------------------
    begin                : Fri Dec 10 2004
    copyright            : (C) 2004-2005 by Philip McLeod
    email                : pmcleod@cs.otago.ac.nz
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   Please read LICENSE.txt for details.
 ***************************************************************************/

#include "freqwidgetGL.h"
#include "mygl.h"
#include "musicnotes.h"
#include "channel.h"
#include "analysisdata.h"
#include "myqt.h"
#include "notedata.h"
#include <QtGui/QMouseEvent>

// zoom cursors
#include "pics/zoomx.xpm"
#include "pics/zoomy.xpm"
#include "pics/zoomxout.xpm"
#include "pics/zoomyout.xpm"

#ifndef WHEEL_DELTA
#define WHEEL_DELTA 120
#endif

FreqWidgetGL::FreqWidgetGL(QWidget * /*parent*/, const char* /*name*/)
{
  setMouseTracking(true);

   dragMode = DragNone;

   QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
   sizePolicy.setHeightForWidth(false);
   setSizePolicy(sizePolicy);

   setFocusPolicy(Qt::StrongFocus);
   gdata->view->setPixelHeight(height());
}

void FreqWidgetGL::initializeGL()
{
  qglClearColor(gdata->backgroundColor());
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnableClientState(GL_VERTEX_ARRAY);
  //glEnableClientState(GL_COLOR_ARRAY);
  //setLineWidth(1.0f);
}

void FreqWidgetGL::resizeGL(int w, int h)
{
  mygl_resize2d(w, h);
}

FreqWidgetGL::~FreqWidgetGL()
{
}

void FreqWidgetGL::drawReferenceLinesGL(double /*leftTime*/, double currentTime, double zoomX, double viewBottom, double zoomY, int /*viewType*/)
{
  // Draw the lines and notes
  QFontMetrics fm = fontMetrics();
  int fontHeightSpace = fm.height() / 4;
  int fontWidth = fm.width("C#0") + 3;

  MusicKey &musicKey = gMusicKeys[gdata->temperedType()];
  MusicScale &musicScale = gMusicScales[gdata->musicKeyType()];

  int keyRoot = cycle(gMusicKeyRoot[gdata->musicKey()]+musicScale.semitoneOffset(), 12);
  int viewBottomNote = (int)viewBottom - keyRoot;
  int remainder = cycle(viewBottomNote, 12);
  int lowRoot = viewBottomNote - remainder + keyRoot;
  int rootOctave = lowRoot / 12;
  int rootOffset = cycle(lowRoot, 12);
  int numOctaves = int(ceil(zoomY * (double)height() / 12.0))+1;
  int topOctave = rootOctave + numOctaves;
  double lineY;
  double curRoot;
  double curPitch;
  int stippleOffset = toInt(currentTime / zoomX) % 16;
  QString noteLabel;
  int nameIndex;

  //Draw the horizontal reference lines
  for(int octave = rootOctave; octave < topOctave; ++octave) {
    curRoot = double(octave*12 + rootOffset);
    for(int j=0; j<musicKey.size(); j++) {
      if(musicScale.hasSemitone(musicKey.noteType(j))) { //draw it
        curPitch = curRoot + musicKey.noteOffset(j);
        lineY = double(height()) - myround((curPitch - viewBottom)/zoomY);
		if(j == 0) { //root note
			glColor4ub(0, 0, 0, 128);
      mygl_line(fontWidth, lineY, width() - 1, lineY);
		} else if((gdata->musicKeyType() == MusicScale::Chromatic) && !gMusicScales[MusicScale::Major].hasSemitone(musicKey.noteType(j))) {
			glColor4ub(25, 125, 170, 128);
      glEnable(GL_LINE_STIPPLE);
      glLineStipple(1, my_wrap_right(0xFCFC, stippleOffset));  // bitpattern 64716 = 1111110011001100
      mygl_line(fontWidth, lineY, width() - 1, lineY);
      glDisable(GL_LINE_STIPPLE);
		} else {
			glColor4ub(25, 125, 170, 196);
      glEnable(GL_LINE_STIPPLE);
      glLineStipple(1, my_wrap_right(0xFFFC, stippleOffset));
      mygl_line(fontWidth, lineY, width() - 1, lineY);
      glDisable(GL_LINE_STIPPLE);
		}
      }
      if(zoomY > 0.1) break;
    }
  }

  //Draw the text labels down the left hand side
  for(int octave = rootOctave; octave < topOctave; ++octave) {
    curRoot = double(octave*12 + rootOffset);
    for(int j=0; j<musicKey.size(); j++) {
      if(musicScale.hasSemitone(musicKey.noteType(j))) { //draw it
        curPitch = curRoot + musicKey.noteOffset(j);
        lineY = double(height()) - myround((curPitch - viewBottom)/zoomY);
        nameIndex = toInt(curPitch);
        glColor3ub(0, 0, 0);
        noteLabel.sprintf("%s%d", noteName(nameIndex), noteOctave(nameIndex));
        renderText(2, toInt(lineY) + fontHeightSpace, noteLabel);
      }
      if(zoomY > 0.1) break;
    }
  }

  //Draw the vertical reference lines
  double timeStep = timeWidth() / double(width()) * 150.0; //time per 150 pixels
  double timeScaleBase = pow10(floor(log10(timeStep))); //round down to the nearest power of 10

  //choose a timeScaleStep which is a multiple of 1, 2 or 5 of timeScaleBase
  int largeFreq;
  if(timeScaleBase * 5.0 < timeStep) { largeFreq = 5; }
  else if (timeScaleBase * 2.0 < timeStep) { largeFreq = 2; }
  else { largeFreq = 2; timeScaleBase /= 2; }

  double timePos = floor(leftTime() / (timeScaleBase*largeFreq)) * (timeScaleBase*largeFreq); //calc the first one just off the left of the screen
  int x, largeCounter=-1;
  double ratio = double(width()) / timeWidth();
  double lTime = leftTime();

  for(; timePos <= rightTime(); timePos += timeScaleBase) {
    if(++largeCounter == largeFreq) {
      largeCounter = 0;
      glColor4ub(25, 125, 170, 128); //draw the darker lines
    } else {
      glColor4ub(25, 125, 170, 64); //draw the lighter lines
	}
    x = toInt((timePos-lTime) * ratio);
    mygl_line(x, 0, x, height()-1);
  }
}

void FreqWidgetGL::paintGL()
{
  glClear(GL_COLOR_BUFFER_BIT);
  View *view = gdata->view;


  if(view->autoFollow() && gdata->getActiveChannel() && gdata->running == STREAM_FORWARD)
    setChannelVerticalView(gdata->getActiveChannel(), view->viewLeft(), view->currentTime(), view->zoomX(), view->viewBottom(), view->zoomY());
  int curTimePixel = view->screenPixelX(view->currentTime());

  glLineWidth(3.0f);

  //draw the red/blue background color shading if needed
  if(view->backgroundShading() && gdata->getActiveChannel())
    drawChannelFilledGL(gdata->getActiveChannel(), view->viewLeft(), view->currentTime(), view->zoomX(), view->viewBottom(), view->zoomY(), DRAW_VIEW_NORMAL);

  glLineWidth(1.0f);
  drawReferenceLinesGL(view->viewLeft(), view->currentTime(), view->zoomX(), view->viewBottom(), view->zoomY(), DRAW_VIEW_NORMAL);

  glEnable(GL_LINE_SMOOTH);
  //draw all the visible channels
  for (uint i = 0; i < gdata->channels.size(); i++) {
   	Channel *ch = gdata->channels.at(i);
   	if(!ch->isVisible()) continue;
    drawChannelGL(ch, view->viewLeft(), view->currentTime(), view->zoomX(), view->viewBottom(), view->zoomY(), DRAW_VIEW_NORMAL);
  }
  glDisable(GL_LINE_SMOOTH);

  // Draw a light grey band indicating which time is being used in the current window
  if(gdata->getActiveChannel()) {
    QColor lineColor = palette().color(foregroundRole());
    lineColor.setAlpha(50);
    Channel *ch = gdata->getActiveChannel();
    double halfWindowTime = (double)ch->size() / (double)(ch->rate() * 2);
    int pixelLeft = view->screenPixelX(view->currentTime() - halfWindowTime);
    int pixelRight = view->screenPixelX(view->currentTime() + halfWindowTime);
    qglColor(lineColor);
    mygl_rect(pixelLeft, 0, pixelRight-pixelLeft, height()-1);
  }

  // Draw the current time line
  qglColor(palette().color(foregroundRole()));
  glLineWidth(1.0f);
  mygl_line(curTimePixel, 0, curTimePixel, height() - 1);
}


Channel *FreqWidgetGL::channelAtPixel(int x, int y)
{
  double time = mouseTime(x);
  float pitch = mousePitch(y);
  float tolerance = 6 * gdata->view->zoomY(); //10 pixel tolerance

  std::vector<Channel*> channels;

  //loop through channels in reverse order finding which one the user clicked on
  for (std::vector<Channel*>::reverse_iterator it = gdata->channels.rbegin(); it != gdata->channels.rend();  it++) {
    if((*it)->isVisible()) {
      AnalysisData *data = (*it)->dataAtTime(time);
      if(data && within(tolerance, data->pitch, pitch)) return *it;
    }
  }
  return NULL;
}

/*
 * If control or alt is pressed, zooms. If shift is also pressed, it 'reverses' the zoom: ie ctrl+shift zooms x
 * out, alt+shift zooms y out. Otherwise, does internal processing.
 *
 * @param e the QMouseEvent to respond to.
 */
void FreqWidgetGL::mousePressEvent( QMouseEvent *e)
{
  View *view = gdata->view;
  int timeX = toInt(view->viewOffset() / view->zoomX());
  bool zoomed = false;
  dragMode = DragNone;
  
  
  //Check if user clicked on center bar, to drag it
  if(within(4, e->x(), timeX)) {
    dragMode = DragTimeBar;
    mouseX = e->x();
    return;
  }

  // If the control or alt keys are pressed, zoom in or out on the correct axis, otherwise scroll.
  if (e->modifiers() & Qt::ControlModifier) {
    // Do we zoom in or out?
    if (e->modifiers() & Qt::ShiftModifier) {
      //view->viewZoomOutX();
    } else {
      //view->viewZoomInX();
    }
    zoomed = true;
  } else if (e->modifiers() & Qt::AltModifier) {
    // Do we zoom in or out?
    if (e->modifiers() & Qt::ShiftModifier) {
      //view->viewZoomOutY();
    } else {
      //view->viewZoomInY();
    }
    zoomed = true;
  } else {
    mouseX = e->x();
    mouseY = e->y();

    Channel *ch = channelAtPixel(e->x(), e->y());
    if(ch) { //Clicked on a channel
      gdata->setActiveChannel(ch);
      dragMode = DragChannel;
    } else {
      //Must have clicked on background
      dragMode = DragBackground;
      downTime = view->currentTime();
      downNote = view->viewBottom();
    }
  }
}

void FreqWidgetGL::mouseMoveEvent( QMouseEvent *e )
{
  View *view = gdata->view;
  int pixelAtCurrentTimeX = toInt(view->viewOffset() / view->zoomX());
  
  switch(dragMode) {
  case DragTimeBar:
    {
      int newX = pixelAtCurrentTimeX + (e->x() - mouseX);
      view->setViewOffset(double(newX) * view->zoomX());
      mouseX = e->x();
      view->doSlowUpdate();
	  }
    break;
  case DragBackground:
    view->setViewBottom(downNote - (mouseY - e->y()) * view->zoomY());
    gdata->updateActiveChunkTime(downTime - (e->x() - mouseX) * view->zoomX());
    view->doSlowUpdate();
    break;
  case DragNone:
    if(within(4, e->x(), pixelAtCurrentTimeX)) {
      setCursor(QCursor(Qt::SplitHCursor));
    } else if(channelAtPixel(e->x(), e->y()) != NULL) {
      setCursor(QCursor(Qt::PointingHandCursor));
    } else {
      unsetCursor();
    }
  }
}

void FreqWidgetGL::mouseReleaseEvent( QMouseEvent * )
{
  dragMode = DragNone;
}

/**
 Calculates at what time the mouse is.
 @param x the mouse's x co-ordinate
 @return the time the mouse is positioned at.
 */
double FreqWidgetGL::mouseTime(int x)
{
	return gdata->view->viewLeft() + gdata->view->zoomX() * x;

}


/**
 Calculates at what note pitch the mouse is at.
 @param x the mouse's y co-ordinate
 @return the pitch the mouse is positioned at.
 */
double FreqWidgetGL::mousePitch(int y)
{
	return gdata->view->viewBottom() + gdata->view->zoomY() * (height() - y);
}

void FreqWidgetGL::wheelEvent(QWheelEvent * e)
{
    View *view = gdata->view;
    double amount = double(e->delta())/WHEEL_DELTA * 0.15;
    bool isZoom = gdata->mouseWheelZooms();
    if(e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) isZoom = !isZoom;

  if(isZoom) {
      if(e->delta() >= 0) { //zooming in
        double before = view->logZoomY();
        view->setZoomFactorY(view->logZoomY() + amount, height() - e->y());
        amount = view->logZoomY() - before;
        if(gdata->running == STREAM_FORWARD) {
          view->setZoomFactorX(view->logZoomX() + amount);
        } else { //zoom toward mouse pointer
          view->setZoomFactorX(view->logZoomX() + amount, e->x());
        }
      } else { //zoom out toward center
        double before = view->logZoomY();
        view->setZoomFactorY(view->logZoomY() + amount, height()/2);
        amount = view->logZoomY() - before;
        if(gdata->running == STREAM_FORWARD) {
          view->setZoomFactorX(view->logZoomX() + amount);
        } else {
          view->setZoomFactorX(view->logZoomX() + amount, width()/2);
        }
      }
  } else { //mouse wheel scrolls
    view->setViewBottom(view->viewBottom() + amount * 0.75 * view->viewHeight());
  }
  view->doSlowUpdate();

  e->accept();
}

void FreqWidgetGL::resizeEvent (QResizeEvent *q)
{
    QGLWidget::resizeEvent(q);

    if (q->size() == q->oldSize()) return;
    
    View *v = gdata->view;

    double oldViewWidth = double(v->viewWidth());
    
    v->setPixelHeight(height());
    v->setPixelWidth(width());
    // Work out what the times/heights of the view should be based on the zoom factors
    float newYHeight = height() * v->zoomY();
    float newYBottom = v->viewBottom() - ((newYHeight - v->viewHeight()) / 2.0);
    v->setViewOffset(v->viewOffset() / oldViewWidth * v->viewWidth());
    v->setViewBottom(newYBottom);
}

/*
 * Changes the cursor icon to be one of the zoom ones depending on if the control or alt keys were pressed.
 * Otherwise, ignores the event.
 *
 * @param k the QKeyEvent to respond to.
 */
void FreqWidgetGL::keyPressEvent( QKeyEvent *k )
{
  switch (k->key()) {
    case Qt::Key_Control:
    setCursor(QCursor(QPixmap(zoomx)));
    break;
    case Qt::Key_Alt:
    setCursor(QCursor(QPixmap(zoomy)));
    break;
    case Qt::Key_Shift:
    if (k->modifiers() & Qt::ControlModifier) {
      setCursor(QCursor(QPixmap(zoomxout)));
    } else if (k->modifiers() & Qt::AltModifier) {
      setCursor(QCursor(QPixmap(zoomyout)));
    } else {
      k->ignore();
    }
    break;
    default:
    k->ignore();
    break;
  }
  
}

/*
 * Unsets the cursor icon if the control or alt key was released. Otherwise, ignores the event.
 *
 * @param k the QKeyEvent to respond to.
 */
void FreqWidgetGL::keyReleaseEvent( QKeyEvent *k)
{
  switch (k->key()) {
    case Qt::Key_Control: // Unset the cursor if the control or alt keys were released, ignore otherwise
    case Qt::Key_Alt:
      unsetCursor();
    break;
    case Qt::Key_Shift:
    if (k->modifiers() & Qt::ControlModifier) {
      setCursor(QCursor(QPixmap(zoomx)));
    } else if (k->modifiers() & Qt::AltModifier) {
      setCursor(QCursor(QPixmap(zoomy)));
    } else {
      k->ignore();
    }
    break;
    default:
    k->ignore();
    break;
  }
}

void FreqWidgetGL::leaveEvent ( QEvent * e) {
  unsetCursor();
  QWidget::leaveEvent(e);
}

void FreqWidgetGL::drawChannelGL(Channel *ch, double leftTime, double currentTime, double zoomX, double viewBottom, double zoomY, int viewType)
{
  viewBottom += gdata->semitoneOffset();
  float lineWidth = 3.0f;
  float lineHalfWidth = lineWidth/2;
  ZoomLookup *z;
  if(viewType == DRAW_VIEW_SUMMARY) z = &ch->summaryZoomLookup;
  else z = &ch->normalZoomLookup;

  ChannelLocker channelLocker(ch);

  QColor current = ch->color;
  qglColor(current);

  int viewBottomOffset = toInt(viewBottom / zoomY);
  viewBottom = double(viewBottomOffset) * zoomY;
  
  // baseX is the no. of chunks a pixel must represent.
  double baseX = zoomX / ch->timePerChunk();

  z->setZoomLevel(baseX);
  
  double currentChunk = ch->chunkFractionAtTime(currentTime);
  double leftFrameTime = currentChunk - ((currentTime - leftTime) / ch->timePerChunk());

  double frameTime = leftFrameTime;
  int n = 0;
  int baseElement = int(floor(frameTime / baseX));
  if(baseElement < 0) { n -= baseElement; baseElement = 0; }
  int lastBaseElement = int(floor(double(ch->totalChunks()) / baseX));

  if (baseX > 1) { // More samples than pixels
    int theWidth = width();
    if(lastBaseElement > z->size()) z->setSize(lastBaseElement);
    for(; n < theWidth && baseElement < lastBaseElement; n++, baseElement++) {
      myassert(baseElement >= 0);
      ZoomElement &ze = z->at(baseElement);
      if(!ze.isValid()) {
        if(!calcZoomElement(ch, ze, baseElement, baseX)) continue;
      }
     
      if(ze.high() != 0.0f && ze.high() - ze.low() < 1.0) { //if range is closer than one semi-tone then draw a line between them
        qglColor(ze.color());
        //Note: lineTo doen't draw a pixel on the last point of the line
        //p.drawLine(n, pd.height() - lineTopHalfWidth - toInt(ze.high() / zoomY) + viewBottomOffset, n, pd.height() + lineBottomHalfWidth - toInt(ze.low() / zoomY) + viewBottomOffset);
        //mygl_line(float(n), height() - lineHalfWidth - (ze.high() / zoomY) + viewBottomOffset, n, height() + lineHalfWidth - (ze.low() / zoomY) + viewBottomOffset);
        mygl_line(float(n), height() - lineHalfWidth - (ze.high() / zoomY) + viewBottomOffset, n, height() + lineHalfWidth - (ze.low() / zoomY) + viewBottomOffset);
      }
    }
  } else { // More pixels than samples
    float err = 0.0, pitch = 0.0, prevPitch = 0.0, vol;
    int intChunk = (int) floor(frameTime); // Integer version of frame time
    if(intChunk < 0) intChunk = 0;
    double stepSize = 1.0 / baseX; // So we skip some pixels
    int x = 0, y;
  
    double start = (double(intChunk) - frameTime) * stepSize;
    double stop = width() + (2 * stepSize);
    int squareSize = (int(sqrt(stepSize)) / 2) * 2 + 1; //make it an odd number
    int halfSquareSize = squareSize/2;
    int penX=0, penY=0;
    
    for (double n = start; n < stop && intChunk < (int)ch->totalChunks(); n += stepSize, intChunk++) {
      myassert(intChunk >= 0);
      AnalysisData *data = ch->dataAtChunk(intChunk);
      err = data->correlation();
      vol = dB2Normalised(data->logrms(), ch->rmsCeiling, ch->rmsFloor);
      glLineWidth(lineWidth);
      if(gdata->pitchContourMode() == 0) {
        if(viewType == DRAW_VIEW_PRINT) {
          qglColor(colorBetween(QColor(255, 255, 255), ch->color, err*vol));
        } else {
          qglColor(colorBetween(gdata->backgroundColor(), ch->color, err*vol));
        }
      } else {
        qglColor(ch->color);
      }
      
      x = toInt(n);
      pitch = (ch->isVisibleChunk(data, gdata->ampThreshold(NOTE_SCORE,0))) ? data->pitch : 0.0f;
      myassert(pitch >= 0.0 && pitch <= gdata->topPitch());
      y = height() - 1 - toInt(pitch / zoomY) + viewBottomOffset;
      if(pitch > 0.0f) {
        if(fabs(prevPitch - pitch) < 1.0 && n != start) { //if closer than one semi-tone from previous then draw a line between them
          mygl_line((float)penX, (float)penY, (float)x, (float)y);
          penX = x; penY = y;
        } else {
          penX = x; penY = y;
        }
        if(stepSize > 10) { //draw squares on the data points
          mygl_rect(x - halfSquareSize, y - halfSquareSize, squareSize, squareSize);
        }
      }
      prevPitch = pitch;
    }
  }
}

void FreqWidgetGL::drawChannelFilledGL(Channel *ch, double leftTime, double currentTime, double zoomX, double viewBottom, double zoomY, int viewType)
{
  viewBottom += gdata->semitoneOffset();
  ZoomLookup *z;
  if(viewType == DRAW_VIEW_SUMMARY) z = &ch->summaryZoomLookup;
  else z = &ch->normalZoomLookup;
    
  ChannelLocker channelLocker(ch);

  QColor current = ch->color;
  qglColor(current);

  int viewBottomOffset = toInt(viewBottom / zoomY);
  viewBottom = double(viewBottomOffset) * zoomY;
  
  // baseX is the no. of chunks a pixel must represent.
  double baseX = zoomX / ch->timePerChunk();

  z->setZoomLevel(baseX);
  
  double currentChunk = ch->chunkFractionAtTime(currentTime);
  double leftFrameTime = currentChunk - ((currentTime - leftTime) / ch->timePerChunk());
  
  double frameTime = leftFrameTime;
  int n = 0;
  int baseElement = int(floor(frameTime / baseX));
  if(baseElement < 0) { n -= baseElement; baseElement = 0; }
  int lastBaseElement = int(floor(double(ch->totalChunks()) / baseX));
  
  int firstN = n;
  int lastN = firstN;
  
  Array1d<MyGLfloat2d> bottomPoints(width()*2);
  Array1d<MyGLfloat2d> evenMidPoints(width()*2);
  Array1d<MyGLfloat2d> oddMidPoints(width()*2);
  Array1d<MyGLfloat2d> evenMidPoints2(width()*2);
  Array1d<MyGLfloat2d> oddMidPoints2(width()*2);
  Array1d<MyGLfloat2d> noteRect(width()*2);
  Array1d<MyGLfloat2d> noteRect2(width()*2);
  std::vector<bool> isNoteRectEven(width()*2);
  int pointIndex = 0;
  int evenMidPointIndex = 0;
  int oddMidPointIndex = 0;
  int evenMidPointIndex2 = 0;
  int oddMidPointIndex2 = 0;
  int rectIndex = 0;
  int rectIndex2 = 0;

  if (baseX > 1) { // More samples than pixels
    int theWidth = width();
    if(lastBaseElement > z->size()) z->setSize(lastBaseElement);
    for(; n < theWidth && baseElement < lastBaseElement; n++, baseElement++) {
      myassert(baseElement >= 0);
      ZoomElement &ze = z->at(baseElement);
      if(!ze.isValid()) {
        if(!calcZoomElement(ch, ze, baseElement, baseX)) continue;
      }

      int y = height() - 1 - toInt(ze.high() / zoomY) + viewBottomOffset;
      int y2, y3;
      if(ze.noteIndex() != -1 && ch->dataAtChunk(ze.midChunk())->noteIndex != -1) {
        myassert(ze.noteIndex() >= 0);
        myassert(ze.noteIndex() < int(ch->noteData.size()));
        myassert(ch->isValidChunk(ze.midChunk()));
        AnalysisData *data = ch->dataAtChunk(ze.midChunk());

        if(gdata->showMeanVarianceBars()) {
          //longTermMean bars
          y2 = height() - 1 - toInt((data->longTermMean + data->longTermDeviation) / zoomY) + viewBottomOffset;
          y3 = height() - 1 - toInt((data->longTermMean - data->longTermDeviation) / zoomY) + viewBottomOffset;
          if(ze.noteIndex() % 2 == 0) {
            evenMidPoints[evenMidPointIndex++].set(n, y2);
            evenMidPoints[evenMidPointIndex++].set(n, y3);
          } else {
            oddMidPoints[oddMidPointIndex++].set(n, y2);
            oddMidPoints[oddMidPointIndex++].set(n, y3);
          }
  
          //shortTermMean bars
          y2 = height() - 1 - toInt((data->shortTermMean + data->shortTermDeviation) / zoomY) + viewBottomOffset;
          y3 = height() - 1 - toInt((data->shortTermMean - data->shortTermDeviation) / zoomY) + viewBottomOffset;
          if(ze.noteIndex() % 2 == 0) {
            evenMidPoints2[evenMidPointIndex2++].set(n, y2);
            evenMidPoints2[evenMidPointIndex2++].set(n, y3);
          } else {
            oddMidPoints2[oddMidPointIndex2++].set(n, y2);
            oddMidPoints2[oddMidPointIndex2++].set(n, y3);
          }
        }
      }
      bottomPoints[pointIndex++].set(n, y);
      bottomPoints[pointIndex++].set(n, height());
      lastN = n;
    }
    qglColor(gdata->shading1Color());
    mygl_rect(firstN, 0, lastN, height());
    qglColor(gdata->shading2Color());
    if(pointIndex > 1) {
      glVertexPointer(2, GL_FLOAT, 0, bottomPoints.begin());
      glDrawArrays(GL_QUAD_STRIP, 0, pointIndex);
    }

    if(gdata->showMeanVarianceBars()) {
      qglColor(Qt::green);
      if(evenMidPointIndex2 > 1) {
        glVertexPointer(2, GL_FLOAT, 0, evenMidPoints2.begin());
        glDrawArrays(GL_LINES, 0, evenMidPointIndex2);
      }
      qglColor(Qt::yellow);
      if(oddMidPointIndex2 > 1) {
        glVertexPointer(2, GL_FLOAT, 0, oddMidPoints2.begin());
        glDrawArrays(GL_LINES, 0, oddMidPointIndex2);
      }

      //longTermMean bars
      qglColor(Qt::yellow);
      if(evenMidPointIndex > 1) {
        glVertexPointer(2, GL_FLOAT, 0, evenMidPoints.begin());
        glDrawArrays(GL_LINES, 0, evenMidPointIndex);
      }
      qglColor(Qt::green);
      if(oddMidPointIndex > 1) {
        glVertexPointer(2, GL_FLOAT, 0, oddMidPoints.begin());
        glDrawArrays(GL_LINES, 0, oddMidPointIndex);
      }
    }

  } else { // More pixels than samples
    float err = 0.0, pitch = 0.0, prevPitch = 0.0;
    int intChunk = (int) floor(frameTime); // Integer version of frame time
    if(intChunk < 0) intChunk = 0;
    double stepSize = 1.0 / baseX; // So we skip some pixels
    int x = 0, y, y2, y3, x2;
  
    double start = (double(intChunk) - frameTime) * stepSize;
    double stop = width() + (2 * stepSize);
    lastN = firstN = toInt(start);
    for (double n = start; n < stop && intChunk < (int)ch->totalChunks(); n += stepSize, intChunk++) {
      myassert(intChunk >= 0);
      AnalysisData *data = ch->dataAtChunk(intChunk);
      err = data->correlation();

      if(gdata->pitchContourMode() == 0)
       qglColor(colorBetween(QColor(255, 255, 255), ch->color, err*dB2ViewVal(data->logrms())));
      else
        qglColor(ch->color);

      x = toInt(n);
      lastN = x;
      pitch = (ch->isVisibleChunk(data, gdata->ampThreshold(NOTE_SCORE,0))) ? data->pitch : 0.0f;

      if(data->noteIndex >= 0) {
        isNoteRectEven[rectIndex] = (data->noteIndex % 2) == 0;

        if(gdata->showMeanVarianceBars()) {
          x2 = toInt(n+stepSize);
          y2 = height() - 1 - toInt((data->longTermMean + data->longTermDeviation) / zoomY) + viewBottomOffset;
          y3 = height() - 1 - toInt((data->longTermMean - data->longTermDeviation) / zoomY) + viewBottomOffset;
          noteRect[rectIndex++].set(x, y2);
          noteRect[rectIndex++].set(x2, y3);
  
          //shortTermMean bars
          y2 = height() - 1 - toInt((data->shortTermMean + data->shortTermDeviation) / zoomY) + viewBottomOffset;
          y3 = height() - 1 - toInt((data->shortTermMean - data->shortTermDeviation) / zoomY) + viewBottomOffset;
          noteRect2[rectIndex2++].set(x, y2);
          noteRect2[rectIndex2++].set(x2, y3);
        }
      }

      myassert(pitch >= 0.0 && pitch <= gdata->topPitch());
      y = height() - 1 - toInt(pitch / zoomY) + viewBottomOffset;
      bottomPoints[pointIndex++].set(x, y);
      bottomPoints[pointIndex++].set(x, height());
      prevPitch = pitch;
    }

    myassert(pointIndex <= width()*2);
    qglColor(gdata->shading1Color());
    mygl_rect(firstN, 0, lastN, height());
    qglColor(gdata->shading2Color());
    glVertexPointer(2, GL_FLOAT, 0, bottomPoints.begin());
    glDrawArrays(GL_QUAD_STRIP, 0, pointIndex);

    if(gdata->showMeanVarianceBars()) {
      //shortTermMean bars
      for(int j=0; j<rectIndex2; j+=2) {
        if(isNoteRectEven[j]) qglColor(Qt::green);
        else qglColor(Qt::yellow);
        mygl_rect(noteRect2[j], noteRect2[j+1]);
      }
      //longTermMean bars
      QColor seeThroughYellow = Qt::yellow;
      seeThroughYellow.setAlpha(255);
      QColor seeThroughGreen = Qt::green;
      seeThroughGreen.setAlpha(255);
      for(int j=0; j<rectIndex; j+=2) {
        if(isNoteRectEven[j]) qglColor(seeThroughYellow);
        else qglColor(seeThroughGreen);
        mygl_rect(noteRect[j], noteRect[j+1]);
      }
    }
  }
}

/** calculates elements in the zoom lookup table
  @param ch The channel we are working with
  @param baseElement The element's index in the zoom lookup table
  @param baseX  The number of chunks each pixel represents (can include a fraction part)
  @return false if a zoomElement can't be calculated, else true
*/
bool FreqWidgetGL::calcZoomElement(Channel *ch, ZoomElement &ze, int baseElement, double baseX)
{
  int startChunk = toInt(double(baseElement) * baseX);
  int finishChunk = toInt(double(baseElement+1) * baseX) + 1;
  if(finishChunk >= (int)ch->totalChunks()) finishChunk--; //dont go off the end
  if(finishChunk >= (int)ch->totalChunks()) return false; //that data doesn't exist yet
  
  std::pair<std::deque<AnalysisData>::iterator, std::deque<AnalysisData>::iterator> a =
    minMaxElement(ch->dataIteratorAtChunk(startChunk), ch->dataIteratorAtChunk(finishChunk), lessPitch());
  if(a.second == ch->dataIteratorAtChunk(finishChunk)) return false;
  
  std::deque<AnalysisData>::iterator err = std::max_element(ch->dataIteratorAtChunk(startChunk), ch->dataIteratorAtChunk(finishChunk), lessValue(0));
  if(err == ch->dataIteratorAtChunk(finishChunk)) return false;
  
  float low, high;
  int noteIndex;
  if(ch->isVisibleChunk(&*err, gdata->ampThreshold(NOTE_SCORE,0))) {
    low = a.first->pitch;
    high = a.second->pitch;
    noteIndex = a.first->noteIndex;
  } else {
    low = 0;
    high = 0;
    noteIndex = NO_NOTE;
  }
  float corr = err->correlation()*dB2Normalised(err->logrms(), ch->rmsCeiling, ch->rmsFloor);
  QColor theColor = (gdata->pitchContourMode() == 0) ? colorBetween(gdata->backgroundColor(), ch->color, corr) : ch->color;

  ze.set(low, high, corr, theColor, noteIndex, (startChunk+finishChunk)/2);
  return true;
}

void FreqWidgetGL::setChannelVerticalView(Channel *ch, double leftTime, double currentTime, double zoomX, double viewBottom, double zoomY)
{
  ZoomLookup *z = &ch->normalZoomLookup;
    
  ChannelLocker channelLocker(ch);

  int viewBottomOffset = toInt(viewBottom / zoomY);
  viewBottom = double(viewBottomOffset) * zoomY;

  std::vector<float> ys;
  ys.reserve(width());
  std::vector<float> weightings;
  weightings.reserve(width());
  float maxY = 0.0f, minY = gdata->topPitch();
  float totalY = 0.0f;
  float numY = 0.0f;
  
  // baseX is the no. of chunks a pixel must represent.
  double baseX = zoomX / ch->timePerChunk();

  z->setZoomLevel(baseX);
  
  double currentChunk = ch->chunkFractionAtTime(currentTime);
  double leftFrameTime = currentChunk - ((currentTime - leftTime) / ch->timePerChunk());
  
  double frameTime = leftFrameTime;
  int n = 0;
  int currentBaseElement = int(floor(currentChunk / baseX));
  int firstBaseElement = int(floor(frameTime / baseX));
  int baseElement = firstBaseElement;
  if(baseElement < 0) { n -= baseElement; baseElement = 0; }
  int lastBaseElement = int(floor(double(ch->totalChunks()) / baseX));
  double leftBaseWidth = MAX(1.0, double(currentBaseElement - firstBaseElement));
  double rightBaseWidth = MAX(1.0, double(lastBaseElement - currentBaseElement));
  
/*
  We calculate the auto follow and scale by averaging all the note elements in view.
  We weight the frequency averaging by a triangle weighting function centered on the current time.
  Also it is weighted by the corr value of each frame.
             /|\
            / | \
           /  |  \
          /   |   \
            ^   ^
 leftBaseWidth rightBaseWidth
*/

  int firstN = n;
  int lastN = firstN;
  
  if (baseX > 1) { // More samples than pixels
    int theWidth = width();
    if(lastBaseElement > z->size()) z->setSize(lastBaseElement);
    for(; n < theWidth && baseElement < lastBaseElement; n++, baseElement++) {
      myassert(baseElement >= 0);
      ZoomElement &ze = z->at(baseElement);
      if(!ze.isValid()) {
        if(!calcZoomElement(ch, ze, baseElement, baseX)) continue;
      }
      if(ze.low() > 0.0f && ze.high() > 0.0f) {
        float weight = ze.corr();
        if(baseElement < currentBaseElement) weight *= double(currentBaseElement - baseElement) / leftBaseWidth;
        else if(baseElement > currentBaseElement) weight *= double(baseElement - currentBaseElement) / rightBaseWidth;
        if(ze.low() < minY) minY = ze.low();
        if(ze.high() > maxY) maxY = ze.high();
        totalY += (ze.low() + ze.high()) / 2.0f * weight;
        numY += weight;
        ys.push_back((ze.low() + ze.high()) / 2.0f);
        weightings.push_back(weight);
      }
      lastN = n;
    }
  } else { // More pixels than samples
    float err = 0.0, pitch = 0.0, prevPitch = 0.0;
    int intChunk = (int) floor(frameTime); // Integer version of frame time
    if(intChunk < 0) intChunk = 0;
    double stepSize = 1.0 / baseX; // So we skip some pixels
    int x = 0, y;
    float corr;
    
    double start = (double(intChunk) - frameTime) * stepSize;
    double stop = width() + (2 * stepSize);
    lastN = firstN = toInt(start);
    for (double n = start; n < stop && intChunk < (int)ch->totalChunks(); n += stepSize, intChunk++) {
      myassert(intChunk >= 0);
      AnalysisData *data = ch->dataAtChunk(intChunk);
      err = data->correlation();
      
      x = toInt(n);
      lastN = x;
      pitch = (ch->isVisibleChunk(data, gdata->ampThreshold(NOTE_SCORE,0))) ? data->pitch : 0.0f;
      myassert(pitch >= 0.0 && pitch <= gdata->topPitch());
      corr = data->correlation()*dB2ViewVal(data->logrms());
      if(pitch > 0.0f) {
        float weight = corr;
        if(minY < pitch) minY = pitch;
        if(maxY > pitch) maxY = pitch;
        totalY += pitch * weight;
        numY += weight;
        ys.push_back(pitch);
        weightings.push_back(weight);
      }
      y = height() - 1 - toInt(pitch / zoomY) + viewBottomOffset;
      prevPitch = pitch;
    }
  }
  
  if(!ys.empty() > 0) {
    float meanY = totalY / numY;
    double spred = 0.0;
    myassert(ys.size() == weightings.size());
    //use a linear spred function. not a squared one like standard deviation
    for(uint j=0; j<ys.size(); j++) {
      spred += sq(ys[j] - meanY) * weightings[j];
    }
    spred = sqrt(spred / numY) * 4.0;
    if(spred < 12.0) spred = 12.0; //show a minimum of 12 semi-tones
    gdata->view->setViewBottomRaw(meanY - gdata->view->viewHeight() / 2.0);
  }
}
