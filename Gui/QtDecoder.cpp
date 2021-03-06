//  Natron
//
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 * contact: immarespond at gmail dot com
 *
 */

#include "QtDecoder.h"

#include <stdexcept>

CLANG_DIAG_OFF(deprecated)
CLANG_DIAG_OFF(uninitialized)
#include <QtGui/QImage>
#include <QtGui/QColor>
#include <QtGui/QImageReader>
CLANG_DIAG_ON(deprecated)
CLANG_DIAG_ON(uninitialized)

#include "Engine/AppManager.h"
#include "Engine/Image.h"
#include "Engine/Lut.h"
#include "Engine/KnobTypes.h"
#include "Engine/KnobFile.h"
#include "Engine/Knob.h"
#include "Engine/Project.h"
#include "Engine/Node.h"

using namespace Natron;
using std::cout; using std::endl;

QtReader::QtReader(boost::shared_ptr<Natron::Node> node)
    : Natron::EffectInstance(node)
      , _lut( Color::LutManager::sRGBLut() )
      , _img(0)
      , _fileKnob()
      , _firstFrame()
      , _before()
      , _lastFrame()
      , _after()
      , _missingFrameChoice()
      , _frameMode()
      , _startingFrame()
      , _timeOffset()
      , _settingFrameRange(false)
{
}

QtReader::~QtReader()
{
    if (_img) {
        delete _img;
    }
}

std::string
QtReader::getPluginID() const
{
    return NATRON_ORGANIZATION_DOMAIN_TOPLEVEL "." NATRON_ORGANIZATION_DOMAIN_SUB ".built-in.ReadQt";;
}

std::string
QtReader::getPluginLabel() const
{
    return "ReadQt";
}

void
QtReader::getPluginGrouping(std::list<std::string>* grouping) const
{
    grouping->push_back(PLUGIN_GROUP_IMAGE);
}

std::string
QtReader::getDescription() const
{
    return QObject::tr("A QImage (Qt) based image reader.").toStdString();
}

void
QtReader::initializeKnobs()
{
    Natron::warningDialog( getName(), QObject::tr("This plugin exists only to help the developers team to test %1"
                                                  ". You cannot use it when rendering a project.").arg(NATRON_APPLICATION_NAME).toStdString() );


    _fileKnob = getNode()->createKnob<File_Knob>( QObject::tr("File").toStdString() );
    _fileKnob->setAsInputImage();

    _firstFrame =  getNode()->createKnob<Int_Knob>( QObject::tr("First frame").toStdString() );
    _firstFrame->setAnimationEnabled(false);
    _firstFrame->setDefaultValue(0,0);


    _before = getNode()->createKnob<Choice_Knob>( QObject::tr("Before").toStdString() );
    std::vector<std::string> beforeOptions;
    beforeOptions.push_back( QObject::tr("hold").toStdString() );
    beforeOptions.push_back( QObject::tr("loop").toStdString() );
    beforeOptions.push_back( QObject::tr("bounce").toStdString() );
    beforeOptions.push_back( QObject::tr("black").toStdString() );
    beforeOptions.push_back( QObject::tr("error").toStdString() );
    _before->populateChoices(beforeOptions);
    _before->setAnimationEnabled(false);
    _before->setDefaultValue(0,0);

    _lastFrame =  getNode()->createKnob<Int_Knob>( QObject::tr("Last frame").toStdString() );
    _lastFrame->setAnimationEnabled(false);
    _lastFrame->setDefaultValue(0,0);


    _after = getNode()->createKnob<Choice_Knob>( QObject::tr("After").toStdString() );
    std::vector<std::string> afterOptions;
    afterOptions.push_back( QObject::tr("hold").toStdString() );
    afterOptions.push_back( QObject::tr("loop").toStdString() );
    afterOptions.push_back( QObject::tr("bounce").toStdString() );
    afterOptions.push_back( QObject::tr("black").toStdString() );
    afterOptions.push_back( QObject::tr("error").toStdString() );
    _after->populateChoices(beforeOptions);
    _after->setAnimationEnabled(false);
    _after->setDefaultValue(0,0);

    _missingFrameChoice = getNode()->createKnob<Choice_Knob>( QObject::tr("On missing frame").toStdString() );
    std::vector<std::string> missingFrameOptions;
    missingFrameOptions.push_back( QObject::tr("Load nearest").toStdString() );
    missingFrameOptions.push_back( QObject::tr("Error").toStdString() );
    missingFrameOptions.push_back( QObject::tr("Black image").toStdString() );
    _missingFrameChoice->populateChoices(missingFrameOptions);
    _missingFrameChoice->setDefaultValue(0,0);
    _missingFrameChoice->setAnimationEnabled(false);

    _frameMode = getNode()->createKnob<Choice_Knob>( QObject::tr("Frame mode").toStdString() );
    _frameMode->setAnimationEnabled(false);
    std::vector<std::string> frameModeOptions;
    frameModeOptions.push_back( QObject::tr("Starting frame").toStdString() );
    frameModeOptions.push_back( QObject::tr("Time offset").toStdString() );
    _frameMode->populateChoices(frameModeOptions);
    _frameMode->setDefaultValue(0,0);

    _startingFrame = getNode()->createKnob<Int_Knob>( QObject::tr("Starting frame").toStdString() );
    _startingFrame->setAnimationEnabled(false);
    _startingFrame->setDefaultValue(0,0);

    _timeOffset = getNode()->createKnob<Int_Knob>( QObject::tr("Time offset").toStdString() );
    _timeOffset->setAnimationEnabled(false);
    _timeOffset->setDefaultValue(0,0);
    _timeOffset->setSecret(true);
} // initializeKnobs

