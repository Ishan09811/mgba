#pragma once

#include <oboe/Oboe.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <vector>

class SpscRingBuffer {
public:
    explicit SpscRingBuffer(size_t capacityFrames)
            : capacity(nextPowerOfTwo(capacityFrames)),
              mask(capacity - 1),
              buffer(capacity * kChannels) {}

    size_t write(const int16_t* src, size_t frameCount) {
        size_t w = writeIndex.load(std::memory_order_relaxed);
        size_t r = readIndex.load(std::memory_order_acquire);
        size_t freeFrames = capacity - (w - r);
        size_t toWrite = std::min(frameCount, freeFrames);

		if (toWrite < frameCount) {
			m_overflowFrames.fetch_add(
			    frameCount - toWrite,
			    std::memory_order_relaxed
			);
		}

        for (size_t i = 0; i < toWrite; ++i) {
            size_t idx = (w + i) & mask;
            buffer[idx * kChannels] = src[i * kChannels];
            buffer[idx * kChannels + 1] = src[i * kChannels + 1];
        }

        writeIndex.store(w + toWrite, std::memory_order_release);
        return toWrite;
    }

    size_t available() const {
        const size_t r = readIndex.load(std::memory_order_acquire);
        const size_t w = writeIndex.load(std::memory_order_acquire);
        return w - r;
    }

    size_t capacityFrames() const { return capacity; }

    size_t freeFrames() const {
        const size_t r = readIndex.load(std::memory_order_acquire);
        const size_t w = writeIndex.load(std::memory_order_acquire);
        return capacity - (w - r);
    }

    size_t read(int16_t* dst, size_t frameCount) {
        size_t r = readIndex.load(std::memory_order_relaxed);
        size_t w = writeIndex.load(std::memory_order_acquire);
        size_t availableFrames = w - r;
        size_t toRead = std::min(frameCount, availableFrames);

		if (toRead < frameCount) {
			m_underrunFrames.fetch_add(
			    frameCount - toRead,
			    std::memory_order_relaxed
			);

			std::memset(
			    dst + toRead * 2,
			    0,
			    (frameCount - toRead) *
			        2 *
			        sizeof(int16_t)
			);
		}


		for (size_t i = 0; i < toRead; ++i) {
            size_t idx = (r + i) & mask;
            dst[i * kChannels] = buffer[idx * kChannels];
            dst[i * kChannels + 1] = buffer[idx * kChannels + 1];
        }
        for (size_t i = toRead; i < frameCount; ++i) {
            dst[i * kChannels] = 0;
            dst[i * kChannels + 1] = 0;
        }

        readIndex.store(r + toRead, std::memory_order_release);
        return toRead;
    }

	size_t getOverflowFrames() const {
		return m_overflowFrames.load(std::memory_order_relaxed);
	}

	size_t getUnderrunFrames() const {
		return m_underrunFrames.load(std::memory_order_relaxed);
	}

private:
    static constexpr size_t kChannels = 2;

    static size_t nextPowerOfTwo(size_t v) {
        size_t p = 1;
        while (p < v) p <<= 1;
        return p;
    }

    const size_t capacity;
    const size_t mask;
    std::vector<int16_t> buffer;
    std::atomic<size_t> writeIndex{0};
    std::atomic<size_t> readIndex{0};
	std::atomic<size_t> m_overflowFrames{0};
	std::atomic<size_t> m_underrunFrames{0};
};

class OboeAudioPlayer : public oboe::AudioStreamDataCallback, public oboe::AudioStreamErrorCallback {
public:
    bool start(int32_t sampleRateHz, size_t ringBufferFrames);
    void stop();

    void setMuted(bool mute) { isMuted = mute; }

    size_t write(const int16_t* samples, size_t frameCount);
    size_t freeFrames() const;
    size_t capacityFrames() const;
    int32_t getStreamXRunCount() const;
    bool hasStream() const { return stream != nullptr; }

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream* stream, void* audioData, int32_t numFrames) override;
    void onErrorAfterClose(oboe::AudioStream* stream, oboe::Result error) override;

private:
    std::shared_ptr<oboe::AudioStream> stream;
    std::unique_ptr<SpscRingBuffer> ringBuffer;
    int32_t sampleRate = 0;
    size_t ringBufferFrames = 0;
    std::atomic<bool> isMuted{false};
    std::atomic<bool> playbackStarted{false};
    static constexpr size_t kPrebufferFrames = 1024;
};