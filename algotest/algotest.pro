TEMPLATE = app
TARGET = algotest
CONFIG -= qt
DEPENDPATH += . 
INCLUDEPATH += ../include ../general ../sound ../sound/filters ../global
QMAKE_CXXFLAGS = -Wextra -Wunused -pedantic -Werror

HEADERS += 
SOURCES += main.cpp ../general/mytransforms.cpp ../general/fast_smooth.cpp \
  ../general/bspline.cpp ../global/conversions.cpp ../general/useful.cpp \
  ../sound/analysisdata.cpp ../general/channelbase.cpp ../sound/notedata.cpp \
  ../general/prony.cpp ../sound/filters/FastSmoothedAveragingFilter.cpp \
  ../sound/filters/GrowingAveragingFilter.cpp ../general/mymatrix.cpp \
  ../general/modes.cpp

# testapp
SOURCES += mychannel.cpp

LIBS += -lfftw3f
CONFIG += debug_and_release staticlib