void
QtReader::knobChanged(KnobI* k,
                      Natron::ValueChangedReasonEnum /*reason*/,
                      int /*view*/,
                      SequenceTime /*time*/,
                      bool /*originatedFromMainThread*/)
{
    if ( k == _fileKnob.get() ) {
        SequenceTime first,last;
        getSequenceTimeDomain(first,last);
        timeDomainFromSequenceTimeDomain(first,last, true);
        _startingFrame->setValue(first,0);
    } else if ( ( k == _firstFrame.get() ) && !_settingFrameRange ) {
        int first = _firstFrame->getValue();
        _lastFrame->setMinimum(first);

        int offset = _timeOffset->getValue();
        _settingFrameRange = true;
        _startingFrame->setValue(first + offset,0);
        _settingFrameRange = false;
    } else if ( k == _lastFrame.get() ) {
        int last = _lastFrame->getValue();
        _firstFrame->setMaximum(last);
    } else if ( k == _frameMode.get() ) {
        int mode = _frameMode->getValue();
        switch (mode) {
        case 0:     //starting frame
            _startingFrame->setSecret(false);
            _timeOffset->setSecret(true);
            break;
        case 1:     //time offset
            _startingFrame->setSecret(true);
            _timeOffset->setSecret(false);
            break;
        default:
            //no such case
            assert(false);
            break;
        }
    } else if ( ( k == _startingFrame.get() ) && !_settingFrameRange ) {
        //also update the time offset
        int startingFrame = _startingFrame->getValue();
        int firstFrame = _firstFrame->getValue();


        ///prevent recursive calls of setValue(...)
        _settingFrameRange = true;
        _timeOffset->setValue(startingFrame - firstFrame,0);
        _settingFrameRange = false;
    } else if ( ( k == _timeOffset.get() ) && !_settingFrameRange ) {
        //also update the starting frame
        int offset = _timeOffset->getValue();
        int first = _firstFrame->getValue();


        ///prevent recursive calls of setValue(...)
        _settingFrameRange = true;
        _startingFrame->setValue(offset + first,0);
        _settingFrameRange = false;
    }
} // knobChanged

void
QtReader::getSequenceTimeDomain(SequenceTime & first,
                                SequenceTime & last)
{
    first = _fileKnob->firstFrame();
    last = _fileKnob->lastFrame();
}

