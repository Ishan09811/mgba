#include "oboe_audio_player.h"
#include <android/log.h>
#include <algorithm>

#define LOG_TAG "oboe_audio_player"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

bool OboeAudioPlayer::start(int32_t sampleRateHz, size_t requestedRingBufferFrames) {
    if (stream != nullptr) {
        LOGI("start() called but a stream already exists; ignoring");
        return true;
    }

    sampleRate = sampleRateHz;
    ringBufferFrames = requestedRingBufferFrames;
    ringBuffer = std::make_unique<SpscRingBuffer>(ringBufferFrames);
    playbackStarted.store(false, std::memory_order_release);

    oboe::AudioStreamBuilder builder;
    oboe::Result result = builder.setDirection(oboe::Direction::Output)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setSharingMode(oboe::SharingMode::Exclusive)
            ->setUsage(oboe::Usage::Game)
            ->setSampleRate(sampleRate)
            ->setChannelCount(oboe::ChannelCount::Stereo)
            ->setFormat(oboe::AudioFormat::I16)
            ->setDataCallback(this)
            ->setErrorCallback(this)
            ->openStream(stream);

    if (result != oboe::Result::OK) {
        LOGE("Failed to open Oboe stream: %s", oboe::convertToText(result));
        stream = nullptr;
        ringBuffer = nullptr;
        return false;
    }

    LOGI("Oboe stream opened: sampleRate=%d, backend=%s, performanceMode=%s, sharingMode=%s, framesPerBurst=%d",
         stream->getSampleRate(),
         oboe::convertToText(stream->getAudioApi()),
         oboe::convertToText(stream->getPerformanceMode()),
         oboe::convertToText(stream->getSharingMode()),
         stream->getFramesPerBurst());

    LOGI("Oboe stream opened; waiting for prebuffer=%zu frames (ring capacity=%zu)", kPrebufferFrames, ringBuffer->capacityFrames());

    return true;
}

void OboeAudioPlayer::stop() {
    playbackStarted.store(false, std::memory_order_release);
    if (stream == nullptr) {
        ringBuffer = nullptr;
        return;
    }

    stream->stop();
    stream->close();
    stream = nullptr;
    ringBuffer = nullptr;
}

size_t OboeAudioPlayer::write(const int16_t* samples, size_t frameCount) {
    if (!ringBuffer || !samples || frameCount == 0) return 0;

    const size_t written = ringBuffer->write(samples, frameCount);

    if (written < frameCount) {
        LOGD("audio ring short write: read=%zu written=%zu queued=%zu/%zu",
             frameCount,
             written,
             ringBuffer->available(),
             ringBuffer->capacityFrames()
		);
        return written;
    }

    if (stream && !playbackStarted.load(std::memory_order_acquire) &&
        ringBuffer->available() >= kPrebufferFrames) {
        bool expected = false;
        if (playbackStarted.compare_exchange_strong(
                expected,
		        true,
                std::memory_order_acq_rel,
                std::memory_order_acquire)
		) {
            const oboe::Result result = stream->requestStart();
            if (result == oboe::Result::OK) {
                LOGI("Starting Oboe playback after prebuffer: queued=%zu/%zu",
                     ringBuffer->available(), ringBuffer->capacityFrames()
				);
            } else {
                playbackStarted.store(false, std::memory_order_release);
                LOGE("Failed to start Oboe stream after prebuffer: %s",
                     oboe::convertToText(result)
				);
            }
        }
    }

    return written;
}

size_t OboeAudioPlayer::freeFrames() const {
    return ringBuffer ? ringBuffer->freeFrames() : 0;
}

size_t OboeAudioPlayer::capacityFrames() const {
    return ringBuffer ? ringBuffer->capacityFrames() : 0;
}

oboe::DataCallbackResult OboeAudioPlayer::onAudioReady(
        oboe::AudioStream*,
        void* audioData,
        int32_t numFrames
) {
    auto* out = static_cast<int16_t*>(audioData);

    if (ringBuffer) {
        ringBuffer->read(out, static_cast<size_t>(numFrames));
        if (isMuted.load(std::memory_order_relaxed)) {
            std::fill(out, out + numFrames * 2, static_cast<int16_t>(0));
        }
    } else {
        std::fill(out, out + numFrames * 2, static_cast<int16_t>(0));
    }

    static thread_local int callbackCount = 0;
    if (++callbackCount >= 200) {
        callbackCount = 0;
		LOGD(
		    "audio stats: queued=%zu/%zu "
		    "overflow=%zu underrun=%zu xrun=%d",
		    ringBuffer ? ringBuffer->available() : 0,
		    capacityFrames(),
		    ringBuffer ? ringBuffer->getOverflowFrames() : 0,
		    ringBuffer ? ringBuffer->getUnderrunFrames() : 0,
		    getStreamXRunCount()
		);
    }

    return oboe::DataCallbackResult::Continue;
}

void OboeAudioPlayer::onErrorAfterClose(
        oboe::AudioStream* /*audioStream*/,
        oboe::Result error
) {
    LOGE("Oboe stream closed after error: %s - attempting to reopen", oboe::convertToText(error));

    const int32_t savedSampleRate = sampleRate;
    const size_t savedRingBufferFrames = ringBufferFrames;

    stream = nullptr;
    ringBuffer = nullptr;
    playbackStarted.store(false, std::memory_order_release);

    if (savedSampleRate > 0 && savedRingBufferFrames > 0) {
        start(savedSampleRate, savedRingBufferFrames);
    }
}

int32_t OboeAudioPlayer::getStreamXRunCount() const {
    if (!stream) return 0;
    auto result = stream->getXRunCount();
    return result ? result.value() : 0;
}
