#pragma once
#include <string>
#include <vector>
#include <fstream>
#include "math3d.h"

enum Shape { SHAPE_CUBE = 0, SHAPE_SPHERE, SHAPE_CYLINDER, SHAPE_PLANE, SHAPE_MODEL, SHAPE_LIGHT, SHAPE_COUNT };
enum Behavior { BHV_NONE = 0, BHV_SPIN, BHV_BOB };

static const char* kShapeName[SHAPE_COUNT] = {"Cubo", "Esfera", "Cilindro", "Plano", "Modelo", "Luz"};

// Ambiente / iluminacao / realismo
struct Env {
    float sunYaw = 40.0f, sunPitch = -50.0f;   // graus (pitch negativo = sol apontando para baixo)
    float sunColor[3] = {1.0f, 0.95f, 0.85f};
    float sunIntensity = 3.0f;
    float ambient = 0.8f;
    float skyTop[3] = {0.22f, 0.42f, 0.80f};
    float skyHorizon[3] = {0.70f, 0.82f, 0.95f};
    float ground[3] = {0.28f, 0.27f, 0.25f};
    float fog = 0.0f;
    float exposure = 1.0f;
    int tonemap = 1;
    int shadows = 1;
    int shadowSize = 1024;
    float shadowSoft = 1.5f;
    float shadowRange = 25.0f;
    std::string music;            // musica de fundo (arquivo em audio/)
    float musicVolume = 0.6f;
    bool musicLoop = true;
    float musicFade = 1.0f;
};

enum Phys { PHYS_NONE = 0, PHYS_STATIC, PHYS_DYNAMIC };

// Som de um objeto: tipo de gatilho 0 Ao iniciar, 1 Ao colidir, 2 Apos tempo, 3 Ao tocar
struct SoundSlot {
    std::string clip;
    int trigger = 0;
    float volume = 1.0f, pitch = 1.0f, pitchVar = 0.0f, volVar = 0.0f;
    float delay = 0.0f, fadeIn = 0.0f, minDist = 1.0f, maxDist = 25.0f;
    bool loop = false, spatial = true;
    int bus = 1;   // 0 musica, 1 efeitos, 2 ambiente, 3 voz
};
static const char* kBusKey[4] = {"music", "sfx", "ambient", "voice"};
static const char* kBusLabel[4] = {"Música", "Efeitos", "Ambiente", "Voz"};

// Gatilho de animacao: tipo 0 Inicio, 1 Colisao, 2 Tempo (apos o Play), 3 Toque
struct AnimTrigger {
    int type = 0;
    std::string clip;
    float delay = 0;
    bool loop = false;
    float speed = 1;
};
struct PendAnim {
    float delay = 0;
    std::string clip;
    bool loop = false;
    float speed = 1;
};

struct Object {
    int uid = 0;
    std::string name = "Objeto";
    int shape = SHAPE_CUBE;
    std::string model;     // arquivo .obj (SHAPE_MODEL), na pasta models/
    std::string texture;   // imagem na pasta textures/
    Vec3 pos;
    Vec3 rot;              // graus (X, Y, Z)
    Vec3 scl = Vec3(1, 1, 1);
    float color[3] = {0.8f, 0.8f, 0.8f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float emissive[3] = {0, 0, 0};
    float emissiveStrength = 0.0f;
    float tile[2] = {1, 1};
    int behavior = BHV_NONE;
    bool castShadow = true;
    bool visible = true;
    float intensity = 15.0f;  // luz
    float range = 12.0f;      // luz

    // fisica
    int physics = PHYS_NONE;
    float mass = 1.0f, bounce = 0.2f, friction = 0.5f;
    bool useGravity = true;
    // animacao (modelos glTF/GLB)
    bool fileMaterial = true;          // usar material do arquivo
    std::string animDefault;           // animacao padrao (em loop)
    float animSpeedDef = 1.0f;
    bool animReturn = true;            // voltar a padrao apos animacao de evento
    float animBlendDef = 0.2f;         // tempo de transicao (s)
    std::vector<AnimTrigger> triggers;
    std::vector<SoundSlot> sounds;

    // estado de execucao (nao salvo)
    Vec3 vel;
    bool grounded = false;
    std::string animCur, animPrev;
    float animT = 0, animPrevT = 0, animSpeed = 1, animBlend = 0, animBlendT = 0;
    bool animLoop = true, animPlaying = false;
    std::vector<PendAnim> pend;

    Mat4 matrix() const { return translateM(pos) * rotationM(rot) * scaleM(scl); }
};

static inline std::string encTok(const std::string& s) {
    if (s.empty()) return "\x02";
    std::string o = s;
    for (char& c : o) if (c == ' ' || c == '\t' || c == '\n' || c == '\r') c = '\x01';
    return o;
}
static inline std::string decTok(const std::string& s) {
    if (s == "\x02") return "";
    std::string o = s;
    for (char& c : o) if (c == '\x01') c = ' ';
    return o;
}

struct Scene {
    std::vector<Object> objects;
    Env env;
    int nextId = 1;    // contador para nomes
    int nextUid = 1;   // id unico

