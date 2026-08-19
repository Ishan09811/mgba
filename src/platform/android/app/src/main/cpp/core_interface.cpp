
#include "core_interface.h"
#include "audio/oboe_audio_player.h"
#include "no_intro_parser.h"

#include <algorithm>
#include <android/log.h>
#include <atomic>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
#include "mgba/gb/interface.h"
#include <mgba/core/core.h>
#include <mgba/core/version.h>
#include <mgba/core/serialize.h>
#include <mgba/core/interface.h>
#include <mgba-util/audio-buffer.h>
#include <mgba-util/audio-resampler.h>
#include <mgba/internal/gb/gb.h>
#include <mgba-util/vfs.h>
}

#define LOG_TAG "core"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

class CoreImpl : public CoreInterface {
public:
    CoreImpl() {
        s_audioRateOwner.store(this, std::memory_order_release);
    }

    ~CoreImpl() override {
        CoreImpl* expected = this;
        s_audioRateOwner.compare_exchange_strong(
		    expected, nullptr,
		    std::memory_order_acq_rel,
		    std::memory_order_acquire
		);
    }

    bool init() override {
        LOGI("Core::init");
        return true;
    }

    void shutdown() override {
        unloadRom();
        m_audioPlayer.stop();
    }

    bool quickLoadRom(const uint8_t* data, size_t size) override {
        if (m_core != nullptr) {
            unloadRom();
        }

		m_romBuffer.assign(data, data + size);

		VFile* vf = VFileFromConstMemory(m_romBuffer.data(), m_romBuffer.size());
        if (!vf) {
            LOGE("VFileFromConstMemory failed");
            return false;
        }

        m_core = mCoreFindVF(vf);
        if (!m_core) {
            LOGE("mCoreFindVF failed to identify ROM type");
            vf->close(vf);
            return false;
        }

        if (!m_core->init(m_core)) {
            LOGE("mCore init failed");
            m_core->deinit(m_core);
            m_core = nullptr;
            return false;
        }

        if (!m_core->loadROM(m_core, vf)) {
            LOGE("mCore loadROM failed");
            m_core->deinit(m_core);
            m_core = nullptr;
            return false;
        }

		m_gameTitle.clear();
		NoIntroMetadata metadata;

		const uint32_t crc32 = getRomCRC32();

		if (noIntroLookupCRC32(crc32, metadata) && !metadata.name.empty()) {
			m_gameTitle = metadata.name;

			LOGI(
			    "No-Intro match: title='%s' rom='%s' crc=%08X",
			    metadata.name.c_str(),
			    metadata.romName.c_str(),
			    metadata.crc32
			);
		} else {
			LOGI("No-Intro: no match for CRC32=%08X", crc32);
		}

        return true;
    }

