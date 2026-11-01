#pragma once
// Audio (wav, mp3, ogg, flac) sobre o miniaudio. A implementacao fica em audio.cpp / audio_impl.c.
#include <string>
#include <vector>

struct AudioProps {
    float volume = 1.0f, pitch = 1.0f, pan = 0.0f;
    bool loop = false, spatial = false;
    float x = 0, y = 0, z = 0;
    float minDist = 1.0f, maxDist = 25.0f, rolloff = 1.0f;
    float fadeIn = 0.0f, delay = 0.0f;
    std::string bus = "sfx";   // music, sfx, ambient, voice
};

class AudioSystem {
public:
    AudioSystem();
    ~AudioSystem();
    bool init();
    void shutdown();
    bool ok() const;
    void setPaused(bool paused);

    int play(const std::string& path, const AudioProps& p);   // devolve handle (>0) ou 0
    void stop(int id, float fadeOut = 0.0f);
    void pause(int id);
    void resume(int id);
    void setVolume(int id, float v);
    void setPitch(int id, float v);
    void setPan(int id, float v);
    void setPosition(int id, float x, float y, float z);
    bool isPlaying(int id);
    void stopAll();

    void setListener(float px, float py, float pz, float fx, float fy, float fz, float ux, float uy, float uz);
    void setMasterVolume(float v);
    void setBusVolume(const std::string& bus, float v);

    // remove sons terminados; ids dos que acabaram sozinhos vao em 'finished'
    void update(std::vector<int>& finished);

private:
    struct Impl;
    Impl* p;
    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;
};