void
QtReader::timeDomainFromSequenceTimeDomain(SequenceTime & first,
                                           SequenceTime & last
                                           ,
                                           bool mustSetFrameRange)
{
    ///the values held by GUI parameters
    int frameRangeFirst,frameRangeLast;
    int startingFrame;

    if (mustSetFrameRange) {
        frameRangeFirst = first;
        frameRangeLast = last;
        startingFrame = first;
        _settingFrameRange = true;
        _firstFrame->setMinimum(first);
        _firstFrame->setMaximum(last);
        _lastFrame->setMinimum(first);
        _lastFrame->setMaximum(last);

        _firstFrame->setValue(first,0);
        _lastFrame->setValue(last,0);
        _settingFrameRange = false;
    } else {
        ///these are the value held by the "First frame" and "Last frame" param
        frameRangeFirst = _firstFrame->getValue();
        frameRangeLast = _lastFrame->getValue();
        startingFrame = _startingFrame->getValue();
    }

    first = startingFrame;
    last =  startingFrame + frameRangeLast - frameRangeFirst;
}

void
QtReader::getFrameRange(SequenceTime *first,
                        SequenceTime *last)
{
    getSequenceTimeDomain(*first, *last);
    timeDomainFromSequenceTimeDomain(*first, *last, false);
}

void
QtReader::supportedFileFormats_static(std::vector<std::string>* formats)
{
    const QList<QByteArray> & supported = QImageReader::supportedImageFormats();

    for (int i = 0; i < supported.size(); ++i) {
        formats->push_back( supported.at(i).data() );
    }
};
std::vector<std::string>
QtReader::supportedFileFormats() const
{
    std::vector<std::string> ret;
    QtReader::supportedFileFormats_static(&ret);

    return ret;
}

SequenceTime
QtReader::getSequenceTime(SequenceTime t)
{
    int timeOffset = _timeOffset->getValue();
    SequenceTime first = _firstFrame->getValue();
    SequenceTime last = _lastFrame->getValue();


    ///offset the time wrt the starting time
    SequenceTime sequenceTime =  t - timeOffset;

    ///get the offset from the starting time of the sequence in case we bounce or loop
    int timeOffsetFromStart = sequenceTime -  first;

    if (sequenceTime < first) {
        /////if we're before the first frame
        int beforeChoice = _before->getValue();
        switch (beforeChoice) {
        case 0:     //hold
            sequenceTime = first;
            break;
        case 1:     //loop
                    //call this function recursively with the appropriate offset in the time range
            timeOffsetFromStart %= (int)(last - first + 1);
            sequenceTime = last + timeOffsetFromStart;
            break;
        case 2:     //bounce
                    //call this function recursively with the appropriate offset in the time range
        {
            int sequenceIntervalsCount = (last == first) ? 0 : ( timeOffsetFromStart / (last - first) );
            ///if the sequenceIntervalsCount is odd then do exactly like loop, otherwise do the load the opposite frame
            if (sequenceIntervalsCount % 2 == 0) {
                timeOffsetFromStart %= (int)(last - first + 1);
                sequenceTime = first - timeOffsetFromStart;
            } else {
                timeOffsetFromStart %= (int)(last - first + 1);
                sequenceTime = last + timeOffsetFromStart;
            }
            break;
        }
        case 3:     //black
            throw std::invalid_argument("Out of frame range.");
            break;
        case 4:     //error
            setPersistentMessage( Natron::eMessageTypeError,  QObject::tr("Missing frame").toStdString() );
            throw std::invalid_argument("Out of frame range.");
            break;
        default:
            break;
        }
    } else if (sequenceTime > last) {
        /////if we're after the last frame
        int afterChoice = _after->getValue();

        switch (afterChoice) {
        case 0:     //hold
            sequenceTime = last;
            break;
        case 1:     //loop
                    //call this function recursively with the appropriate offset in the time range
            timeOffsetFromStart %= (int)(last - first + 1);
            sequenceTime = first + timeOffsetFromStart;
            break;
        case 2:     //bounce
                    //call this function recursively with the appropriate offset in the time range
        {
            int sequenceIntervalsCount = (last == first) ? 0 : ( timeOffsetFromStart / (last - first) );
            ///if the sequenceIntervalsCount is odd then do exactly like loop, otherwise do the load the opposite frame
            if (sequenceIntervalsCount % 2 == 0) {
                timeOffsetFromStart %= (int)(last - first + 1);
                sequenceTime = first + timeOffsetFromStart;
            } else {
                timeOffsetFromStart %= (int)(last - first + 1);
                sequenceTime = last - timeOffsetFromStart;
            }
            break;
        }
        case 3:     //black
            throw std::invalid_argument("Out of frame range.");
            break;
        case 4:     //error
            setPersistentMessage( Natron::eMessageTypeError, QObject::tr("Missing frame").toStdString() );
            throw std::invalid_argument("Out of frame range.");
            break;
        default:
            break;
        }
    }
    assert(sequenceTime >= first && sequenceTime <= last);

    return sequenceTime;
} // getSequenceTime