    bool loadRom(const uint8_t* data, size_t size, bool skipBios, bool rtcEnable) override {
        if (m_core != nullptr) {
            unloadRom();
        }

        m_romBuffer.assign(data, data + size);
        VFile* vf = VFileFromConstMemory(m_romBuffer.data(), m_romBuffer.size());
        if (!vf) {
            LOGE("VFileFromConstMemory failed");
            return false;
        }

        m_core = mCoreFindVF(vf);
        if (!m_core) {
            LOGE("mCoreFindVF failed to identify ROM type");
            vf->close(vf);
            return false;
        }

        if (!m_core->init(m_core)) {
            LOGE("mCore init failed");
            m_core->deinit(m_core);
            m_core = nullptr;
            return false;
        }

        mCoreInitConfig(m_core, nullptr);

        mCoreConfigSetIntValue(&m_core->config, "skipBios", skipBios ? 1 : 0);
        mCoreConfigSetIntValue(&m_core->config, "hw.rtc", rtcEnable ? 1 : 0);

        LOGI("Config Applied -> Key: '%s' = %d", "skipBios", skipBios ? 1 : 0);
        LOGI("Config Applied -> Key: '%s' = %d", "hw.rtc", rtcEnable ? 1 : 0);

        unsigned width, height;
        m_core->baseVideoSize(m_core, &width, &height);
        m_width = static_cast<int>(width);
        m_height = static_cast<int>(height);

        m_videoBuffer.assign(static_cast<size_t>(m_width) * m_height, 0);

        m_core->setVideoBuffer(m_core, m_videoBuffer.data(), static_cast<size_t>(m_width));

        m_sampleRate = 0;

        m_avStream = {};
        m_avStream.audioRateChanged = &CoreImpl::audioRateChangedThunk;
        m_core->setAVStream(m_core, &m_avStream);

        if (!m_core->loadROM(m_core, vf)) {
            LOGE("mCore loadROM failed");
            m_core->deinit(m_core);
            m_core = nullptr;
            return false;
        }

        m_coreSampleRate = static_cast<int>(m_core->audioSampleRate(m_core));
        if (m_coreSampleRate <= 0) {
            LOGE("Invalid native audio sample rate: %d", m_coreSampleRate);
            m_core->deinit(m_core);
            m_core = nullptr;
            return false;
        }

        configureCoreAudioBuffer(m_coreSampleRate);

        if (m_audioPlayer.hasStream()) {
            m_audioPlayer.stop();
        }

        constexpr int kPreferredOutputRate = 48000;
        if (!m_audioPlayer.start(kPreferredOutputRate, kAudioRingBufferFrames)) {
            LOGE("Failed to start Oboe audio playback, continuing without audio");
        }

        m_sampleRate = m_audioPlayer.getSampleRate();
        if (m_sampleRate <= 0) {
            LOGE("Oboe returned invalid sample rate: %d", m_sampleRate);
            m_audioPlayer.stop();
            m_core->deinit(m_core);
            m_core = nullptr;
            return false;
        }

        mAudioBufferInit(&m_resampledAudio, kResampledAudioBufferFrames, 2);
        mAudioResamplerInit(&m_audioResampler, mINTERPOLATOR_SINC);
        mAudioResamplerSetSource(
		    &m_audioResampler,
		    m_core->getAudioBuffer(m_core),
		    static_cast<double>(m_coreSampleRate),
		    true
		);
        mAudioResamplerSetDestination(
		    &m_audioResampler,
		    &m_resampledAudio,
		    static_cast<double>(m_sampleRate)
		);

        m_resamplerInitialized = true;
        m_pendingAudioRate.store(0, std::memory_order_release);

        LOGI("ROM loaded: %dx%d coreAudioRate=%d outputRate=%d",
             m_width, m_height, m_coreSampleRate, m_sampleRate);

		m_core->reset(m_core);
        resetAudioPipeline();
        return true;
    }

    void unloadRom() override {
        m_audioPlayer.stop();

        if (m_resamplerInitialized) {
            mAudioResamplerDeinit(&m_audioResampler);
            mAudioBufferDeinit(&m_resampledAudio);
            m_resamplerInitialized = false;
        }

        if (m_core) {
            m_core->unloadROM(m_core);
            m_core->deinit(m_core);
            m_core = nullptr;
        }

        m_pendingAudioRate.store(0, std::memory_order_release);
        m_coreSampleRate = 0;
        m_sampleRate = 0;
        m_romBuffer.clear();
        m_videoBuffer.clear();
    }

    void reset() override {
        if (m_core) {
            m_pendingAudioRate.store(0, std::memory_order_release);
            m_core->reset(m_core);
            m_coreSampleRate = static_cast<int>(m_core->audioSampleRate(m_core));
            configureCoreAudioBuffer(m_coreSampleRate);
            if (m_resamplerInitialized) {
                resetAudioPipeline();
            }
        }
    }

    void runFrame() override {
        if (!m_core || !m_resamplerInitialized) return;

        m_core->runFrame(m_core);

        applyPendingAudioRateChange();

        for (size_t i = 0; i < kMaxAudioProcessIterations; ++i) {
            if (m_audioPlayer.freeFrames() == 0) break;

            const size_t sourceBefore = mAudioBufferAvailable(m_core->getAudioBuffer(m_core));
            const size_t outputBefore = mAudioBufferAvailable(&m_resampledAudio);
            const size_t produced = mAudioResamplerProcess(&m_audioResampler);

            drainResampledAudio();

            const size_t sourceAfter = mAudioBufferAvailable(m_core->getAudioBuffer(m_core));
            const size_t outputAfter = mAudioBufferAvailable(&m_resampledAudio);

            if (produced == 0 &&
                sourceBefore == sourceAfter &&
                outputBefore == outputAfter) {
                break;
            }
        }
    }

    const uint32_t* getVideoBuffer() override {
		return reinterpret_cast<const uint32_t*>(m_videoBuffer.data());
    }