    int add(const Object& o) {
        Object c = o;
        c.uid = nextUid++;
        objects.push_back(c);
        return c.uid;
    }

    bool save(const std::string& path) const {
        std::ofstream f(path);
        if (!f) return false;
        f.precision(7);
        f << "H3D 4\n";
        const Env& e = env;
        f << "env " << e.sunYaw << ' ' << e.sunPitch << ' '
          << e.sunColor[0] << ' ' << e.sunColor[1] << ' ' << e.sunColor[2] << ' ' << e.sunIntensity << ' '
          << e.ambient << ' '
          << e.skyTop[0] << ' ' << e.skyTop[1] << ' ' << e.skyTop[2] << ' '
          << e.skyHorizon[0] << ' ' << e.skyHorizon[1] << ' ' << e.skyHorizon[2] << ' '
          << e.ground[0] << ' ' << e.ground[1] << ' ' << e.ground[2] << ' '
          << e.fog << ' ' << e.exposure << ' ' << e.tonemap << ' ' << e.shadows << ' '
          << e.shadowSize << ' ' << e.shadowSoft << ' ' << e.shadowRange << ' '
          << encTok(e.music) << ' ' << e.musicVolume << ' ' << (e.musicLoop ? 1 : 0) << ' ' << e.musicFade << '\n';
        for (const Object& o : objects) {
            f << "obj " << encTok(o.name) << ' ' << o.shape << ' ' << encTok(o.model) << ' ' << encTok(o.texture) << ' '
              << o.pos.x << ' ' << o.pos.y << ' ' << o.pos.z << ' '
              << o.rot.x << ' ' << o.rot.y << ' ' << o.rot.z << ' '
              << o.scl.x << ' ' << o.scl.y << ' ' << o.scl.z << ' '
              << o.color[0] << ' ' << o.color[1] << ' ' << o.color[2] << ' '
              << o.metallic << ' ' << o.roughness << ' '
              << o.emissive[0] << ' ' << o.emissive[1] << ' ' << o.emissive[2] << ' ' << o.emissiveStrength << ' '
              << o.tile[0] << ' ' << o.tile[1] << ' '
              << o.behavior << ' ' << (o.castShadow ? 1 : 0) << ' ' << (o.visible ? 1 : 0) << ' '
              << o.intensity << ' ' << o.range << ' '
              << o.physics << ' ' << o.mass << ' ' << o.bounce << ' ' << o.friction << ' ' << (o.useGravity ? 1 : 0) << ' '
              << (o.fileMaterial ? 1 : 0) << ' ' << encTok(o.animDefault) << ' ' << o.animSpeedDef << ' '
              << (o.animReturn ? 1 : 0) << ' ' << o.animBlendDef << ' ' << o.triggers.size();
            for (const AnimTrigger& t : o.triggers)
                f << ' ' << t.type << ' ' << encTok(t.clip) << ' ' << t.delay << ' ' << (t.loop ? 1 : 0) << ' ' << t.speed;
            f << ' ' << o.sounds.size();
            for (const SoundSlot& sl : o.sounds)
                f << ' ' << encTok(sl.clip) << ' ' << sl.trigger << ' ' << sl.volume << ' ' << sl.pitch << ' ' << sl.pitchVar << ' '
                  << sl.volVar << ' ' << sl.delay << ' ' << sl.fadeIn << ' ' << sl.minDist << ' ' << sl.maxDist << ' '
                  << (sl.loop ? 1 : 0) << ' ' << (sl.spatial ? 1 : 0) << ' ' << sl.bus;
            f << '\n';
        }
        return (bool)f;
    }

