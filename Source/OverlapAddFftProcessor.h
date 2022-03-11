/*
  ==============================================================================

    OverlapAddFftProcessor.h
    Created: 04 Mar 2022 10:07:00pm
    Author:  Deddy Welsan

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

using namespace juce;

/**
 This processor takes care of buffering input and output samples for your FFT processing.
 With fttSizeAsPowerOf2 and hopSizeDividerAsPowerOf2 the fftSize and hopSize can be specifiec.
 Inherit from this class and override the processFrameInBuffer() function in order to
 implement your processing. You can also override the `createWindow()` method to use
 another window (default: Hann window).
 @code
 class MyProcessor : public OverlappingFFTProcessor
 {
 public:
     MyProcessor () : OverlappingFFTProcessor (11, 2) {}
     ~MyProcessor() {}
 private:
     void processFrameInBuffer (const int maxNumChannels) override
     {
         for (int ch = 0; ch < maxNumChannels; ++ch)
            fft.performRealOnlyForwardTransform (fftInOutBuffer.getWritePointer (ch), true);
         // clear high frequency content
         for (int ch = 0; ch < maxNumChannels; ++ch)
            FloatVectorOperations::clear (fftInOutBuffer.getWritePointer (ch, fftSize / 2), fftSize / 2);
         for (int ch = 0; ch < maxNumChannels; ++ch)
            fft.performRealOnlyInverseTransform (fftInOutBuffer.getWritePointer (ch));
     }
 };
 */

class OverlapAddFftProcessor {
public:
    /** Constructor
     @param fftSizeAsPowerOf2 defines the fftSize as a power of 2: fftSize = 2^fftSizeAsPowerOf2
     @param hopSizeDividerAsPowerOf2 defines the hopSize as a fraction of fftSize: hopSize = fftSize / (2^hopSizeDivider)
     */
    OverlapAddFftProcessor(const int fftSizeAsPowerOf2, const int hopSizeDividerAsPowerOf2 = 1)
        : fft(fftSizeAsPowerOf2)
        , fftSize(1 << fftSizeAsPowerOf2)
        , hopSize(fftSize >> hopSizeDividerAsPowerOf2)
    {
        // make sure you have at least an overlap of 50%
        jassert(hopSizeDividerAsPowerOf2 > 0);

        // make sure you don't want to hop smaller than 1 sample
        jassert(hopSizeDividerAsPowerOf2 <= fftSizeAsPowerOf2);

        DBG("Overlapping FFT Processor created with fftSize: " << fftSize << " and hopSize: " << hopSize);

        // gInputBuffer.resize(gBufferSize);
        // gOutputBuffer.resize(gBufferSize);

        // create window
        window.resize(fftSize);
        createWindow();

        // don`t change the size of the window during createWindow()!
        jassert(window.size() == fftSize);
    }

    virtual ~OverlapAddFftProcessor() { }

    void reset() { }

    void prepare(const double sampleRate, const int maximumBlockSize, const int numInputChannels, const int numOutputChannels)
    {
		gOutputBufferWritePointer = fftSize + 2 * hopSize;

        // this->sampleRate = sampleRate;
        numInpChannel = numInputChannels;
        numOutChannel = numOutputChannels;

        // const int bufferSize = maximumBlockSize;

        inputAudioBufferLenght = fftSize * 2;
        // inpBufferWritePointer = 0;

        // notYetUsedAudioData.setSize (numInpChannel, fftSize);

        const auto maxCh = jmax(numInpChannel, numOutChannel);
        fftInOutBuffer.setSize(maxCh, fftSize);
		fftInOutBuffer.clear();

        // const int k = floor (1.0f + ((float) (bufferSize - 1)) / hopSize);
        // const int M = k * hopSize + (fftSize - hopSize);

        outputBuffer.setSize(numOutChannel, inputAudioBufferLenght);
        outputBuffer.clear();

        // int offset = 0;
        // while (offset < fftSize)
        //     offset += bufferSize;

        outputOffset = fftSize;

		const int gBufferSize = 16384;
        gInputBuffer.setSize(numInputChannels, gBufferSize);
		gOutputBuffer.setSize(numInputChannels, gBufferSize);
		gInputBuffer.clear();
		gOutputBuffer.clear();
    }