    int getWidth() const override { return m_width; }
    int getHeight() const override { return m_height; }

    void setKeys(uint16_t keyMask) override {
        if (m_core) m_core->setKeys(m_core, static_cast<uint32_t>(keyMask));
    }

    bool saveState(uint8_t* outBuffer, size_t bufferSize, size_t* outWritten) override {
        if (!m_core) { *outWritten = 0; return false; }
        VFile* vf = VFileFromMemory(outBuffer, bufferSize);
        if (!vf) { *outWritten = 0; return false; }

        bool ok = mCoreSaveStateNamed(m_core, vf, SAVESTATE_SAVEDATA | SAVESTATE_SCREENSHOT);
        *outWritten = ok ? static_cast<size_t>(vf->seek(vf, 0, SEEK_CUR)) : 0;
        vf->close(vf);
        return ok;
    }

    bool loadState(const uint8_t* data, size_t size) override {
        if (!m_core) return false;
        VFile* vf = VFileFromConstMemory(data, size);
        if (!vf) return false;
        bool ok = mCoreLoadStateNamed(m_core, vf, SAVESTATE_SAVEDATA | SAVESTATE_SCREENSHOT);
        vf->close(vf);
        return ok;
    }

    bool loadSaveData(const uint8_t* data, size_t size) override {
        if (!m_core) return false;
        bool ok = m_core->savedataRestore(m_core, data, size, true);
        if (!ok) {
            LOGE("savedataRestore failed (size=%zu)", size);
        }
        return ok;
    }

    std::vector<uint8_t> exportSaveData() override {
        if (!m_core) return {};
        void* sram = nullptr;
        size_t size = m_core->savedataClone(m_core, &sram);
        if (size == 0 || sram == nullptr) {
            return {};
        }

        std::vector<uint8_t> result(static_cast<uint8_t*>(sram), static_cast<uint8_t*>(sram) + size);
        free(sram);
        return result;
    }

	std::string getGameTitle() override {
		return m_gameTitle;
	}

	std::string getGameCode() override {
		if (!m_core) return "";
		mGameInfo info{};
		m_core->getGameInfo(m_core, &info);
		info.code[4] = '\0';
		return {info.code};
	}

    int getPlatform() override {
        if (!m_core) return PLATFORM_UNKNOWN;

        int basePlatform = m_core->platform(m_core);

        if (basePlatform == mPLATFORM_GBA) {
            return PLATFORM_GBA;
        }

        if (basePlatform == mPLATFORM_GB) {
            auto* gbCore = reinterpret_cast<struct GB*>(m_core->board);
            if (!gbCore) return PLATFORM_GB;

            switch (gbCore->model) {
                case GB_MODEL_CGB:
                    return PLATFORM_GBC;
                case GB_MODEL_SGB:
                    return PLATFORM_SGB;
                default:
                    return PLATFORM_GB;
            }
        }

        return PLATFORM_UNKNOWN;
    }


    bool loadBios(const uint8_t* data, size_t size) override {
        if (!m_core) {
            LOGE("loadBios called before loadRom");
            return false;
        }

        VFile* vf = VFileFromConstMemory(data, size);
        if (!vf) {
            LOGE("VFileFromConstMemory failed for BIOS data");
            return false;
        }

        bool ok = m_core->loadBIOS(m_core, vf, 0);
        if (!ok) {
            LOGE("mCore loadBIOS rejected the file (size=%zu)", size);
            vf->close(vf);
        }
        return ok;
    }

    void setConfigInt(const char* key, int value) override {
        if (!m_core) return;
        mCoreConfigSetIntValue(&m_core->config, key, value);
    }

    void setConfigString(const char* key, const char* value) override {
        mCoreConfigSetValue(&m_core->config, key, value);
    }

    void setAudioMuted(bool mute) override {
        m_audioPlayer.setMuted(mute);
    }

private:
    static void audioRateChangedThunk(struct mAVStream*, unsigned rate) {
        CoreImpl* self = s_audioRateOwner.load(std::memory_order_acquire);
        if (self) {
            self->m_pendingAudioRate.store(static_cast<int>(rate), std::memory_order_release);
        }
    }