    bool load(const std::string& path) {
        std::ifstream f(path);
        if (!f) return false;
        std::string magic;
        int ver = 0;
        f >> magic >> ver;
        if (magic != "H3D" || ver < 2 || ver > 4) return false;
        Env e;
        std::vector<Object> out;
        std::string tag;
        while (f >> tag) {
            if (tag == "env") {
                f >> e.sunYaw >> e.sunPitch >> e.sunColor[0] >> e.sunColor[1] >> e.sunColor[2] >> e.sunIntensity
                  >> e.ambient >> e.skyTop[0] >> e.skyTop[1] >> e.skyTop[2]
                  >> e.skyHorizon[0] >> e.skyHorizon[1] >> e.skyHorizon[2]
                  >> e.ground[0] >> e.ground[1] >> e.ground[2]
                  >> e.fog >> e.exposure >> e.tonemap >> e.shadows >> e.shadowSize >> e.shadowSoft >> e.shadowRange;
                if (ver >= 4) {
                    std::string mu;
                    int ml = 1;
                    f >> mu >> e.musicVolume >> ml >> e.musicFade;
                    e.music = decTok(mu);
                    e.musicLoop = ml != 0;
                }
            } else if (tag == "obj") {
                Object o;
                std::string n, m, t;
                int cs = 1, vis = 1;
                f >> n >> o.shape >> m >> t
                  >> o.pos.x >> o.pos.y >> o.pos.z >> o.rot.x >> o.rot.y >> o.rot.z
                  >> o.scl.x >> o.scl.y >> o.scl.z >> o.color[0] >> o.color[1] >> o.color[2]
                  >> o.metallic >> o.roughness >> o.emissive[0] >> o.emissive[1] >> o.emissive[2] >> o.emissiveStrength
                  >> o.tile[0] >> o.tile[1] >> o.behavior >> cs >> vis >> o.intensity >> o.range;
                if (!f) break;
                if (ver >= 3) {
                    std::string ad;
                    int ug = 1, fm = 1, ar = 1;
                    size_t ntr = 0;
                    f >> o.physics >> o.mass >> o.bounce >> o.friction >> ug >> fm >> ad >> o.animSpeedDef >> ar >> o.animBlendDef >> ntr;
                    if (!f) break;
                    o.useGravity = ug != 0; o.fileMaterial = fm != 0; o.animReturn = ar != 0;
                    o.animDefault = decTok(ad);
                    for (size_t k = 0; k < ntr && k < 64; k++) {
                        AnimTrigger t;
                        std::string c;
                        int lp = 0;
                        f >> t.type >> c >> t.delay >> lp >> t.speed;
                        t.clip = decTok(c);
                        t.loop = lp != 0;
                        o.triggers.push_back(t);
                    }
                    if (ver >= 4) {
                        size_t ns = 0;
                        f >> ns;
                        for (size_t k = 0; k < ns && k < 64; k++) {
                            SoundSlot sl;
                            std::string c;
                            int lp = 0, sp = 1;
                            f >> c >> sl.trigger >> sl.volume >> sl.pitch >> sl.pitchVar >> sl.volVar >> sl.delay >> sl.fadeIn
                              >> sl.minDist >> sl.maxDist >> lp >> sp >> sl.bus;
                            sl.clip = decTok(c);
                            sl.loop = lp != 0;
                            sl.spatial = sp != 0;
                            o.sounds.push_back(sl);
                        }
                    }
                }
                o.name = decTok(n);
                o.model = decTok(m);
                o.texture = decTok(t);
                o.castShadow = cs != 0;
                o.visible = vis != 0;
                if (o.shape < 0 || o.shape >= SHAPE_COUNT) o.shape = SHAPE_CUBE;
                o.uid = (int)out.size() + 1;
                out.push_back(o);
            } else {
                std::string skip;
                std::getline(f, skip);
            }
        }
        objects = out;
        env = e;
        nextUid = (int)objects.size() + 1;
        nextId = (int)objects.size() + 1;
        return true;
    }
};