    void process(const dsp::ProcessContextReplacing<float>& context)
    {
        process(context.getInputBlock(), context.getOutputBlock());
    }

    void process(const dsp::ProcessContextNonReplacing<float>& context)
    {
        process(context.getInputBlock(), context.getOutputBlock());
    }

    void process(const dsp::AudioBlock<const float>& inputBlock, dsp::AudioBlock<float>& outputBlock)
    {
        const auto maxNumChannels = inputBlock.getNumChannels();
        const auto inputBlockLength = (int)inputBlock.getNumSamples();

		for (int ch = 0; ch < maxNumChannels; ++ch)
		{
			for (int i = 0; i < inputBlockLength; i++)
			{
				float in = inputBlock.getSample(ch, i);

				// Store the sample ("in") in a buffer for the FFT
				// Increment the pointer and when full window has been 
				// assembled, call process_fft()
				// gInputBuffer[gInputBufferPointer++] = in;
				gInputBuffer.setSample(ch, gInputBufferPointer, in);
				gInputBufferPointer++;
				if(gInputBufferPointer >= gBufferSize) {
					// Wrap the circular buffer
					// Notice: this is not the condition for starting a new FFT
					gInputBufferPointer = 0;
				}
				
				// Get the output sample from the output buffer
				// float out = gOutputBuffer[gOutputBufferReadPointer];
				float out = gOutputBuffer.getSample(ch, gOutputBufferReadPointer);
				
				
				// Then clear the output sample in the buffer so it is ready for the next overlap-add
				// gOutputBuffer[gOutputBufferReadPointer] = 0;
				gOutputBuffer.setSample(ch, gOutputBufferReadPointer, 0);
				
				// Scale the output down by the scale factor, compensating for the overlap
				out *= gScaleFactor;
					
				// Increment the read pointer in the output cicular buffer
				gOutputBufferReadPointer++;
				if(gOutputBufferReadPointer >= gBufferSize)
					gOutputBufferReadPointer = 0;
				
				// Increment the hop counter and start a new FFT if we've reached the hop size
				if(++gHopCounter >= hopSize) {
					gHopCounter = 0;
					
					gCachedInputBufferPointer = gInputBufferPointer;
					// Bela_scheduleAuxiliaryTask(gFftTask);
					// Copy buffer into FFT input
					for(int n = 0; n < fftSize; n++) {
						// Use modulo arithmetic to calculate the circular buffer index
						int circularBufferIndex = (gCachedInputBufferPointer + n - fftSize + gBufferSize) % gBufferSize;
						// unwrappedBuffer[n] = inBuffer[circularBufferIndex] * gAnalysisWindowBuffer[n];
						float sampleVal = gInputBuffer.getSample(ch, circularBufferIndex) * window.at(n);
						fftInOutBuffer.setSample(ch, n, sampleVal); 
					}
            		processFrameInBuffer(maxNumChannels);

					for(int n = 0; n < fftSize; n++) {
						int circularBufferIndex = (gOutputBufferWritePointer + n - fftSize + gBufferSize) % gBufferSize;
						// outBuffer[circularBufferIndex] += gFft.td(n) * gSynthesisWindowBuffer[n];
						float sampleVal = gOutputBuffer.getSample(ch, circularBufferIndex) + fftInOutBuffer.getSample(ch, n) * window.at(n);
						gOutputBuffer.setSample(ch, circularBufferIndex, sampleVal);
					}

					gOutputBufferWritePointer = (gOutputBufferWritePointer + hopSize) % gBufferSize;
				}
				outputBlock.setSample(ch, i, out);
			}
		}

        // if ((inpBufferWritePointer + inputBlockLength) <= inputAudioBufferLenght) {
        //     for (int ch = 0; ch < numInpChannel; ++ch) {
        //         FloatVectorOperations::copy(inputAudioBuffer.getWritePointer(ch, inpBufferWritePointer), // destination
        //             inputBlock.getChannelPointer(ch), // source
        //             inputBlockLength); // number of samples
        //     }
		// 	inpBufferWritePointer += inputBlockLength - 1;
		// 	notYetUsedAudioDataCount += inputBlockLength;
        // } 
		// else {
		// 	auto firstCopyLength = inputAudioBufferLenght - (inpBufferWritePointer + 1);
        //     for (int ch = 0; ch < numInpChannel; ++ch) {
        //         FloatVectorOperations::copy(inputAudioBuffer.getWritePointer(ch, inpBufferWritePointer), // destination
        //             inputBlock.getChannelPointer(ch), // source
        //             firstCopyLength); // number of samples
        //     }
		// 	inpBufferWritePointer = 0;
		// 	auto secondCopyLength = inputBlockLength - firstCopyLength;
        //     for (int ch = 0; ch < numInpChannel; ++ch) {
        //         FloatVectorOperations::copy(inputAudioBuffer.getWritePointer(ch, inpBufferWritePointer), // destination
        //             inputBlock.getChannelPointer(ch) + firstCopyLength - 1, // source
        //             secondCopyLength); // number of samples
        //     }
		// 	inpBufferWritePointer += secondCopyLength - 1;
		// 	notYetUsedAudioDataCount += inputBlockLength;
        // }

        // if (inpBufferWritePointer >= (inputAudioBufferLenght - 1)) {
        //     inpBufferWritePointer -= (inputAudioBufferLenght - 1);
        // }

        // while (notYetUsedAudioDataCount >= fftSize) {
        //     if ((inpBufferReadPointer + fftSize) <= inputAudioBufferLenght) {
		// 		for (int ch = 0; ch < numInpChannel; ++ch) {
		// 			FloatVectorOperations::copy(fftInOutBuffer.getWritePointer(ch, 0), // destination
		// 				inputAudioBuffer.getReadPointer(ch, inpBufferReadPointer), // source
		// 				fftSize); // number of samples
		// 		}
		// 		inpBufferReadPointer += fftSize - 1;
        //     }
		// 	else {
		// 		auto firstCopyLength = inputAudioBufferLenght - (inpBufferReadPointer + 1);
		// 		for (int ch = 0; ch < numInpChannel; ++ch) {
		// 			FloatVectorOperations::copy(fftInOutBuffer.getWritePointer(ch, 0), // destination
		// 				inputAudioBuffer.getReadPointer(ch, inpBufferReadPointer), // source
		// 				firstCopyLength); // number of samples
		// 		}
		// 		inpBufferReadPointer = 0;
		// 		auto secondCopyLength = fftSize - firstCopyLength;
		// 		for (int ch = 0; ch < numInpChannel; ++ch) {
		// 			FloatVectorOperations::copy(fftInOutBuffer.getWritePointer(ch, firstCopyLength - 1), // destination
		// 				inputAudioBuffer.getReadPointer(ch, inpBufferReadPointer), // source
		// 				secondCopyLength); // number of samples
		// 		}
		// 		inpBufferReadPointer += secondCopyLength - 1;
		// 	}

		// 	if (inpBufferReadPointer >= (inputAudioBufferLenght - 1)) {
		// 		inpBufferReadPointer -= (inputAudioBufferLenght - 1);
		// 	}

        //     notYetUsedAudioDataCount -= fftSize;
        //     processFrameInBuffer(maxNumChannels);

		// 	if ((outputOffset + fftSize) <= inputAudioBufferLenght) {
		// 		for (int ch = 0; ch < numInpChannel; ++ch) {
		// 			FloatVectorOperations::copy(outputBuffer.getWritePointer(ch, 0), // destination
		// 				fftInOutBuffer.getReadPointer(ch, 0), // source
		// 				fftSize); // number of samples
		// 		}
		// 		outputOffset += fftSize - 1;
        //     }
		// 	else {
		// 		auto firstCopyLength = inputAudioBufferLenght - (outputOffset + 1);
		// 		for (int ch = 0; ch < numInpChannel; ++ch) {
		// 			FloatVectorOperations::copy(outputBuffer.getWritePointer(ch, outputOffset), // destination
		// 				fftInOutBuffer.getReadPointer(ch, 0), // source
		// 				firstCopyLength); // number of samples
		// 		}
		// 		outputOffset = 0;
		// 		auto secondCopyLength = fftSize - firstCopyLength;
		// 		for (int ch = 0; ch < numInpChannel; ++ch) {
		// 			FloatVectorOperations::copy(outputBuffer.getWritePointer(ch, 0), // destination
		// 				fftInOutBuffer.getReadPointer(ch, firstCopyLength - 1), // source
		// 				secondCopyLength); // number of samples
		// 		}
		// 		outputOffset += secondCopyLength - 1;
		// 	}

		// 	if (outputOffset >= (inputAudioBufferLenght - 1)) {
		// 		outputOffset -= (inputAudioBufferLenght - 1);
		// 	}
        //     // writeBackFrame();
        // }

        /* if data stored in  */

        // const int initialNotYetUsedAudioDataCount = notYetUsedAudioDataCount;
        // int notYetUsedAudioDataOffset = 0;
        // int usedSamples = 0;

        // // we've got some left overs, so let's use them together with the new samples
        // while (notYetUsedAudioDataCount > 0 && notYetUsedAudioDataCount + inputBlockLength >= fftSize)
        // {
        //     // copy not yet used data into fftInOut buffer (with hann windowing)
        //     for (int ch = 0; ch < numChIn; ++ch)
        //     {
        //         FloatVectorOperations::multiply (fftInOutBuffer.getWritePointer (ch), // destination
        //                                          notYetUsedAudioData.getReadPointer (ch, notYetUsedAudioDataOffset), // source 1 (audio data)
        //                                          window.data(), // source 2 (window)
        //                                          notYetUsedAudioDataCount // number of samples
        //                                         );

        //         // fill up fftInOut buffer with new data (with hann windowing)
        //         FloatVectorOperations::multiply (fftInOutBuffer.getWritePointer (ch, notYetUsedAudioDataCount), // destination
        //                                          inputBlock.getChannelPointer (ch), // source 1 (audio data)
        //                                          window.data() + notYetUsedAudioDataCount, // source 2 (window)
        //                                          fftSize - notYetUsedAudioDataCount // number of samples
        //                                         );
        //     }

        //     // process frame and buffer output
        //     processFrameInBuffer (maxNumChannels);
        //     writeBackFrame();

        //     notYetUsedAudioDataOffset += hopSize;
        //     notYetUsedAudioDataCount -= hopSize;
        // }

        // if (notYetUsedAudioDataCount > 0) // not enough new input samples to use all of the previous data
        // {
        //     for (int ch = 0; ch < numChIn; ++ch)
        //     {
        //         FloatVectorOperations::copy (notYetUsedAudioData.getWritePointer (ch),		//destination
        //                                      notYetUsedAudioData.getReadPointer (ch, initialNotYetUsedAudioDataCount - notYetUsedAudioDataCount),	// source
        //                                      notYetUsedAudioDataCount);		// number of samples
        //         FloatVectorOperations::copy (notYetUsedAudioData.getWritePointer (ch, notYetUsedAudioDataCount),	//destination
        //                                      inputBlock.getChannelPointer (ch) + usedSamples,	// source
        //                                      inputBlockLength);		// number of samples
        //     }
        //     notYetUsedAudioDataCount += inputBlockLength;
        // }
        // else  // all of the previous data used
        // {
            ///int dataOffset = - notYetUsedAudioDataCount;
            // int dataOffset = 0;

            // while (inputBlockLength - dataOffset >= fftSize)
            // {
            //     for (int ch = 0; ch < numInpChannel; ++ch)
            //     {
            //         FloatVectorOperations::multiply (fftInOutBuffer.getWritePointer (ch),	// dest
            //                                          inputBlock.getChannelPointer (ch) + dataOffset,	// src1
            //                                          window.data(),		// src2
        	// 										 fftSize);		// number of samples
            //     }
            //     processFrameInBuffer (maxNumChannels);
            //     writeBackFrame();

            //     dataOffset += hopSize;
            // }

            // int remainingSamples = inputBlockLength - dataOffset;
            // if (remainingSamples > 0)
            // {
            //     for (int ch = 0; ch < numInpChannel; ++ch)
            //     {
            //         FloatVectorOperations::copy (notYetUsedAudioData.getWritePointer (ch),
            //                                      inputBlock.getChannelPointer (ch) + dataOffset,
            //                                      inputBlockLength - dataOffset);
            //     }
            // }
            // notYetUsedAudioDataCount = remainingSamples;
        // }

        // // return processed samples from outputBuffer
        // const int shiftStart = inputBlockLength;
        // int shiftL = outputOffset + fftSize - hopSize - inputBlockLength;

        // const int tooMuch = shiftStart + shiftL - outputBuffer.getNumSamples();
        // if (tooMuch > 0)
        //     shiftL -= tooMuch;

        // for (int ch = 0; ch < numOutChannel; ++ch) {
        //     FloatVectorOperations::copy (outputBlock.getChannelPointer (ch), outputBuffer.getReadPointer (ch), inputBlockLength);
        //     FloatVectorOperations::copy (outputBuffer.getWritePointer (ch), outputBuffer.getReadPointer (ch, shiftStart), shiftL);
        // }

        // for (int ch = numChOut; ch < outputBlock.getNumChannels(); ++ch)
        //     FloatVectorOperations::clear (outputBlock.getChannelPointer (ch), inputBlockLength);

        // outputOffset += inputAudioBufferLenght;
    }

private:
    virtual void createWindow()
    {
        dsp::WindowingFunction<float>::fillWindowingTables(window.data(), fftSize, dsp::WindowingFunction<float>::WindowingMethod::hann, false);

        const float hopSizeCompensateFactor = 1.0f / (fftSize / hopSize / 2);
        // for (auto& elem : window)
            // elem *= hopSizeCompensateFactor;
    }