    void configureCoreAudioBuffer(int sourceRate) {
        if (!m_core || sourceRate <= 0) return;

        const double samplesPerFrame =
                static_cast<double>(sourceRate) *
                static_cast<double>(m_core->frameCycles(m_core)) /
                static_cast<double>(m_core->frequency(m_core));

        auto bufferFrames = static_cast<size_t>(std::ceil(samplesPerFrame * 2.0));
        bufferFrames = std::max<size_t>(bufferFrames, 2);
        bufferFrames = std::min<size_t>(bufferFrames, kMaxCoreAudioBufferFrames);
        m_core->setAudioBufferSize(m_core, bufferFrames);

        LOGI("Core audio buffer: sourceRate=%d samplesPerFrame=%.6f frames=%zu",
             sourceRate, samplesPerFrame, bufferFrames);
    }

    void applyPendingAudioRateChange() {
        if (!m_resamplerInitialized || !m_core) return;

        const int newRate = m_pendingAudioRate.exchange(0, std::memory_order_acq_rel);
        if (newRate <= 0 || newRate == m_coreSampleRate) return;

        const int oldRate = m_coreSampleRate;
        m_coreSampleRate = newRate;
        configureCoreAudioBuffer(newRate);
        resetAudioPipeline();

        LOGI("native audio rate changed: %d -> %d, outputRate=%d", oldRate, newRate, m_sampleRate);
    }

    void resetAudioPipeline() {
        if (!m_core || !m_resamplerInitialized) return;

        mAudioBufferClear(&m_resampledAudio);
        mAudioResamplerDeinit(&m_audioResampler);
        mAudioResamplerInit(&m_audioResampler, mINTERPOLATOR_SINC);
        mAudioResamplerSetSource(
		    &m_audioResampler,
		    m_core->getAudioBuffer(m_core),
		    static_cast<double>(m_coreSampleRate),
		    true
		);

        mAudioResamplerSetDestination(
		    &m_audioResampler,
		    &m_resampledAudio,
		    static_cast<double>(m_sampleRate)
		);
    }

    void drainResampledAudio() {
        if (!m_resamplerInitialized) return;

        const size_t available = mAudioBufferAvailable(&m_resampledAudio);
        const size_t freeFrames = m_audioPlayer.freeFrames();
        const size_t frames = std::min({
		    available,
		    freeFrames,
		    kAudioDrainFrames
        });

        if (frames == 0) return;

        int16_t audioBuf[kAudioDrainFrames * 2];
        const size_t read = mAudioBufferRead(&m_resampledAudio, audioBuf, frames);
        if (read == 0) return;

        const size_t written = m_audioPlayer.write(audioBuf, read);
        if (written != read) {
            LOGE("audio output mismatch: resampled=%zu written=%zu", read, written);
        }
    }

    static constexpr size_t kMaxCoreAudioBufferFrames = 0x4000;
    static constexpr size_t kResampledAudioBufferFrames = 1024;
    static constexpr size_t kAudioDrainFrames = 1024;
    static constexpr size_t kMaxAudioProcessIterations = 2;
    static constexpr size_t kAudioRingBufferFrames = 4096;


	uint32_t getRomCRC32() const {
		if (!m_core) {
			return 0;
		}

		uint32_t crc32 = 0;
		m_core->checksum(
		    m_core,
		    &crc32,
		    mCHECKSUM_CRC32
		);

		return crc32;
	}

    enum Platform {
        PLATFORM_UNKNOWN = -1,
        PLATFORM_GB = 1,
        PLATFORM_GBC = 2,
        PLATFORM_SGB = 3,
        PLATFORM_GBA = 4
    };

    mCore* m_core = nullptr;
    OboeAudioPlayer m_audioPlayer;
    std::vector<uint8_t> m_romBuffer;
    std::vector<uint32_t> m_videoBuffer;
    // only populated and used when color_t is 16-bit; empty and unused otherwise.
    std::vector<uint32_t> m_expandedBuffer;
    int m_width = 0;
    int m_height = 0;
    int m_coreSampleRate = 0;
    int m_sampleRate = 0;
    mAudioResampler m_audioResampler{};
    mAudioBuffer m_resampledAudio{};
    mAVStream m_avStream{};
    std::atomic<int> m_pendingAudioRate{0};
    bool m_resamplerInitialized = false;

    static std::atomic<CoreImpl*> s_audioRateOwner;

	std::string m_gameTitle;
};

std::atomic<CoreImpl*> CoreImpl::s_audioRateOwner{nullptr};

CoreInterface* createCore() {
    return new CoreImpl();
}