void
QtReader::getFilenameAtSequenceTime(SequenceTime time,
                                    std::string &filename)
{
    int missingChoice = _missingFrameChoice->getValue();

    filename = _fileKnob->getFileName(time, 0);

    switch (missingChoice) {
    case 0:     // Load nearest
                ///the nearest frame search went out of range and couldn't find a frame.
        if ( filename.empty() ) {
            filename = _fileKnob->getFileName(time, 0);
            if ( filename.empty() ) {
                setPersistentMessage( Natron::eMessageTypeError, QObject::tr("Nearest frame search went out of range").toStdString() );
            }
        }
        break;
    case 1:     // Error
                /// For images sequences, if the offset is not 0, that means no frame were found at the  originally given
                /// time, we can safely say this is  a missing frame.
        if ( filename.empty() ) {
            setPersistentMessage( Natron::eMessageTypeError, QObject::tr("Missing frame").toStdString() );
        }
    case 2:     // Black image
                /// For images sequences, if the offset is not 0, that means no frame were found at the  originally given
                /// time, we can safely say this is  a missing frame.

        break;
    }
}

Natron::StatusEnum
QtReader::getRegionOfDefinition(U64 /*hash*/,SequenceTime time,
                                const RenderScale & /*scale*/,
                                int /*view*/,
                                RectD* rod )
{
    QMutexLocker l(&_lock);
    double sequenceTime;

    try {
        sequenceTime =  getSequenceTime(time);
    } catch (const std::exception & e) {
        return eStatusFailed;
    }

    std::string filename;

    getFilenameAtSequenceTime(sequenceTime, filename);

    if ( filename.empty() ) {
        return eStatusFailed;
    }

    if (filename != _filename) {
        _filename = filename;
        if (_img) {
            delete _img;
        }
        _img = new QImage( _filename.c_str() );
        if (_img->format() == QImage::Format_Invalid) {
            setPersistentMessage(Natron::eMessageTypeError, QObject::tr("Failed to load the image ").toStdString() + filename);

            return eStatusFailed;
        }
    }

    rod->x1 = 0;
    rod->x2 = _img->width();
    rod->y1 = 0;
    rod->y2 = _img->height();

    return eStatusOK;
}

