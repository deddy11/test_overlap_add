/*
  ==============================================================================

    SpectralDynamicProcessor.h
    Created: 14 Dec 2021 10:36:10pm
    Author:  vivo10

  ==============================================================================
*/

#pragma once

#include "OverlapAddFftProcessor.h"

class SpectralDynamicProcessor : public OverlapAddFftProcessor {
public:
    SpectralDynamicProcessor()
        : OverlapAddFftProcessor(10, 3)
    {
    }
    ~SpectralDynamicProcessor() { }

private:
    void processFrameInBuffer(const int maxNumChannels) override
    {
        // for (int ch = 0; ch < maxNumChannels; ++ch)
            // fft.performRealOnlyForwardTransform(fftInOutBuffer.getWritePointer(ch), true);
        // for (int ch = 0; ch < maxNumChannels; ++ch)
        //     FloatVectorOperations::clear(fftInOutBuffer.getWritePointer(ch, fftSize / 2), fftSize / 2);
        // for (int ch = 0; ch < maxNumChannels; ++ch)
            // fft.performRealOnlyInverseTransform(fftInOutBuffer.getWritePointer(ch));
    }
};
