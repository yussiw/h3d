#include "audio.h"
#include "miniaudio.h"
#include <android/log.h>
#include <algorithm>
#include <chrono>
#include <map>
#include <memory>

#define ALOG(...) __android_log_print(ANDROID_LOG_INFO, "h3d-audio", __VA_ARGS__)

static double nowSec() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

struct Snd {
    ma_sound snd;
    bool loop = false;
    double killAt = 0;   // se > 0: remover nesse instante (fade de saida)
};

struct AudioSystem::Impl {
    ma_engine engine;
    ma_sound_group groups[4];
    bool inited = false;
    bool paused = false;
    std::map<int, std::unique_ptr<Snd>> sounds;
    int nextId = 1;
};

static int busIndex(const std::string& b) {
    if (b == "music") return 0;
    if (b == "ambient") return 2;
    if (b == "voice") return 3;
    return 1;
}

AudioSystem::AudioSystem() : p(new Impl()) {}
AudioSystem::~AudioSystem() { shutdown(); delete p; }

bool AudioSystem::ok() const { return p->inited; }

bool AudioSystem::init() {
    if (p->inited) return true;
    ma_engine_config cfg = ma_engine_config_init();
    if (ma_engine_init(&cfg, &p->engine) != MA_SUCCESS) { ALOG("falha ao iniciar o motor de audio"); return false; }
    for (int i = 0; i < 4; i++) ma_sound_group_init(&p->engine, 0, nullptr, &p->groups[i]);
    p->inited = true;
    return true;
}

void AudioSystem::shutdown() {
    if (!p->inited) return;
    stopAll();
    for (int i = 0; i < 4; i++) ma_sound_group_uninit(&p->groups[i]);
    ma_engine_uninit(&p->engine);
    p->inited = false;
}

void AudioSystem::setPaused(bool paused) {
    if (!p->inited || p->paused == paused) return;
    p->paused = paused;
    if (paused) ma_engine_stop(&p->engine); else ma_engine_start(&p->engine);
}

int AudioSystem::play(const std::string& path, const AudioProps& pr) {
    if (!p->inited) return 0;
    int bus = busIndex(pr.bus);
    ma_uint32 flags = 0;
    if (!pr.spatial) flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;
    flags |= (bus == 0) ? MA_SOUND_FLAG_STREAM : MA_SOUND_FLAG_DECODE;
    std::unique_ptr<Snd> s(new Snd());
    ma_result r = ma_sound_init_from_file(&p->engine, path.c_str(), flags, &p->groups[bus], nullptr, &s->snd);
    if (r != MA_SUCCESS) { ALOG("nao foi possivel abrir %s (erro %d)", path.c_str(), (int)r); return 0; }
    s->loop = pr.loop;
    ma_sound_set_volume(&s->snd, std::max(0.0f, pr.volume));
    ma_sound_set_pitch(&s->snd, std::max(0.05f, pr.pitch));
    if (pr.spatial) {
        ma_sound_set_position(&s->snd, pr.x, pr.y, pr.z);
        ma_sound_set_attenuation_model(&s->snd, ma_attenuation_model_inverse);
        ma_sound_set_min_distance(&s->snd, std::max(0.01f, pr.minDist));
        ma_sound_set_max_distance(&s->snd, std::max(pr.minDist + 0.01f, pr.maxDist));
        ma_sound_set_rolloff(&s->snd, pr.rolloff);
    } else {
        ma_sound_set_pan(&s->snd, std::max(-1.0f, std::min(1.0f, pr.pan)));
    }
    ma_sound_set_looping(&s->snd, pr.loop ? MA_TRUE : MA_FALSE);
    if (pr.fadeIn > 0) ma_sound_set_fade_in_milliseconds(&s->snd, 0.0f, 1.0f, (ma_uint64)(pr.fadeIn * 1000.0f));
    if (pr.delay > 0)
        ma_sound_set_start_time_in_milliseconds(&s->snd, ma_engine_get_time_in_milliseconds(&p->engine) + (ma_uint64)(pr.delay * 1000.0f));
    ma_sound_start(&s->snd);
    int id = p->nextId++;
    p->sounds[id] = std::move(s);
    return id;
}

void AudioSystem::stop(int id, float fadeOut) {
    auto it = p->sounds.find(id);
    if (it == p->sounds.end()) return;
    Snd* s = it->second.get();
    if (fadeOut > 0) {
        ma_sound_stop_with_fade_in_milliseconds(&s->snd, (ma_uint64)(fadeOut * 1000.0f));
        s->killAt = nowSec() + fadeOut + 0.05;
    } else {
        ma_sound_uninit(&s->snd);
        p->sounds.erase(it);
    }
}

void AudioSystem::pause(int id) { auto it = p->sounds.find(id); if (it != p->sounds.end()) ma_sound_stop(&it->second->snd); }
void AudioSystem::resume(int id) { auto it = p->sounds.find(id); if (it != p->sounds.end()) ma_sound_start(&it->second->snd); }
void AudioSystem::setVolume(int id, float v) { auto it = p->sounds.find(id); if (it != p->sounds.end()) ma_sound_set_volume(&it->second->snd, std::max(0.0f, v)); }
void AudioSystem::setPitch(int id, float v) { auto it = p->sounds.find(id); if (it != p->sounds.end()) ma_sound_set_pitch(&it->second->snd, std::max(0.05f, v)); }
void AudioSystem::setPan(int id, float v) { auto it = p->sounds.find(id); if (it != p->sounds.end()) ma_sound_set_pan(&it->second->snd, std::max(-1.0f, std::min(1.0f, v))); }
void AudioSystem::setPosition(int id, float x, float y, float z) { auto it = p->sounds.find(id); if (it != p->sounds.end()) ma_sound_set_position(&it->second->snd, x, y, z); }
bool AudioSystem::isPlaying(int id) {
    auto it = p->sounds.find(id);
    return it != p->sounds.end() && ma_sound_is_playing(&it->second->snd) == MA_TRUE;
}

void AudioSystem::stopAll() {
    for (auto& kv : p->sounds) ma_sound_uninit(&kv.second->snd);
    p->sounds.clear();
}

void AudioSystem::setListener(float px, float py, float pz, float fx, float fy, float fz, float ux, float uy, float uz) {
    if (!p->inited) return;
    ma_engine_listener_set_position(&p->engine, 0, px, py, pz);
    ma_engine_listener_set_direction(&p->engine, 0, fx, fy, fz);
    ma_engine_listener_set_world_up(&p->engine, 0, ux, uy, uz);
}

void AudioSystem::setMasterVolume(float v) { if (p->inited) ma_engine_set_volume(&p->engine, std::max(0.0f, v)); }
void AudioSystem::setBusVolume(const std::string& bus, float v) { if (p->inited) ma_sound_group_set_volume(&p->groups[busIndex(bus)], std::max(0.0f, v)); }

void AudioSystem::update(std::vector<int>& finished) {
    if (!p->inited) return;
    double t = nowSec();
    for (auto it = p->sounds.begin(); it != p->sounds.end();) {
        Snd* s = it->second.get();
        bool kill = s->killAt > 0 && t >= s->killAt;
        bool ended = !s->loop && s->killAt <= 0 && ma_sound_at_end(&s->snd) == MA_TRUE;
        if (kill || ended) {
            if (ended) finished.push_back(it->first);
            ma_sound_uninit(&s->snd);
            it = p->sounds.erase(it);
        } else ++it;
    }
}