Natron::StatusEnum
QtReader::render(SequenceTime /*time*/,
                 const RenderScale& /*originalScale*/,
                 const RenderScale & /*mappedScale*/,
                 const RectI & roi,
                 int /*view*/,
                 bool /*isSequentialRender*/,
                 bool /*isRenderResponseToUserInteraction*/,
                 boost::shared_ptr<Natron::Image> output)
{
    int missingFrameChoice = _missingFrameChoice->getValue();

    if (!_img) {
        if ( !_img && (missingFrameChoice == 2) ) { // black image
            return eStatusOK;
        }
        assert(missingFrameChoice != 0); // nearest value - should never happen
        return eStatusFailed; // error
    }

    assert(_img);
    switch ( _img->format() ) {
    case QImage::Format_RGB32:     // The image is stored using a 32-bit RGB format (0xffRRGGBB).
    case QImage::Format_ARGB32:     // The image is stored using a 32-bit ARGB format (0xAARRGGBB).
        //might have to invert y coordinates here
        _lut->from_byte_packed( (float*)output->pixelAt(0, 0), _img->bits(), roi, output->getBounds(), output->getBounds(),
                                Natron::Color::PACKING_BGRA,Natron::Color::PACKING_RGBA,true,false );
        break;
    case QImage::Format_ARGB32_Premultiplied:     // The image is stored using a premultiplied 32-bit ARGB format (0xAARRGGBB).
        //might have to invert y coordinates here
        _lut->from_byte_packed( (float*)output->pixelAt(0, 0), _img->bits(), roi, output->getBounds(), output->getBounds(),
                                Natron::Color::PACKING_BGRA,Natron::Color::PACKING_RGBA,true,true );
        break;
    case QImage::Format_Mono:     // The image is stored using 1-bit per pixel. Bytes are packed with the most significant bit (MSB) first.
    case QImage::Format_MonoLSB:     // The image is stored using 1-bit per pixel. Bytes are packed with the less significant bit (LSB) first.
    case QImage::Format_Indexed8:     // The image is stored using 8-bit indexes into a colormap.
    case QImage::Format_RGB16:     // The image is stored using a 16-bit RGB format (5-6-5).
    case QImage::Format_RGB666:     // The image is stored using a 24-bit RGB format (6-6-6). The unused most significant bits is always zero.
    case QImage::Format_RGB555:     // The image is stored using a 16-bit RGB format (5-5-5). The unused most significant bit is always zero.
    case QImage::Format_RGB888:     // The image is stored using a 24-bit RGB format (8-8-8).
    case QImage::Format_RGB444:     // The image is stored using a 16-bit RGB format (4-4-4). The unused bits are always zero.
    {
        QImage img = _img->convertToFormat(QImage::Format_ARGB32);
        _lut->from_byte_packed( (float*)output->pixelAt(0, 0), img.bits(), roi, output->getBounds(), output->getBounds(),
                                Natron::Color::PACKING_BGRA, Natron::Color::PACKING_RGBA, true, false );
        break;
    }
    case QImage::Format_ARGB8565_Premultiplied:     // The image is stored using a premultiplied 24-bit ARGB format (8-5-6-5).
    case QImage::Format_ARGB6666_Premultiplied:     // The image is stored using a premultiplied 24-bit ARGB format (6-6-6-6).
    case QImage::Format_ARGB8555_Premultiplied:     // The image is stored using a premultiplied 24-bit ARGB format (8-5-5-5).
    case QImage::Format_ARGB4444_Premultiplied:     // The image is stored using a premultiplied 16-bit ARGB format (4-4-4-4).
    {
        QImage img = _img->convertToFormat(QImage::Format_ARGB32_Premultiplied);
        _lut->from_byte_packed( (float*)output->pixelAt(0, 0), img.bits(), roi, output->getBounds(), output->getBounds(),
                                Natron::Color::PACKING_BGRA, Natron::Color::PACKING_RGBA, true, true );
        break;
    }
    case QImage::Format_Invalid:
    default:
        output->fill(roi,0.f,1.f);
        setPersistentMessage( Natron::eMessageTypeError, QObject::tr("Invalid image format.").toStdString() );

        return eStatusFailed;
    }

    return eStatusOK;
} // render

void
QtReader::addAcceptedComponents(int /*inputNb*/,
                                std::list<Natron::ImageComponentsEnum>* comps)
{
    ///QtReader only supports RGBA for now.
    comps->push_back(Natron::eImageComponentRGBA);
}

void
QtReader::addSupportedBitDepth(std::list<Natron::ImageBitDepthEnum>* depths) const
{
    depths->push_back(eImageBitDepthFloat);
}