    /**
     This method get's called each time the processor has gathered enough samples for a transformation.
     The data in the `fftInOutBuffer` is still in time domain. Use the `fft` member to transform it into
     frequency domain, do your calculations, and transform it back to time domain.
     @param maxNumChannels the max number of channels of `fftInOutBuffer` you should use
     */
    virtual void processFrameInBuffer(const int maxNumChannels) { }

    void writeBackFrame()
    {
        for (int ch = 0; ch < numOutChannel; ++ch) {
            outputBuffer.addFrom(ch, outputOffset, fftInOutBuffer, ch, 0, fftSize - hopSize);
            outputBuffer.copyFrom(ch, outputOffset + fftSize - hopSize, fftInOutBuffer, ch, fftSize - hopSize, hopSize);
        }
        outputOffset += hopSize;
    }

protected:
    dsp::FFT fft;
    const int fftSize;
    const int hopSize;
	float gScaleFactor = 0.5;
    const int gBufferSize = 16384;

    std::vector<float> window;
    // std::vector<float> gInputBuffer;
    // std::vector<float> gOutputBuffer;
    AudioBuffer<float> fftInOutBuffer;
    double sampleRate;

    // AudioBuffer<float> notYetUsedAudioData;

private:
    int numInpChannel;
    int numOutChannel;

    AudioBuffer<float> gInputBuffer;
    int gInputBufferPointer = 0;
	int gHopCounter = 0;

    AudioBuffer<float> gOutputBuffer;
	int gOutputBufferWritePointer = 0;
	int gOutputBufferReadPointer = 0;
	int gCachedInputBufferPointer = 0;

    int inpBufferWritePointer = 0;
    int inpBufferReadPointer = 0;
    int inputAudioBufferLenght = 0;
    int notYetUsedAudioDataCount = 0;

    AudioBuffer<float> outputBuffer;
    int outputOffset = fftSize;

    JUCE_DECLARE_NON_COPYABLE(OverlapAddFftProcessor)
};