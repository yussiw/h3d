#pragma once
#include <string>
#include <vector>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <algorithm>
#include "imgui.h"
#include "math3d.h"
#include "scene.h"
#include "fsutil.h"
#include "script.h"
#include "luavm.h"
#include "physics.h"
#include "audio.h"
#include "renderer.h"
#include "h3dm.h"

static const char* kMainTemplate = R"LUA(-- main.lua - codigo principal do jogo (Lua 5.4)
-- start() roda uma vez ao tocar em PLAY; update(dt) roda a cada quadro.
-- Outros arquivos .lua da pasta scripts/ tambem sao carregados
-- (funcoes e variaveis globais ficam visiveis para todos).

local cubo, esfera
velocidade = 90

function start()
  cubo = find("Cubo 1")
  esfera = find("Esfera 1")
  trace("Jogo iniciado!")
end

function update(dt)
  if cubo then rotate(cubo, 0, velocidade * dt, 0) end
  if esfera then setPos(esfera, 0, 0.6 + math.sin(time() * 2) * 0.4, 0) end
  uiText("Tempo: " .. math.floor(time()))
  if uiButton("Cor aleatoria") then
    setColor(cubo, random(), random(), random())
  end
end
)LUA";

static const char* kApiDoc = R"API(API DE SCRIPT (Lua 5.4) - h3d

Arquivos: scripts/main.lua + outros .lua (todos carregados).
(Projetos antigos com main.hx e sem .lua usam o modo Haxe.)

CICLO DE VIDA
main()  start()  rodam uma vez ao dar Play
update(dt)       roda todo quadro (dt em segundos)

OBJETOS (um objeto e um numero 'handle')
find("nome") -> handle ou nil
exists(o)  getName(o)  destroy(o)
spawn("cubo"|"esfera"|"cilindro"|"plano", x,y,z)
spawnModel("arquivo.obj", x,y,z)
spawnLight(x,y,z, r,g,b, intensidade, alcance)
setPos(o,x,y,z)  move(o,dx,dy,dz)  getPos(o) -> {x=,y=,z=}
setRot(o,x,y,z)  rotate(o,dx,dy,dz)  getRot(o)  (graus)
setScale(o,x,y,z)  getScale(o)
getX(o) getY(o) getZ(o)
setColor(o,r,g,b)  (0 a 1)
setMaterial(o, metalico, rugosidade)
setEmissive(o, r,g,b, forca)
setTexture(o, "arquivo.png")
setVisible(o, true/false)
setLight(o, r,g,b, intensidade)
distance(a,b)  overlaps(a,b)
raycast(ox,oy,oz, dx,dy,dz) -> handle ou nil ; hitDistance()

FISICA
setPhysics(o, "none"|"static"|"dynamic")
setVelocity(o,x,y,z)  getVelocity(o)  addImpulse(o,x,y,z)
isGrounded(o)  setGravity(g)  setBounce(o,v)  setFriction(o,v)  setMass(o,m)
Evento: function onCollision(a, b) ... end   (quando dois objetos comecam a colidir)

ANIMACAO (modelos .glb/.gltf)
animNames(o) -> lista   animDuration(o,"nome")   currentAnim(o)
playAnim(o, "nome", loop, velocidade, transicao)   loop: true/false
playAnimAfter(o, "nome", atraso, loop, velocidade)
stopAnim(o)  setAnimSpeed(o,v)  isAnimPlaying(o)  animTime(o)
Eventos: function onAnimEnd(o, nome) ... end
         function onTap(o) ... end   (toque num objeto durante o Play)
Tambem da para configurar animacao padrao e eventos (iniciar, colidir,
apos tempo, tocar) no inspetor, sem codigo.

AUDIO (wav, mp3, ogg, flac - pasta audio/ do projeto)
playSound("arq.ogg", {opcoes}) -> handle
  opcoes (todas opcionais): volume, pitch, pan(-1..1), loop, fadeIn, delay,
  bus ("music","sfx","ambient","voice"), spatial, x,y,z, minDist, maxDist, rolloff
  tambem vale: playSound("arq.wav", volume, pitch, loop)
playSoundAt("arq", x,y,z, {opcoes})   som 3D numa posicao
playSoundOn(o, "arq", {opcoes})       som 3D que segue o objeto
playMusic("arq", {opcoes})  stopMusic(fadeOut)
stopSound(h, fadeOut)  pauseSound(h)  resumeSound(h)  stopAllSounds()
setSoundVolume(h,v)  setSoundPitch(h,v)  setSoundPan(h,v)  setSoundPos(h,x,y,z)
isSoundPlaying(h)  setMasterVolume(v)  setBusVolume("music",v)
Evento: function onSoundEnd(h) ... end
Exemplo: playSound("tiro.ogg", {volume=0.8, pitch=1.2, bus="sfx"})
Tambem da para ligar sons a eventos (iniciar, colidir, apos tempo, tocar)
no inspetor, e definir a musica de fundo na aba Ambiente.

CAMERA / JOGADOR
camPos()  camForward()  camYaw()  camPitch()
setCamPos(x,y,z)  setCamRot(yaw,pitch)  (graus)
setPlayerControl(true/false)  liga/desliga o andar padrao

ENTRADA
stickX() stickY()  (-1 a 1)  touching()  touchX() touchY()
screenW() screenH()  time()

INTERFACE
uiText("texto")  uiButton("rotulo") -> boolean  uiSameLine()
uiTextAt(x,y,"texto")  (pixels)

AMBIENTE
setSun(azimute, altura)  setSunIntensity(v)  setAmbient(v)
setFog(v)  setExposure(v)

OUTROS
trace(...) / print(...)  escreve no console
random()  random(a,b)  clamp(x,a,b)  lerp(a,b,t)
Alem disso, vale a biblioteca padrao do Lua:
math.sin math.cos math.floor math.sqrt math.pi ...
string.format  table.insert  ipairs  pairs  etc.

EXEMPLO
function update(dt)
  local p = getPos(cubo)
  if p.y < 5 then move(cubo, 0, 2*dt, 0) end
end
)API";

struct Ptr {
    bool active = false, ui = false;
    int gAxis = -1;
    int gMode = 0;
    float gStart = 0, gAcc = 0;
    float x = 0, y = 0, dx = 0, dy = 0, sx = 0, sy = 0, moved = 0;
};

struct ConsoleLine { std::string text; bool error; };

enum Screen { SCR_MENU, SCR_EDITOR };
enum Dlg { DLG_NONE, DLG_NEW_PROJECT, DLG_RENAME_PROJECT, DLG_DUP_PROJECT, DLG_DELETE_PROJECT, DLG_NEW_SCRIPT, DLG_DELETE_SCRIPT };

static int resizeCb(ImGuiInputTextCallbackData* d) {
    if (d->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        std::string* s = (std::string*)d->UserData;
        s->resize((size_t)d->BufTextLen);
        d->Buf = (char*)s->c_str();
    }
    return 0;
}

static bool inputStr(const char* label, std::string& s) {
    char buf[128];
    std::strncpy(buf, s.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    if (ImGui::InputText(label, buf, sizeof(buf))) { s = buf; return true; }
    return false;
}

static double argN(const hx::Args& a, size_t i, double d = 0) { return i < a.size() && a[i].t == hx::Value::Num ? a[i].n : d; }
static std::string argS(const hx::Args& a, size_t i) { return i < a.size() ? hx::toStr(a[i]) : std::string(); }
static void setC3(float* d, float a, float b, float c) { d[0] = a; d[1] = b; d[2] = c; }
static double optN(const hx::Value& o, const char* k, double d) {
    if (o.t != hx::Value::Obj) return d;
    auto it = o.o->find(k);
    return it != o.o->end() && it->second.t == hx::Value::Num ? it->second.n : d;
}
static bool optB(const hx::Value& o, const char* k, bool d) {
    if (o.t != hx::Value::Obj) return d;
    auto it = o.o->find(k);
    return it != o.o->end() && it->second.t != hx::Value::Null ? it->second.truthy() : d;
}
static std::string optS(const hx::Value& o, const char* k, const std::string& d) {
    if (o.t != hx::Value::Obj) return d;
    auto it = o.o->find(k);
    return it != o.o->end() && it->second.t == hx::Value::Str ? it->second.s : d;
}

class App {
public:
    Screen screen = SCR_MENU;
    Scene scene, play;
    Env playEnv;
    Renderer rd;
    std::unique_ptr<hx::Interp> interp;
    std::unique_ptr<LuaVM> lua;
    std::map<std::string, hx::NativeFn> natives;
    PhysicsWorld world;
    AudioSystem audio;
    std::vector<std::string> audioFiles;
    std::map<int, int> follow;     // som -> objeto que ele segue
    std::vector<int> finishedSounds;
    int musicId = 0, previewId = 0;
    float mixMaster = 1, mixBus[4] = {1, 1, 1, 1};
    bool kbToggle = false;
    std::vector<int> tapQueue;
    std::vector<std::pair<int, std::string>> endedEvents;
    bool useLua = false;
    bool scriptOk = false, playerControl = true, scriptUI = false, scriptUIPrev = false;
    std::vector<ConsoleLine> console;

    std::string projectsDir, projDir, curProject;
    std::vector<std::string> projects, scriptFiles, modelFiles, textureFiles;
    std::string curScript, scriptText, scriptMsg;
    bool scriptDirty = false;

    int selected = -1;
    bool playing = false;
    float time = 0;
    int W = 1, H = 1;
    float dp = 2.5f;
    ImFont* fontMono = nullptr;

    float yaw = 0.7f, pitch = 0.5f, dist = 9.0f;
    Vec3 target = Vec3(0, 0.5f, 0);

    Vec3 pPos;
    float pYaw = 0, pPitch = 0;
    float stickX = 0, stickY = 0;

    int gizmoMode = 1;   // 0 sel, 1 mover, 2 girar, 3 escalar
    bool snap = false;
    float lastHit = 0;

    Ptr ptr[10];
    bool multi = false;
    bool showPanel = true;
    int panelTab = 0;
    H3dModel modelEditor = H3dModel::makeDefault();
    std::string modelEditorFile = "modelo.h3dm";
    int modelEditorBone = 0;
    int modelEditorShape = 0;
    int modelEditorClip = 0;
    bool wantPlay = false, wantStop = false, wantClose = false, popupOpen = false;
    float barH = 0;
    std::string status;
    std::vector<std::array<float, 4>> rects;
    int frameCount = 0;

    Dlg dlg = DLG_NONE;
    char dlgBuf[128] = {0};
    std::string dlgTarget, dlgError;
    bool dlgJustOpened = false;

    // ================= projetos =================
    void init(const std::string& baseDir, float density) {
        projectsDir = baseDir + "/projects";
        fsu::mkdirs(projectsDir);
        dp = density;
        audio.init();
        refreshProjects();
        std::srand((unsigned)std::time(nullptr));
    }

    void refreshProjects() { projects = fsu::list(projectsDir, true); }

    static std::string sanitize(std::string n) {
        for (char& c : n) if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|' || c == '.') c = '_';
        while (!n.empty() && n.front() == ' ') n.erase(n.begin());
        while (!n.empty() && n.back() == ' ') n.pop_back();
        if (n.size() > 40) n.resize(40);
        return n;
    }

    static void makeDefault(Scene& s) {
        s.objects.clear();
        s.env = Env();
        s.nextId = 2;
        s.nextUid = 1;
        Object g; g.name = "Chao"; g.shape = SHAPE_PLANE; g.scl = Vec3(20, 1, 20);
        setC3(g.color, 0.38f, 0.55f, 0.35f); g.roughness = 0.95f; g.tile[0] = g.tile[1] = 1; g.physics = PHYS_STATIC;
        s.add(g);
        Object a; a.name = "Cubo 1"; a.shape = SHAPE_CUBE; a.pos = Vec3(-2, 0.5f, 0);
        setC3(a.color, 0.85f, 0.25f, 0.22f); a.roughness = 0.45f;
        s.add(a);
        Object b; b.name = "Esfera 1"; b.shape = SHAPE_SPHERE; b.pos = Vec3(0, 0.5f, 0);
        setC3(b.color, 0.95f, 0.75f, 0.35f); b.metallic = 1.0f; b.roughness = 0.2f;
        s.add(b);
        Object c; c.name = "Cilindro 1"; c.shape = SHAPE_CYLINDER; c.pos = Vec3(2, 3.0f, 0); c.physics = PHYS_DYNAMIC; c.bounce = 0.4f;
        setC3(c.color, 0.25f, 0.5f, 0.9f); c.roughness = 0.6f;
        s.add(c);
        Object l; l.name = "Luz 1"; l.shape = SHAPE_LIGHT; l.pos = Vec3(0, 2.5f, 2.0f);
        setC3(l.color, 1.0f, 0.85f, 0.65f); l.intensity = 12.0f; l.range = 10.0f; l.castShadow = false;
        s.add(l);
    }

    std::string createProject(std::string name) {
        name = sanitize(name);
        if (name.empty()) return "Nome invalido";
        std::string dir = projectsDir + "/" + name;
        if (fsu::exists(dir)) return "Ja existe um projeto com esse nome";
        if (!fsu::mkdirs(dir + "/scripts")) return "Nao foi possivel criar a pasta do projeto";
        fsu::mkdirs(dir + "/models");
        fsu::mkdirs(dir + "/textures");
        fsu::mkdirs(dir + "/audio");
        fsu::writeFile(dir + "/scripts/main.lua", kMainTemplate);
        fsu::writeFile(dir + "/API.txt", kApiDoc);
        Scene s;
        makeDefault(s);
        s.save(dir + "/scene.h3d");
        refreshProjects();
        return "";
    }

    void openProject(const std::string& name) {
        projDir = projectsDir + "/" + name;
        curProject = name;
        if (!scene.load(projDir + "/scene.h3d")) makeDefault(scene);
        selected = -1;
        playing = false;
        target = Vec3(0, 0.5f, 0);
        yaw = 0.7f; pitch = 0.5f; dist = 9.0f;
        rd.setProjectDir(projDir);
        fsu::mkdirs(projDir + "/scripts");
        fsu::mkdirs(projDir + "/models");
        fsu::mkdirs(projDir + "/textures");
        fsu::mkdirs(projDir + "/audio");
        refreshScripts();
        curScript.clear();
        openScript(fsu::exists(projDir + "/scripts/main.lua") ? "main.lua" : (fsu::exists(projDir + "/scripts/main.hx") ? "main.hx" : (scriptFiles.empty() ? "" : scriptFiles[0])));
        refreshAssets();
        status.clear();
        console.clear();
        screen = SCR_EDITOR;
        cancel();
    }

    void saveProject() {
        bool ok = scene.save(projDir + "/scene.h3d");
        if (scriptDirty) saveScript();
        status = ok ? "Projeto salvo" : "Erro ao salvar";
    }

    void closeProject() {
        if (playing) stopPlay();
        audio.stopAll();
        previewId = 0;
        saveProject();
        screen = SCR_MENU;
        rd.clearAssets();
        refreshProjects();
        cancel();
    }

    // ================= scripts / recursos =================
    void refreshScripts() {
        scriptFiles.clear();
        for (auto& f : fsu::list(projDir + "/scripts")) if (fsu::ext(f) == "lua" || fsu::ext(f) == "hx") scriptFiles.push_back(f);
    }
    void openScript(const std::string& f) {
        if (scriptDirty) saveScript();
        curScript = f;
        scriptText.clear();
        if (!f.empty()) fsu::readFile(projDir + "/scripts/" + f, scriptText);
        scriptDirty = false;
        scriptMsg.clear();
    }
    void saveScript() {
        if (curScript.empty()) return;
        fsu::writeFile(projDir + "/scripts/" + curScript, scriptText);
        scriptDirty = false;
    }
    void refreshAssets() {
        modelFiles.clear();
        textureFiles.clear();
        for (auto& f : fsu::list(projDir + "/models")) { std::string e = fsu::ext(f); if (e == "obj" || e == "glb" || e == "gltf") modelFiles.push_back(f); }
        audioFiles.clear();
        for (auto& f : fsu::list(projDir + "/audio")) {
            std::string e = fsu::ext(f);
            if (e == "wav" || e == "mp3" || e == "ogg" || e == "flac") audioFiles.push_back(f);
        }
        for (auto& f : fsu::list(projDir + "/textures")) {
            std::string e = fsu::ext(f);
            if (e == "png" || e == "jpg" || e == "jpeg" || e == "bmp" || e == "tga") textureFiles.push_back(f);
        }
    }

    void logLine(const std::string& s, bool err) {
        console.push_back({s, err});
        if (console.size() > 200) console.erase(console.begin(), console.begin() + 50);
        LOGI("[script] %s", s.c_str());
    }

    // ================= camera =================
    void camBasis(Vec3& eye, Vec3& fwd, Vec3& right, Vec3& up) const {
        if (playing) {
            float cp = std::cos(pPitch);
            fwd = Vec3(std::sin(pYaw) * cp, std::sin(pPitch), -std::cos(pYaw) * cp);
            eye = pPos;
        } else {
            eye = target + Vec3(std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw)) * dist;
            fwd = normalize(target - eye);
        }
        right = normalize(cross(fwd, Vec3(0, 1, 0)));
        up = cross(right, fwd);
    }

    Mat4 viewProj() const {
        Vec3 eye, fwd, right, up;
        camBasis(eye, fwd, right, up);
        float aspect = (float)W / (float)(H > 0 ? H : 1);
        return perspectiveM(60.0f, aspect, 0.1f, 300.0f) * lookAtM(eye, eye + fwd, Vec3(0, 1, 0));
    }

    // ================= objetos =================
    bool hasSel() const { return selected >= 0 && selected < (int)scene.objects.size(); }

    void addPrimitive(int shape) {
        Object o;
        o.shape = shape;
        o.name = std::string(kShapeName[shape]) + " " + std::to_string(scene.nextId++);
        o.pos = Vec3(std::round(target.x), shape == SHAPE_PLANE ? 0.0f : 0.5f, std::round(target.z));
        if (shape == SHAPE_PLANE) { o.scl = Vec3(4, 1, 4); setC3(o.color, 0.6f, 0.6f, 0.62f); o.roughness = 0.8f; }
        if (shape == SHAPE_LIGHT) { o.pos.y = 2.5f; setC3(o.color, 1.0f, 0.9f, 0.75f); o.intensity = 12.0f; o.range = 10.0f; o.castShadow = false; }
        scene.add(o);
        selected = (int)scene.objects.size() - 1;
    }

    void addModel(const std::string& file) {
        Object o;
        o.shape = SHAPE_MODEL;
        o.model = file;
        o.name = fsu::stem(file) + " " + std::to_string(scene.nextId++);
        o.pos = Vec3(std::round(target.x), 0.5f, std::round(target.z));
        o.roughness = 0.6f;
        if (Renderer::isGltfName(file)) {
            setC3(o.color, 1.0f, 1.0f, 1.0f);
            if (const Gltf* g = rd.gltfInfo(file)) {
                for (auto& a : g->anims) {
                    std::string l = a.name;
                    for (char& ch : l) ch = (char)std::tolower((unsigned char)ch);
                    if (l.find("idle") != std::string::npos) { o.animDefault = a.name; break; }
                }
            }
        }
        scene.add(o);
        selected = (int)scene.objects.size() - 1;
    }

    void duplicateSelected() {
        if (!hasSel()) return;
        Object o = scene.objects[selected];
        o.name += " copia";
        o.pos.x += 1.0f;
        scene.add(o);
        selected = (int)scene.objects.size() - 1;
    }

    void deleteSelected() {
        if (!hasSel()) return;
        scene.objects.erase(scene.objects.begin() + selected);
        selected = -1;
    }

    // ================= raycast =================
    static bool rayBox(const Vec3& o, const Vec3& d, const Vec3& h, float& t) {
        float oo[3] = {o.x, o.y, o.z}, dd[3] = {d.x, d.y, d.z}, hh[3] = {h.x, h.y, h.z};
        float tmin = -1e30f, tmax = 1e30f;
        for (int i = 0; i < 3; i++) {
            if (std::fabs(dd[i]) < 1e-8f) {
                if (oo[i] < -hh[i] || oo[i] > hh[i]) return false;
            } else {
                float t1 = (-hh[i] - oo[i]) / dd[i], t2 = (hh[i] - oo[i]) / dd[i];
                if (t1 > t2) std::swap(t1, t2);
                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);
                if (tmin > tmax) return false;
            }
        }
        if (tmax < 0) return false;
        t = tmin >= 0 ? tmin : tmax;
        return true;
    }

    static bool raySphere(const Vec3& lo, const Vec3& ld, float r, float& t) {
        float a = dot(ld, ld), b = 2 * dot(lo, ld), c = dot(lo, lo) - r * r;
        float disc = b * b - 4 * a * c;
        if (disc < 0 || a < 1e-12f) return false;
        float sq = std::sqrt(disc);
        float t1 = (-b - sq) / (2 * a), t2 = (-b + sq) / (2 * a);
        if (t2 < 0) return false;
        t = t1 >= 0 ? t1 : t2;
        return true;
    }

    bool rayObject(const Object& ob, const Vec3& ro, const Vec3& rdir, float& t) {
        if (ob.shape == SHAPE_LIGHT) return raySphere(ro - ob.pos, rdir, 0.45f, t);
        Mat4 R = rotationM(ob.rot);
        Vec3 s(std::max(std::fabs(ob.scl.x), 0.001f), std::max(std::fabs(ob.scl.y), 0.001f), std::max(std::fabs(ob.scl.z), 0.001f));
        Vec3 lo = mulTransposed3(R, ro - ob.pos);
        Vec3 ld = mulTransposed3(R, rdir);
        lo = Vec3(lo.x / s.x, lo.y / s.y, lo.z / s.z);
        ld = Vec3(ld.x / s.x, ld.y / s.y, ld.z / s.z);
        if (ob.shape == SHAPE_SPHERE) return raySphere(lo, ld, 0.5f, t);
        Vec3 h(0.5f, 0.5f, 0.5f);
        if (ob.shape == SHAPE_PLANE) h = Vec3(0.5f, 0.03f, 0.5f);
        else if (ob.shape == SHAPE_MODEL) h = rd.modelHalf(ob.model);
        return rayBox(lo, ld, h, t);
    }

    void pick(float px, float py) {
        Vec3 eye, fwd, right, up;
        camBasis(eye, fwd, right, up);
        float th = std::tan(30.0f * kDeg2Rad);
        float aspect = (float)W / (float)(H > 0 ? H : 1);
        float nx = 2.0f * px / W - 1.0f, ny = 1.0f - 2.0f * py / H;
        Vec3 d = normalize(fwd + right * (nx * th * aspect) + up * (ny * th));
        float best = 1e30f;
        int hit = -1;
        for (size_t i = 0; i < scene.objects.size(); i++) {
            float t;
            if (rayObject(scene.objects[i], eye, d, t) && t < best) { best = t; hit = (int)i; }
        }
        selected = hit;
    }

    // ================= gizmo =================
    float gizmoLen() const { return 80.0f * dp; }

    bool gizmoHandle(int axis, float& hx, float& hy, float& vx, float& vy) const {
        if (!hasSel()) return false;
        const Object& o = scene.objects[selected];
        Mat4 vp = viewProj();
        float sx, sy, tx, ty;
        if (!projectPoint(vp, o.pos, (float)W, (float)H, sx, sy)) return false;
        Vec3 ax(axis == 0 ? 1.0f : 0.0f, axis == 1 ? 1.0f : 0.0f, axis == 2 ? 1.0f : 0.0f);
        if (!projectPoint(vp, o.pos + ax, (float)W, (float)H, tx, ty)) return false;
        vx = tx - sx; vy = ty - sy;
        float len = std::sqrt(vx * vx + vy * vy);
        if (len < 2.0f) return false;
        hx = sx + vx / len * gizmoLen();
        hy = sy + vy / len * gizmoLen();
        return true;
    }

    float* gizmoProp(Object& o, int mode, int axis) {
        Vec3* v = mode == 1 ? &o.pos : (mode == 2 ? &o.rot : &o.scl);
        return &v->x + axis;
    }

    void drawGizmo() {
        if (!hasSel() || gizmoMode == 0) return;
        const Object& o = scene.objects[selected];
        float sx, sy;
        Mat4 vp = viewProj();
        if (!projectPoint(vp, o.pos, (float)W, (float)H, sx, sy)) return;
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        const ImU32 cols[3] = {IM_COL32(240, 70, 70, 255), IM_COL32(80, 230, 80, 255), IM_COL32(80, 130, 255, 255)};
        const char* names[3] = {"X", "Y", "Z"};
        for (int a = 0; a < 3; a++) {
            float hx, hy, vx, vy;
            if (!gizmoHandle(a, hx, hy, vx, vy)) continue;
            dl->AddLine(ImVec2(sx, sy), ImVec2(hx, hy), cols[a], 4.0f);
            dl->AddCircleFilled(ImVec2(hx, hy), 17.0f * dp * 0.8f, cols[a], 24);
            dl->AddText(ImVec2(hx - 5.0f * dp, hy - 8.0f * dp), IM_COL32(0, 0, 0, 255), names[a]);
        }
        dl->AddCircleFilled(ImVec2(sx, sy), 5.0f * dp * 0.7f, IM_COL32(255, 255, 255, 255), 16);
    }

    // ================= toque =================
    bool hitUI(float x, float y) const {
        if (popupOpen) return true;
        for (auto& r : rects)
            if (x >= r[0] && x <= r[0] + r[2] && y >= r[1] && y <= r[1] + r[3]) return true;
        return false;
    }

    int freeCount() const {
        int n = 0;
        for (auto& p : ptr) if (p.active && !p.ui && p.gAxis < 0) n++;
        return n;
    }

    float stickRadius() const { return 0.12f * (float)std::min(W, H); }

    void pDown(int id, float x, float y) {
        if (screen != SCR_EDITOR || id < 0 || id >= 10) return;
        Ptr& p = ptr[id];
        p = Ptr();
        p.active = true;
        p.x = p.sx = x;
        p.y = p.sy = y;
        p.ui = hitUI(x, y);
        if (!p.ui && !playing && hasSel() && gizmoMode > 0) {
            float best = 26.0f * dp;
            for (int a = 0; a < 3; a++) {
                float hx, hy, vx, vy;
                if (!gizmoHandle(a, hx, hy, vx, vy)) continue;
                float d = std::sqrt((hx - x) * (hx - x) + (hy - y) * (hy - y));
                if (d < best) {
                    best = d;
                    p.gAxis = a;
                    p.gMode = gizmoMode;
                    p.gStart = *gizmoProp(scene.objects[selected], gizmoMode, a);
                }
            }
        }
        if (!p.ui && p.gAxis < 0 && freeCount() > 1) multi = true;
    }

    void pMove(int id, float x, float y) {
        if (screen != SCR_EDITOR || id < 0 || id >= 10 || !ptr[id].active) return;
        Ptr& p = ptr[id];
        p.dx = x - p.x; p.dy = y - p.y;
        p.x = x; p.y = y;
        p.moved += std::sqrt(p.dx * p.dx + p.dy * p.dy);
    }

    void endMove() {
        if (screen != SCR_EDITOR) return;
        std::vector<int> ids;
        for (int i = 0; i < 10; i++) if (ptr[i].active && !ptr[i].ui && ptr[i].gAxis < 0) ids.push_back(i);
        if (!playing) {
            for (auto& p : ptr) {
                if (!p.active || p.gAxis < 0 || !hasSel()) continue;
                float hx, hy, vx, vy;
                if (!gizmoHandle(p.gAxis, hx, hy, vx, vy)) continue;
                float amt;
                if (p.gMode == 2) amt = (p.dx * vx + p.dy * vy) / std::sqrt(vx * vx + vy * vy) * 0.5f;
                else amt = (p.dx * vx + p.dy * vy) / (vx * vx + vy * vy);
                p.gAcc += amt;
                float val = p.gStart + p.gAcc;
                if (snap) {
                    float step = p.gMode == 1 ? 0.25f : (p.gMode == 2 ? 15.0f : 0.1f);
                    val = std::round(val / step) * step;
                }
                if (p.gMode == 3) val = std::max(val, 0.01f);
                *gizmoProp(scene.objects[selected], p.gMode, p.gAxis) = val;
            }
            if (ids.size() == 1) {
                Ptr& p = ptr[ids[0]];
                yaw -= p.dx * 0.008f;
                pitch += p.dy * 0.008f;
                pitch = std::max(-1.5f, std::min(1.5f, pitch));
            } else if (ids.size() >= 2) {
                Ptr& a = ptr[ids[0]];
                Ptr& b = ptr[ids[1]];
                float cur = std::hypot(a.x - b.x, a.y - b.y);
                float prev = std::hypot((a.x - a.dx) - (b.x - b.dx), (a.y - a.dy) - (b.y - b.dy));
                if (cur > 1 && prev > 1) {
                    dist *= prev / cur;
                    dist = std::max(0.5f, std::min(150.0f, dist));
                }
                float mdx = (a.dx + b.dx) * 0.5f, mdy = (a.dy + b.dy) * 0.5f;
                Vec3 eye, fwd, right, up;
                camBasis(eye, fwd, right, up);
                float s = dist * 2.0f * std::tan(30.0f * kDeg2Rad) / (float)H;
                target -= right * (mdx * s);
                target += up * (mdy * s);
            }
        } else {
            bool stick = false;
            for (int id : ids) {
                Ptr& p = ptr[id];
                if (p.sx < W * 0.4f) {
                    float R = stickRadius();
                    stickX = std::max(-1.0f, std::min(1.0f, (p.x - p.sx) / R));
                    stickY = std::max(-1.0f, std::min(1.0f, (p.y - p.sy) / R));
                    stick = true;
                } else if (playerControl) {
                    pYaw += p.dx * 0.005f;
                    pPitch -= p.dy * 0.005f;
                    pPitch = std::max(-1.5f, std::min(1.5f, pPitch));
                }
            }
            if (!stick) stickX = stickY = 0;
        }
        for (auto& p : ptr) { p.dx = 0; p.dy = 0; }
    }

    void pUp(int id, float x, float y) {
        if (screen != SCR_EDITOR || id < 0 || id >= 10 || !ptr[id].active) return;
        Ptr& p = ptr[id];
        if (!playing && !p.ui && p.gAxis < 0 && !multi && p.moved < 10.0f * dp) pick(x, y);
        if (playing && !p.ui && p.sx >= W * 0.4f && p.moved < 10.0f * dp) tapPlay(x, y);
        if (playing && p.sx < W * 0.4f) stickX = stickY = 0;
        p = Ptr();
        if (freeCount() == 0) multi = false;
    }

    void cancel() {
        for (auto& p : ptr) p = Ptr();
        multi = false;
        stickX = stickY = 0;
    }

    // ================= play / scripts =================
    // ---------- audio ----------
    std::string audioPath(const std::string& clip) const { return projDir + "/audio/" + clip; }

    static float rnd01() { return std::rand() / (float)RAND_MAX; }

    int playSlot(Object& o, const SoundSlot& s) {
        if (s.clip.empty()) return 0;
        AudioProps p;
        float vv = s.volVar > 0 ? (rnd01() * 2.0f - 1.0f) * s.volVar : 0.0f;
        float pv = s.pitchVar > 0 ? (rnd01() * 2.0f - 1.0f) * s.pitchVar : 0.0f;
        p.volume = s.volume * (1.0f + vv);
        p.pitch = s.pitch * (1.0f + pv);
        p.loop = s.loop;
        p.spatial = s.spatial;
        p.x = o.pos.x; p.y = o.pos.y; p.z = o.pos.z;
        p.minDist = s.minDist; p.maxDist = s.maxDist;
        p.fadeIn = s.fadeIn; p.delay = s.delay;
        p.bus = kBusKey[std::max(0, std::min(3, s.bus))];
        int h = audio.play(audioPath(s.clip), p);
        if (h && s.spatial) follow[h] = o.uid;
        return h;
    }

    void fireSounds(Object& o, int type) {
        for (const SoundSlot& s : o.sounds)
            if (s.trigger == type) playSlot(o, s);
    }

    void previewSound(const std::string& clip, float vol, const char* bus) {
        audio.stop(previewId);
        AudioProps p;
        p.volume = vol;
        p.bus = bus;
        previewId = clip.empty() ? 0 : audio.play(audioPath(clip), p);
    }

    void playMusicClip(const std::string& clip, float vol, bool loop, float fade) {
        if (musicId) audio.stop(musicId, fade > 0 ? fade : 0);
        musicId = 0;
        if (clip.empty()) return;
        AudioProps p;
        p.volume = vol; p.loop = loop; p.fadeIn = fade; p.bus = "music";
        musicId = audio.play(audioPath(clip), p);
    }

    void startPlay() {
        if (scriptDirty) saveScript();
        audio.stopAll();
        follow.clear();
        previewId = 0;
        for (int i = 0; i < 4; i++) audio.setBusVolume(kBusKey[i], mixBus[i]);
        audio.setMasterVolume(mixMaster);
        Vec3 eye, fwd, right, up;
        camBasis(eye, fwd, right, up);
        pPos = eye;
        pYaw = std::atan2(fwd.x, -fwd.z);
        pPitch = std::asin(std::max(-1.0f, std::min(1.0f, fwd.y)));
        play = scene;
        playEnv = scene.env;
        tapQueue.clear();
        world.reset();
        world.gravity = 9.8f;
        world.halfOf = [this](const Object& o) { return worldHalf(o); };
        for (auto& o : play.objects) {
            o.vel = Vec3(); o.grounded = false;
            o.animCur.clear(); o.animPrev.clear(); o.pend.clear();
            o.animPlaying = false; o.animT = 0; o.animBlend = 0; o.animBlendT = 0;
            if (!o.animDefault.empty()) startClip(o, o.animDefault, true, o.animSpeedDef, 0);
            fireTrigger(o, 0);
            for (const SoundSlot& sl : o.sounds) if (sl.trigger == 0 || sl.trigger == 2) playSlot(o, sl);
            for (const AnimTrigger& t : o.triggers)
                if (t.type == 2) { PendAnim pa; pa.delay = std::max(t.delay, 0.001f); pa.clip = t.clip; pa.loop = t.loop; pa.speed = t.speed; o.pend.push_back(pa); }
        }
        time = 0;
        stickX = stickY = 0;
        playerControl = true;
        cancel();
        playing = true;
        console.clear();
        playMusicClip(scene.env.music, scene.env.musicVolume, scene.env.musicLoop, scene.env.musicFade);
        startScripts();
    }

    void stopPlay() {
        playing = false;
        audio.stopAll();
        follow.clear();
        musicId = 0;
        interp.reset();
        lua.reset();
        scriptOk = false;
        stickX = stickY = 0;
        cancel();
    }

    void startScripts() {
        interp.reset();
        lua.reset();
        setupApi();
        scriptOk = true;
        std::vector<std::string> files = fsu::list(projDir + "/scripts");
        bool hasLua = false;
        for (auto& f : files) if (fsu::ext(f) == "lua") hasLua = true;
        useLua = hasLua;
        const std::string want = hasLua ? "lua" : "hx";
        const std::string mainName = "main." + want;
        std::vector<std::string> order;
        if (std::find(files.begin(), files.end(), mainName) != files.end()) order.push_back(mainName);
        for (auto& f : files) if (fsu::ext(f) == want && f != mainName) order.push_back(f);
        if (order.empty()) { scriptOk = false; logLine("Nenhum script na pasta scripts/", false); return; }
        if (useLua) {
            lua.reset(new LuaVM());
            if (!lua->init(natives)) { logLine("Falha ao iniciar o Lua", true); scriptOk = false; return; }
        } else {
            interp.reset(new hx::Interp());
            interp->natives = natives;
        }
        for (auto& f : order) {
            std::string src, err;
            if (!fsu::readFile(projDir + "/scripts/" + f, src)) continue;
            bool ok = useLua ? lua->load(src, f, err) : interp->load(src, f, err);
            if (!ok) { logLine(err, true); scriptOk = false; return; }
        }
        callScript("main", {});
        callScript("start", {});
    }

    void callScript(const char* fn, hx::Args args) {
        if (!scriptOk) return;
        std::string err;
        bool ok = true;
        if (useLua && lua) {
            ok = lua->call(fn, args, err);
        } else if (interp) {
            ok = interp->call(fn, args, err);
        } else return;
        if (!ok) { logLine(err, true); scriptOk = false; }
    }

    Object* objFrom(const hx::Value& v) {
        if (v.t != hx::Value::Num) return nullptr;
        int uid = (int)v.n;
        for (auto& o : play.objects) if (o.uid == uid) return &o;
        return nullptr;
    }
    static hx::Value vec3v(const Vec3& v) {
        hx::Value o = hx::Value::obj();
        (*o.o)["x"] = hx::Value::num(v.x);
        (*o.o)["y"] = hx::Value::num(v.y);
        (*o.o)["z"] = hx::Value::num(v.z);
        return o;
    }
    Vec3 worldHalf(const Object& o) {
        Vec3 h0(0.5f, 0.5f, 0.5f);
        if (o.shape == SHAPE_PLANE) h0 = Vec3(0.5f, 0.03f, 0.5f);
        else if (o.shape == SHAPE_MODEL) h0 = rd.modelHalf(o.model);
        else if (o.shape == SHAPE_LIGHT) return Vec3(0.2f, 0.2f, 0.2f);
        return Vec3(h0.x * std::fabs(o.scl.x), h0.y * std::fabs(o.scl.y), h0.z * std::fabs(o.scl.z));
    }

    void setupApi() {
        natives.clear();
        auto reg = [this](const char* n, hx::NativeFn f) { natives[n] = f; };
        typedef hx::Args A;
        typedef hx::Value V;

        auto tr = [this](A& a) -> V {
            std::string s;
            for (size_t i = 0; i < a.size(); i++) { if (i) s += " "; s += hx::toStr(a[i]); }
            logLine(s, false);
            return V();
        };
        reg("trace", tr);
        reg("print", tr);

        // matematica
        reg("Math.sin", [](A& a) -> V { return V::num(std::sin(argN(a, 0))); });
        reg("Math.cos", [](A& a) -> V { return V::num(std::cos(argN(a, 0))); });
        reg("Math.tan", [](A& a) -> V { return V::num(std::tan(argN(a, 0))); });
        reg("Math.asin", [](A& a) -> V { return V::num(std::asin(argN(a, 0))); });
        reg("Math.acos", [](A& a) -> V { return V::num(std::acos(argN(a, 0))); });
        reg("Math.atan", [](A& a) -> V { return V::num(std::atan(argN(a, 0))); });
        reg("Math.atan2", [](A& a) -> V { return V::num(std::atan2(argN(a, 0), argN(a, 1))); });
        reg("Math.sqrt", [](A& a) -> V { return V::num(std::sqrt(argN(a, 0))); });
        reg("Math.abs", [](A& a) -> V { return V::num(std::fabs(argN(a, 0))); });
        reg("Math.floor", [](A& a) -> V { return V::num(std::floor(argN(a, 0))); });
        reg("Math.ceil", [](A& a) -> V { return V::num(std::ceil(argN(a, 0))); });
        reg("Math.round", [](A& a) -> V { return V::num(std::floor(argN(a, 0) + 0.5)); });
        reg("Math.exp", [](A& a) -> V { return V::num(std::exp(argN(a, 0))); });
        reg("Math.log", [](A& a) -> V { return V::num(std::log(argN(a, 0))); });
        reg("Math.pow", [](A& a) -> V { return V::num(std::pow(argN(a, 0), argN(a, 1))); });
        reg("Math.min", [](A& a) -> V { return V::num(std::min(argN(a, 0), argN(a, 1))); });
        reg("Math.max", [](A& a) -> V { return V::num(std::max(argN(a, 0), argN(a, 1))); });
        reg("Math.random", [](A&) -> V { return V::num(std::rand() / (double)RAND_MAX); });
        reg("random", [](A& a) -> V {
            double r = std::rand() / (double)RAND_MAX;
            if (a.size() >= 2) return V::num(argN(a, 0) + r * (argN(a, 1) - argN(a, 0)));
            return V::num(r);
        });
        reg("clamp", [](A& a) -> V { return V::num(std::max(argN(a, 1), std::min(argN(a, 2), argN(a, 0)))); });
        reg("lerp", [](A& a) -> V { return V::num(argN(a, 0) + (argN(a, 1) - argN(a, 0)) * argN(a, 2)); });
        reg("Std.int", [](A& a) -> V { return V::num(std::trunc(argN(a, 0))); });
        reg("Std.string", [](A& a) -> V { return V::str(argS(a, 0)); });
        reg("time", [this](A&) -> V { return V::num(time); });

        // objetos
        reg("find", [this](A& a) -> V {
            std::string n = argS(a, 0);
            for (auto& o : play.objects) if (o.name == n) return V::num(o.uid);
            return V();
        });
        reg("exists", [this](A& a) -> V { return V::boolean(a.size() > 0 && objFrom(a[0]) != nullptr); });
        reg("getName", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); return o ? V::str(o->name) : V(); });
        reg("destroy", [this](A& a) -> V {
            Object* o = a.empty() ? nullptr : objFrom(a[0]);
            if (o) play.objects.erase(play.objects.begin() + (o - &play.objects[0]));
            return V();
        });
        reg("spawn", [this](A& a) -> V {
            std::string k = argS(a, 0);
            Object o;
            o.shape = SHAPE_CUBE;
            if (k == "sphere" || k == "esfera") o.shape = SHAPE_SPHERE;
            else if (k == "cylinder" || k == "cilindro") o.shape = SHAPE_CYLINDER;
            else if (k == "plane" || k == "plano") o.shape = SHAPE_PLANE;
            o.name = k;
            o.pos = Vec3((float)argN(a, 1), (float)argN(a, 2), (float)argN(a, 3));
            return V::num(play.add(o));
        });
        reg("spawnModel", [this](A& a) -> V {
            Object o;
            o.shape = SHAPE_MODEL;
            o.model = argS(a, 0);
            o.name = o.model;
            o.pos = Vec3((float)argN(a, 1), (float)argN(a, 2), (float)argN(a, 3));
            return V::num(play.add(o));
        });
        reg("spawnLight", [this](A& a) -> V {
            Object o;
            o.shape = SHAPE_LIGHT;
            o.name = "luz";
            o.pos = Vec3((float)argN(a, 0), (float)argN(a, 1), (float)argN(a, 2));
            o.color[0] = (float)argN(a, 3, 1); o.color[1] = (float)argN(a, 4, 1); o.color[2] = (float)argN(a, 5, 1);
            o.intensity = (float)argN(a, 6, 10);
            o.range = (float)argN(a, 7, 10);
            return V::num(play.add(o));
        });
        reg("setPos", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); if (o) o->pos = Vec3((float)argN(a, 1), (float)argN(a, 2), (float)argN(a, 3)); return V(); });
        reg("move", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); if (o) o->pos += Vec3((float)argN(a, 1), (float)argN(a, 2), (float)argN(a, 3)); return V(); });
        reg("setRot", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); if (o) o->rot = Vec3((float)argN(a, 1), (float)argN(a, 2), (float)argN(a, 3)); return V(); });
        reg("rotate", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); if (o) o->rot += Vec3((float)argN(a, 1), (float)argN(a, 2), (float)argN(a, 3)); return V(); });
        reg("setScale", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); if (o) o->scl = Vec3((float)argN(a, 1, 1), (float)argN(a, 2, 1), (float)argN(a, 3, 1)); return V(); });
        reg("getPos", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); return o ? vec3v(o->pos) : V(); });
        reg("getRot", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); return o ? vec3v(o->rot) : V(); });
        reg("getScale", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); return o ? vec3v(o->scl) : V(); });
        reg("getX", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); return V::num(o ? o->pos.x : 0); });
        reg("getY", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); return V::num(o ? o->pos.y : 0); });
        reg("getZ", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); return V::num(o ? o->pos.z : 0); });
        reg("setColor", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); if (o) { o->color[0] = (float)argN(a, 1); o->color[1] = (float)argN(a, 2); o->color[2] = (float)argN(a, 3); } return V(); });
        reg("setMaterial", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); if (o) { o->metallic = (float)argN(a, 1); o->roughness = (float)argN(a, 2, 0.5); } return V(); });
        reg("setEmissive", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); if (o) { o->emissive[0] = (float)argN(a, 1); o->emissive[1] = (float)argN(a, 2); o->emissive[2] = (float)argN(a, 3); o->emissiveStrength = (float)argN(a, 4, 1); } return V(); });
        reg("setTexture", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); if (o) o->texture = argS(a, 1); return V(); });
        reg("setVisible", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); if (o && a.size() > 1) o->visible = a[1].truthy(); return V(); });
        reg("setLight", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); if (o) { o->color[0] = (float)argN(a, 1, 1); o->color[1] = (float)argN(a, 2, 1); o->color[2] = (float)argN(a, 3, 1); o->intensity = (float)argN(a, 4, 10); } return V(); });
        reg("distance", [this](A& a) -> V {
            Object* p = a.size() > 0 ? objFrom(a[0]) : nullptr;
            Object* q = a.size() > 1 ? objFrom(a[1]) : nullptr;
            return V::num(p && q ? length(p->pos - q->pos) : 1e9);
        });
        reg("overlaps", [this](A& a) -> V {
            Object* p = a.size() > 0 ? objFrom(a[0]) : nullptr;
            Object* q = a.size() > 1 ? objFrom(a[1]) : nullptr;
            if (!p || !q) return V::boolean(false);
            Vec3 hp = worldHalf(*p), hq = worldHalf(*q), d = p->pos - q->pos;
            return V::boolean(std::fabs(d.x) <= hp.x + hq.x && std::fabs(d.y) <= hp.y + hq.y && std::fabs(d.z) <= hp.z + hq.z);
        });
        reg("raycast", [this](A& a) -> V {
            Vec3 o((float)argN(a, 0), (float)argN(a, 1), (float)argN(a, 2));
            Vec3 d = normalize(Vec3((float)argN(a, 3), (float)argN(a, 4), (float)argN(a, 5)));
            float best = 1e30f;
            int hit = -1;
            for (auto& ob : play.objects) {
                if (!ob.visible || ob.shape == SHAPE_LIGHT) continue;
                float t;
                if (rayObject(ob, o, d, t) && t < best) { best = t; hit = ob.uid; }
            }
            lastHit = best;
            return hit < 0 ? V() : V::num(hit);
        });
        reg("hitDistance", [this](A&) -> V { return V::num(lastHit); });

        // camera / jogador
        reg("camPos", [this](A&) -> V { return vec3v(pPos); });
        reg("camForward", [this](A&) -> V {
            float cp = std::cos(pPitch);
            return vec3v(Vec3(std::sin(pYaw) * cp, std::sin(pPitch), -std::cos(pYaw) * cp));
        });
        reg("camYaw", [this](A&) -> V { return V::num(pYaw / kDeg2Rad); });
        reg("camPitch", [this](A&) -> V { return V::num(pPitch / kDeg2Rad); });
        reg("setCamPos", [this](A& a) -> V { pPos = Vec3((float)argN(a, 0), (float)argN(a, 1), (float)argN(a, 2)); return V(); });
        reg("setCamRot", [this](A& a) -> V { pYaw = (float)argN(a, 0) * kDeg2Rad; pPitch = std::max(-1.5f, std::min(1.5f, (float)argN(a, 1) * kDeg2Rad)); return V(); });
        reg("setPlayerControl", [this](A& a) -> V { playerControl = a.empty() ? true : a[0].truthy(); return V(); });

        // entrada
        reg("stickX", [this](A&) -> V { return V::num(stickX); });
        reg("stickY", [this](A&) -> V { return V::num(stickY); });
        reg("touching", [this](A&) -> V {
            for (auto& p : ptr) if (p.active && !p.ui && p.sx >= W * 0.4f) return V::boolean(true);
            return V::boolean(false);
        });
        reg("touchX", [this](A&) -> V {
            for (auto& p : ptr) if (p.active && !p.ui && p.sx >= W * 0.4f) return V::num(p.x);
            return V::num(0);
        });
        reg("touchY", [this](A&) -> V {
            for (auto& p : ptr) if (p.active && !p.ui && p.sx >= W * 0.4f) return V::num(p.y);
            return V::num(0);
        });
        reg("screenW", [this](A&) -> V { return V::num(W); });
        reg("screenH", [this](A&) -> V { return V::num(H); });

        // interface
        reg("uiText", [this](A& a) -> V { scriptUI = true; ImGui::TextUnformatted(argS(a, 0).c_str()); return V(); });
        reg("uiButton", [this](A& a) -> V { scriptUI = true; return V::boolean(ImGui::Button(argS(a, 0).c_str())); });
        reg("uiSameLine", [this](A&) -> V { ImGui::SameLine(); return V(); });
        reg("uiTextAt", [](A& a) -> V {
            ImGui::GetForegroundDrawList()->AddText(ImVec2((float)argN(a, 0), (float)argN(a, 1)), IM_COL32(255, 255, 255, 255), argS(a, 2).c_str());
            return V();
        });

        // fisica
        reg("setPhysics", [this](A& a) -> V {
            Object* o = a.empty() ? nullptr : objFrom(a[0]);
            if (o) {
                std::string m = argS(a, 1);
                o->physics = (m == "dynamic" || m == "dinamico") ? PHYS_DYNAMIC : ((m == "static" || m == "estatico") ? PHYS_STATIC : PHYS_NONE);
            }
            return V();
        });
        reg("setVelocity", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); if (o) o->vel = Vec3((float)argN(a, 1), (float)argN(a, 2), (float)argN(a, 3)); return V(); });
        reg("getVelocity", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); return o ? vec3v(o->vel) : V(); });
        reg("addImpulse", [this](A& a) -> V {
            Object* o = a.empty() ? nullptr : objFrom(a[0]);
            if (o) o->vel += Vec3((float)argN(a, 1), (float)argN(a, 2), (float)argN(a, 3)) * (1.0f / std::max(o->mass, 0.001f));
            return V();
        });
        reg("isGrounded", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); return V::boolean(o && o->grounded); });
        reg("setGravity", [this](A& a) -> V { world.gravity = (float)argN(a, 0, 9.8); return V(); });
        reg("setBounce", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); if (o) o->bounce = (float)argN(a, 1); return V(); });
        reg("setFriction", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); if (o) o->friction = (float)argN(a, 1); return V(); });
        reg("setMass", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); if (o) o->mass = (float)argN(a, 1, 1); return V(); });

        // animacao
        reg("playAnim", [this](A& a) -> V {
            Object* o = a.empty() ? nullptr : objFrom(a[0]);
            if (o) startClip(*o, argS(a, 1), a.size() > 2 && a[2].truthy(), (float)argN(a, 3, 1), (float)argN(a, 4, o->animBlendDef));
            return V();
        });
        reg("playAnimAfter", [this](A& a) -> V {
            Object* o = a.empty() ? nullptr : objFrom(a[0]);
            if (o) {
                PendAnim p;
                p.clip = argS(a, 1); p.delay = (float)std::max(argN(a, 2), 0.001); p.loop = a.size() > 3 && a[3].truthy(); p.speed = (float)argN(a, 4, 1);
                o->pend.push_back(p);
            }
            return V();
        });
        reg("stopAnim", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); if (o) o->animPlaying = false; return V(); });
        reg("setAnimSpeed", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); if (o) o->animSpeed = (float)argN(a, 1, 1); return V(); });
        reg("isAnimPlaying", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); return V::boolean(o && o->animPlaying); });
        reg("animTime", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); return V::num(o ? o->animT : 0); });
        reg("currentAnim", [this](A& a) -> V { Object* o = a.empty() ? nullptr : objFrom(a[0]); return o ? V::str(o->animCur) : V(); });
        reg("animNames", [this](A& a) -> V {
            V r = V::arr();
            Object* o = a.empty() ? nullptr : objFrom(a[0]);
            const Gltf* g = o ? gltfOf(*o) : nullptr;
            if (g) for (auto& an : g->anims) r.a->push_back(V::str(an.name));
            return r;
        });
        reg("animDuration", [this](A& a) -> V {
            Object* o = a.empty() ? nullptr : objFrom(a[0]);
            const Gltf* g = o ? gltfOf(*o) : nullptr;
            const GAnim* an = g ? g->find(argS(a, 1)) : nullptr;
            return V::num(an ? an->duration : 0);
        });

        // audio
        auto propsFrom = [](const A& a, size_t idx) -> AudioProps {
            AudioProps p;
            if (idx >= a.size()) return p;
            const V& v = a[idx];
            if (v.t == V::Obj) {
                p.volume = (float)optN(v, "volume", 1); p.pitch = (float)optN(v, "pitch", 1); p.pan = (float)optN(v, "pan", 0);
                p.loop = optB(v, "loop", false); p.spatial = optB(v, "spatial", false);
                p.x = (float)optN(v, "x", 0); p.y = (float)optN(v, "y", 0); p.z = (float)optN(v, "z", 0);
                p.minDist = (float)optN(v, "minDist", 1); p.maxDist = (float)optN(v, "maxDist", 25); p.rolloff = (float)optN(v, "rolloff", 1);
                p.fadeIn = (float)optN(v, "fadeIn", 0); p.delay = (float)optN(v, "delay", 0);
                std::string b = optS(v, "bus", "sfx");
                p.bus = b;
            } else if (v.t == V::Num) {
                p.volume = (float)v.n;
                p.pitch = (float)argN(a, idx + 1, 1);
                p.loop = a.size() > idx + 2 && a[idx + 2].truthy();
            }
            return p;
        };
        reg("playSound", [this, propsFrom](A& a) -> V {
            AudioProps p = propsFrom(a, 1);
            int h = audio.play(audioPath(argS(a, 0)), p);
            return h ? V::num(h) : V();
        });
        reg("playSoundAt", [this, propsFrom](A& a) -> V {
            AudioProps p = propsFrom(a, 4);
            p.spatial = true;
            p.x = (float)argN(a, 1); p.y = (float)argN(a, 2); p.z = (float)argN(a, 3);
            int h = audio.play(audioPath(argS(a, 0)), p);
            return h ? V::num(h) : V();
        });
        reg("playSoundOn", [this, propsFrom](A& a) -> V {
            Object* o = a.empty() ? nullptr : objFrom(a[0]);
            if (!o) return V();
            AudioProps p = propsFrom(a, 2);
            p.spatial = true;
            p.x = o->pos.x; p.y = o->pos.y; p.z = o->pos.z;
            int h = audio.play(audioPath(argS(a, 1)), p);
            if (h) follow[h] = o->uid;
            return h ? V::num(h) : V();
        });
        reg("playMusic", [this, propsFrom](A& a) -> V {
            AudioProps p = propsFrom(a, 1);
            if (musicId) audio.stop(musicId, 0.5f);
            p.bus = "music";
            p.spatial = false;
            musicId = audio.play(audioPath(argS(a, 0)), p);
            return musicId ? V::num(musicId) : V();
        });
        reg("stopMusic", [this](A& a) -> V { if (musicId) audio.stop(musicId, (float)argN(a, 0, 0)); musicId = 0; return V(); });
        reg("stopSound", [this](A& a) -> V { audio.stop((int)argN(a, 0), (float)argN(a, 1, 0)); follow.erase((int)argN(a, 0)); return V(); });
        reg("pauseSound", [this](A& a) -> V { audio.pause((int)argN(a, 0)); return V(); });
        reg("resumeSound", [this](A& a) -> V { audio.resume((int)argN(a, 0)); return V(); });
        reg("stopAllSounds", [this](A&) -> V { audio.stopAll(); follow.clear(); musicId = 0; return V(); });
        reg("setSoundVolume", [this](A& a) -> V { audio.setVolume((int)argN(a, 0), (float)argN(a, 1, 1)); return V(); });
        reg("setSoundPitch", [this](A& a) -> V { audio.setPitch((int)argN(a, 0), (float)argN(a, 1, 1)); return V(); });
        reg("setSoundPan", [this](A& a) -> V { audio.setPan((int)argN(a, 0), (float)argN(a, 1, 0)); return V(); });
        reg("setSoundPos", [this](A& a) -> V { audio.setPosition((int)argN(a, 0), (float)argN(a, 1), (float)argN(a, 2), (float)argN(a, 3)); return V(); });
        reg("isSoundPlaying", [this](A& a) -> V { return V::boolean(audio.isPlaying((int)argN(a, 0))); });
        reg("setMasterVolume", [this](A& a) -> V { mixMaster = (float)argN(a, 0, 1); audio.setMasterVolume(mixMaster); return V(); });
        reg("setBusVolume", [this](A& a) -> V { audio.setBusVolume(argS(a, 0), (float)argN(a, 1, 1)); return V(); });

        // ambiente
        reg("setSun", [this](A& a) -> V { playEnv.sunYaw = (float)argN(a, 0); playEnv.sunPitch = -(float)argN(a, 1); return V(); });
        reg("setSunIntensity", [this](A& a) -> V { playEnv.sunIntensity = (float)argN(a, 0); return V(); });
        reg("setAmbient", [this](A& a) -> V { playEnv.ambient = (float)argN(a, 0); return V(); });
        reg("setFog", [this](A& a) -> V { playEnv.fog = (float)argN(a, 0); return V(); });
        reg("setExposure", [this](A& a) -> V { playEnv.exposure = (float)argN(a, 0); return V(); });
    }

    void update(float dt) {
        time += dt;
        frameCount++;
        if (screen != SCR_EDITOR) return;
        {
            Vec3 eye, fwd, right, up;
            camBasis(eye, fwd, right, up);
            audio.setListener(eye.x, eye.y, eye.z, fwd.x, fwd.y, fwd.z, 0, 1, 0);
            if (playing) {
                for (auto it = follow.begin(); it != follow.end();) {
                    Object* fo = uidObj(it->second);
                    if (fo) { audio.setPosition(it->first, fo->pos.x, fo->pos.y, fo->pos.z); ++it; }
                    else it = follow.erase(it);
                }
            }
            finishedSounds.clear();
            audio.update(finishedSounds);
            for (int h : finishedSounds) follow.erase(h);
        }
        if (!playing) { previewAnims(dt); return; }
        for (auto& o : play.objects) {
            const Object* b = nullptr;
            for (auto& s2 : scene.objects) if (s2.uid == o.uid) { b = &s2; break; }
            if (!b) continue;
            if (b->behavior == BHV_SPIN) o.rot.y = b->rot.y + time * 90.0f;
            else if (b->behavior == BHV_BOB) o.pos.y = b->pos.y + std::sin(time * 2.0f) * 0.5f;
        }
        endedEvents.clear();
        for (auto& o : play.objects) stepAnim(o, dt, &endedEvents);
        world.step(play.objects, dt);
        for (auto& pr : world.enter) {
            if (Object* a = uidObj(pr.first)) { fireTrigger(*a, 1); fireSounds(*a, 1); }
            if (Object* b = uidObj(pr.second)) { fireTrigger(*b, 1); fireSounds(*b, 1); }
        }
        ImGui::SetNextWindowPos(ImVec2(W * 0.5f, 10.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowBgAlpha(scriptUIPrev ? 0.55f : 0.0f);
        ImGui::Begin("##sui", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoNav);
        scriptUI = false;
        std::vector<std::pair<int, int>> enters = world.enter;
        for (auto& pr : enters) callScript("onCollision", {hx::Value::num(pr.first), hx::Value::num(pr.second)});
        std::vector<std::pair<int, std::string>> ended = endedEvents;
        for (auto& e : ended) callScript("onAnimEnd", {hx::Value::num(e.first), hx::Value::str(e.second)});
        std::vector<int> taps = tapQueue;
        tapQueue.clear();
        for (int uid : taps) {
            if (Object* o = uidObj(uid)) { fireTrigger(*o, 3); fireSounds(*o, 3); }
            callScript("onTap", {hx::Value::num(uid)});
        }
        std::vector<int> sfin = finishedSounds;
        for (int h : sfin) callScript("onSoundEnd", {hx::Value::num(h)});
        callScript("update", {hx::Value::num(dt)});
        if (scriptUI) trackRect();
        scriptUIPrev = scriptUI;
        ImGui::End();

        if (playerControl) {
            Vec3 eye, fwd, right, up;
            camBasis(eye, fwd, right, up);
            Vec3 mv = fwd * (-stickY) + right * stickX;
            pPos += mv * (4.0f * dt);
        }
    }

    // ---------- animacao ----------
    const Gltf* gltfOf(const Object& o) {
        if (o.shape != SHAPE_MODEL || !Renderer::isGltfName(o.model)) return nullptr;
        return rd.gltfInfo(o.model);
    }

    void startClip(Object& o, const std::string& name, bool loop, float speed, float blend) {
        const Gltf* g = gltfOf(o);
        if (!g || !g->find(name)) return;
        if (o.animPlaying && o.animCur == name && o.animLoop && loop) { o.animSpeed = speed; return; }
        if (!o.animCur.empty() && blend > 0) {
            o.animPrev = o.animCur; o.animPrevT = o.animT; o.animBlend = blend; o.animBlendT = 0;
        } else { o.animPrev.clear(); o.animBlend = 0; o.animBlendT = 0; }
        o.animCur = name; o.animT = 0; o.animLoop = loop; o.animSpeed = speed; o.animPlaying = true;
    }

    void fireTrigger(Object& o, int type) {
        for (const AnimTrigger& t : o.triggers) {
            if (t.type != type) continue;
            if (t.delay > 0) {
                PendAnim p;
                p.delay = t.delay; p.clip = t.clip; p.loop = t.loop; p.speed = t.speed;
                o.pend.push_back(p);
            } else startClip(o, t.clip, t.loop, t.speed, o.animBlendDef);
        }
    }

    void stepAnim(Object& o, float dt, std::vector<std::pair<int, std::string>>* ended) {
        const Gltf* g = gltfOf(o);
        if (!g) return;
        for (size_t i = 0; i < o.pend.size();) {
            o.pend[i].delay -= dt;
            if (o.pend[i].delay <= 0) {
                PendAnim p = o.pend[i];
                o.pend.erase(o.pend.begin() + i);
                startClip(o, p.clip, p.loop, p.speed, o.animBlendDef);
            } else i++;
        }
        if (!o.animPlaying || o.animCur.empty()) return;
        const GAnim* a = g->find(o.animCur);
        if (!a || a->duration <= 0) { o.animPlaying = false; return; }
        o.animT += dt * o.animSpeed;
        if (o.animBlendT < o.animBlend) {
            o.animBlendT += dt;
            const GAnim* pa = g->find(o.animPrev);
            if (pa && pa->duration > 0) o.animPrevT = std::fmod(o.animPrevT + dt, pa->duration);
        }
        if (o.animT >= a->duration) {
            if (o.animLoop) o.animT = std::fmod(o.animT, a->duration);
            else {
                o.animT = a->duration;
                o.animPlaying = false;
                if (ended) ended->push_back(std::make_pair(o.uid, o.animCur));
                if (o.animReturn && !o.animDefault.empty() && o.animCur != o.animDefault)
                    startClip(o, o.animDefault, true, o.animSpeedDef, o.animBlendDef);
            }
        }
    }

    // no editor: mostra a animacao padrao em loop como previa
    void previewAnims(float dt) {
        for (auto& o : scene.objects) {
            if (o.shape != SHAPE_MODEL) continue;
            if (o.animDefault.empty()) { o.animCur.clear(); o.animPlaying = false; continue; }
            if (!gltfOf(o)) continue;
            if (o.animCur != o.animDefault) {
                o.animCur = o.animDefault; o.animT = 0; o.animLoop = true; o.animPlaying = true; o.animBlend = 0;
            }
            o.animSpeed = o.animSpeedDef;
            o.animLoop = true;
            o.animPlaying = true;
            stepAnim(o, dt, nullptr);
        }
    }

    Object* uidObj(int uid) {
        for (auto& o : play.objects) if (o.uid == uid) return &o;
        return nullptr;
    }

    void tapPlay(float px, float py) {
        Vec3 eye, fwd, right, up;
        camBasis(eye, fwd, right, up);
        float th = std::tan(30.0f * kDeg2Rad);
        float aspect = (float)W / (float)(H > 0 ? H : 1);
        float nx = 2.0f * px / W - 1.0f, ny = 1.0f - 2.0f * py / H;
        Vec3 d = normalize(fwd + right * (nx * th * aspect) + up * (ny * th));
        float best = 1e30f;
        int hit = -1;
        for (auto& o : play.objects) {
            if (!o.visible || o.shape == SHAPE_LIGHT) continue;
            float t;
            if (rayObject(o, eye, d, t) && t < best) { best = t; hit = o.uid; }
        }
        if (hit >= 0) tapQueue.push_back(hit);
    }

    // ================= interface =================
    void trackRect() {
        ImVec2 p = ImGui::GetWindowPos(), s = ImGui::GetWindowSize();
        rects.push_back({p.x, p.y, s.x, s.y});
    }

    bool wrapBtn(const char* label, bool& first, bool active = false) {
        ImGuiStyle& st = ImGui::GetStyle();
        float w = ImGui::CalcTextSize(label).x + st.FramePadding.x * 2.0f;
        if (!first) {
            float x2 = ImGui::GetItemRectMax().x + st.ItemSpacing.x + w;
            float lim = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
            if (x2 < lim) ImGui::SameLine();
        }
        first = false;
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.28f, 1.0f));
        bool r = ImGui::Button(label);
        if (active) ImGui::PopStyleColor();
        return r;
    }

    void openDialog(Dlg d, const std::string& target, const std::string& initial) {
        dlg = d;
        dlgTarget = target;
        dlgError.clear();
        std::strncpy(dlgBuf, initial.c_str(), sizeof(dlgBuf) - 1);
        dlgBuf[sizeof(dlgBuf) - 1] = 0;
        dlgJustOpened = true;
    }

    void ui() {
        ImGuiIO& io = ImGui::GetIO();
        W = (int)io.DisplaySize.x;
        H = (int)io.DisplaySize.y;
        rects.clear();
        if (screen == SCR_MENU) menuUI();
        else if (playing) playUI();
        else editorUI();
        dialogUI();

        char buf[96];
        std::snprintf(buf, sizeof(buf), "%.0f FPS | %d objetos", io.Framerate, (int)(playing ? play.objects.size() : scene.objects.size()));
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        if (screen == SCR_EDITOR) {
            dl->AddText(ImVec2(10.0f * dp, H - 22.0f * dp), IM_COL32(230, 230, 230, 200), buf);
            if (!playing) {
                dl->AddText(ImVec2(10.0f * dp, H - 42.0f * dp), IM_COL32(200, 200, 200, 160),
                            "1 dedo: orbitar | 2 dedos: zoom/mover | toque: selecionar | arraste as setas");
                drawGizmo();
            }
        }
        popupOpen = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);

        if (wantPlay) { wantPlay = false; startPlay(); }
        if (wantStop) { wantStop = false; stopPlay(); }
        if (wantClose) { wantClose = false; closeProject(); }
    }

    void menuUI() {
        const ImGuiWindowFlags fl = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse;
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)W, (float)H));
        ImGui::Begin("##menu", nullptr, fl);
        trackRect();
        ImGui::SetWindowFontScale(2.0f);
        ImGui::Text("h3d");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextDisabled("Engine 3D - editor de cenas e jogos em Lua");
        ImGui::Separator();
        if (ImGui::Button("+ Novo projeto")) openDialog(DLG_NEW_PROJECT, "", "Meu Jogo");
        ImGui::SameLine();
        if (ImGui::Button("Atualizar lista")) refreshProjects();
        ImGui::Text("Projetos (%d)", (int)projects.size());
        ImGui::TextDisabled("Pasta: %s", projectsDir.c_str());
        ImGui::BeginChild("##plist", ImVec2(0, 0), true);
        if (projects.empty()) ImGui::TextWrapped("Nenhum projeto ainda. Toque em \"+ Novo projeto\".");
        for (size_t i = 0; i < projects.size(); i++) {
            const std::string name = projects[i];
            ImGui::PushID((int)i);
            ImGui::Text("%s", name.c_str());
            if (ImGui::Button("Abrir")) { openProject(name); ImGui::PopID(); break; }
            ImGui::SameLine();
            if (ImGui::Button("Renomear")) openDialog(DLG_RENAME_PROJECT, name, name);
            ImGui::SameLine();
            if (ImGui::Button("Duplicar")) openDialog(DLG_DUP_PROJECT, name, name + " copia");
            ImGui::SameLine();
            if (ImGui::Button("Excluir")) openDialog(DLG_DELETE_PROJECT, name, "");
            ImGui::Separator();
            ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::End();
    }

    void dialogUI() {
        if (dlg == DLG_NONE) return;
        bool isInput = dlg != DLG_DELETE_PROJECT && dlg != DLG_DELETE_SCRIPT;
        float w = std::min((float)W * 0.92f, 560.0f * dp / 2.0f + 200.0f);
        ImGui::SetNextWindowPos(ImVec2(W * 0.5f, H * 0.05f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(w, 0));
        ImGui::Begin("##dlg", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar);
        trackRect();
        const char* title = "";
        switch (dlg) {
            case DLG_NEW_PROJECT: title = "Novo projeto"; break;
            case DLG_RENAME_PROJECT: title = "Renomear projeto"; break;
            case DLG_DUP_PROJECT: title = "Duplicar projeto"; break;
            case DLG_DELETE_PROJECT: title = "Excluir projeto"; break;
            case DLG_NEW_SCRIPT: title = "Novo script (.lua)"; break;
            case DLG_DELETE_SCRIPT: title = "Excluir script"; break;
            default: break;
        }
        ImGui::Text("%s", title);
        ImGui::Separator();
        bool ok = false;
        if (isInput) {
            ImGui::Text("Nome:");
            if (dlgJustOpened) { ImGui::SetKeyboardFocusHere(); dlgJustOpened = false; kbToggle = true; }
            ImGui::PushItemWidth(-1);
            if (ImGui::InputText("##dlgname", dlgBuf, sizeof(dlgBuf), ImGuiInputTextFlags_EnterReturnsTrue)) ok = true;
            ImGui::PopItemWidth();
        } else {
            ImGui::TextWrapped("Excluir \"%s\"? Isso nao pode ser desfeito.", dlgTarget.c_str());
        }
        if (!dlgError.empty()) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", dlgError.c_str());
        if (ImGui::Button(isInput ? "OK" : "Excluir")) ok = true;
        ImGui::SameLine();
        if (ImGui::Button("Cancelar")) { dlg = DLG_NONE; }
        if (isInput) { ImGui::SameLine(); if (ImGui::Button("Teclado")) kbToggle = true; }
        ImGui::End();
        if (!ok || dlg == DLG_NONE) return;

        std::string nm = sanitize(dlgBuf);
        switch (dlg) {
            case DLG_NEW_PROJECT: {
                std::string e = createProject(nm);
                if (e.empty()) { dlg = DLG_NONE; openProject(nm); } else dlgError = e;
                break;
            }
            case DLG_RENAME_PROJECT: {
                if (nm.empty()) { dlgError = "Nome invalido"; break; }
                if (nm != dlgTarget && fsu::exists(projectsDir + "/" + nm)) { dlgError = "Ja existe um projeto com esse nome"; break; }
                if (nm != dlgTarget && std::rename((projectsDir + "/" + dlgTarget).c_str(), (projectsDir + "/" + nm).c_str()) != 0) { dlgError = "Falha ao renomear"; break; }
                refreshProjects();
                dlg = DLG_NONE;
                break;
            }
            case DLG_DUP_PROJECT: {
                if (nm.empty()) { dlgError = "Nome invalido"; break; }
                if (fsu::exists(projectsDir + "/" + nm)) { dlgError = "Ja existe um projeto com esse nome"; break; }
                fsu::copyDir(projectsDir + "/" + dlgTarget, projectsDir + "/" + nm);
                refreshProjects();
                dlg = DLG_NONE;
                break;
            }
            case DLG_DELETE_PROJECT:
                fsu::removeAll(projectsDir + "/" + dlgTarget);
                refreshProjects();
                dlg = DLG_NONE;
                break;
            case DLG_NEW_SCRIPT: {
                if (nm.empty()) { dlgError = "Nome invalido"; break; }
                std::string fn = nm + ".lua";
                if (fsu::exists(projDir + "/scripts/" + fn)) { dlgError = "Esse script ja existe"; break; }
                fsu::writeFile(projDir + "/scripts/" + fn, "-- " + fn + "\n-- Funcoes e variaveis globais daqui ficam disponiveis para todos os scripts.\n\n");
                refreshScripts();
                openScript(fn);
                dlg = DLG_NONE;
                break;
            }
            case DLG_DELETE_SCRIPT:
                fsu::removeAll(projDir + "/scripts/" + dlgTarget);
                curScript.clear();
                scriptText.clear();
                scriptDirty = false;
                refreshScripts();
                openScript(scriptFiles.empty() ? "" : scriptFiles[0]);
                dlg = DLG_NONE;
                break;
            default: dlg = DLG_NONE; break;
        }
    }

    void editorUI() {
        const ImGuiWindowFlags fl = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse;
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)W, 0));
        ImGui::Begin("##bar", nullptr, fl);
        trackRect();
        barH = ImGui::GetWindowSize().y;
        bool first = true;
        // projeto
        if (wrapBtn("< Menu", first)) wantClose = true;
        if (wrapBtn("Salvar", first)) saveProject();
        // ferramentas
        if (wrapBtn("Sel", first, gizmoMode == 0)) gizmoMode = 0;
        if (wrapBtn("Mover", first, gizmoMode == 1)) gizmoMode = 1;
        if (wrapBtn("Girar", first, gizmoMode == 2)) gizmoMode = 2;
        if (wrapBtn("Escalar", first, gizmoMode == 3)) gizmoMode = 3;
        if (wrapBtn("Grade", first, snap)) snap = !snap;
        // objetos
        if (wrapBtn("+ Adicionar", first)) ImGui::OpenPopup("##add");
        bool has = hasSel();
        if (!has) ImGui::BeginDisabled();
        if (wrapBtn("Duplicar", first)) duplicateSelected();
        if (wrapBtn("Excluir", first)) deleteSelected();
        if (!has) ImGui::EndDisabled();
        // vista / entrada
        if (wrapBtn("Painel", first, showPanel)) showPanel = !showPanel;
        if (wrapBtn("Teclado", first)) kbToggle = true;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.55f, 0.20f, 1));
        if (wrapBtn("PLAY", first)) wantPlay = true;
        ImGui::PopStyleColor();
        if (!status.empty()) { ImGui::SameLine(); ImGui::TextDisabled("%s", status.c_str()); }
        if (ImGui::BeginPopup("##add")) {
            ImGui::TextDisabled("Formas");
            if (ImGui::Selectable("Cubo")) addPrimitive(SHAPE_CUBE);
            if (ImGui::Selectable("Esfera")) addPrimitive(SHAPE_SPHERE);
            if (ImGui::Selectable("Cilindro")) addPrimitive(SHAPE_CYLINDER);
            if (ImGui::Selectable("Plano")) addPrimitive(SHAPE_PLANE);
            ImGui::Separator();
            ImGui::TextDisabled("Luz");
            if (ImGui::Selectable("Luz de ponto")) addPrimitive(SHAPE_LIGHT);
            ImGui::Separator();
            ImGui::TextDisabled("Modelos (%d)", (int)modelFiles.size());
            for (size_t i = 0; i < modelFiles.size(); i++) {
                ImGui::PushID((int)i);
                if (ImGui::Selectable(modelFiles[i].c_str())) addModel(modelFiles[i]);
                ImGui::PopID();
            }
            if (modelFiles.empty()) ImGui::TextDisabled("Coloque .obj/.glb em models/");
            ImGui::EndPopup();
        }
        ImGui::End();

        if (showPanel) panelUI(fl & ~ImGuiWindowFlags_NoScrollbar);
    }

    void panelUI(ImGuiWindowFlags fl) {
        ImGuiIO& io = ImGui::GetIO();
        bool landscape = W >= H;
        bool wantText = io.WantTextInput;
        float px, py, pw, ph;
        if (landscape) {
            pw = W * (panelTab == 2 ? 0.56f : 0.36f);
            px = W - pw; py = barH; ph = H - barH;
            if (wantText) ph = std::min(ph, H * 0.48f);
        } else {
            pw = (float)W; px = 0;
            if (wantText) { py = barH; ph = H * 0.42f; } else { ph = H * 0.5f; py = H - ph; }
        }
        ImGui::SetNextWindowPos(ImVec2(px, py));
        ImGui::SetNextWindowSize(ImVec2(pw, ph));
        ImGui::SetNextWindowBgAlpha(0.92f);
        ImGui::Begin("##panel", nullptr, fl);
        trackRect();
        if (ImGui::BeginTabBar("##tabs")) {
            if (ImGui::BeginTabItem("Cena")) { panelTab = 0; sceneTab(ph); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Ambiente")) { panelTab = 1; envTab(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Scripts")) { panelTab = 2; scriptsTab(ph); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Recursos")) { panelTab = 3; assetsTab(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Modelo 3D")) { panelTab = 5; modelEditorTab(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Console")) { panelTab = 4; consoleTab(); ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }
        ImGui::End();
    }

    bool clipCombo(const char* label, const Gltf* g, std::string& cur, bool allowNone) {
        bool changed = false;
        if (ImGui::BeginCombo(label, cur.empty() ? "(nenhuma)" : cur.c_str())) {
            if (allowNone && ImGui::Selectable("(nenhuma)", cur.empty())) { cur.clear(); changed = true; }
            for (auto& a : g->anims)
                if (ImGui::Selectable(a.name.c_str(), a.name == cur)) { cur = a.name; changed = true; }
            ImGui::EndCombo();
        }
        return changed;
    }

    void animUI(Object& o) {
        const Gltf* g = gltfOf(o);
        if (!g) {
            std::string e = rd.modelError(o.model);
            ImGui::TextWrapped("Modelo glTF/GLB não carregado. %s", e.c_str());
            return;
        }
        ImGui::Text("%d animações no arquivo:", (int)g->anims.size());
        for (auto& a : g->anims) ImGui::BulletText("%s (%.1fs)", a.name.c_str(), a.duration);
        if (g->anims.empty()) { ImGui::TextDisabled("Este modelo não tem animações."); return; }
        ImGui::Separator();
        ImGui::TextDisabled("Padrão: toca em loop (também na prévia do editor)");
        clipCombo("Padrão", g, o.animDefault, true);
        ImGui::SliderFloat("Velocidade", &o.animSpeedDef, 0.1f, 3.0f, "%.2f");
        ImGui::SliderFloat("Transição", &o.animBlendDef, 0.0f, 1.0f, "%.2f s");
        ImGui::Checkbox("Voltar ao padrão após evento", &o.animReturn);
        ImGui::Separator();
        ImGui::TextDisabled("Eventos de animação");
        static const char* typeNames[4] = {"Ao iniciar", "Ao colidir", "Após tempo", "Ao tocar"};
        int del = -1;
        for (size_t i = 0; i < o.triggers.size(); i++) {
            AnimTrigger& t = o.triggers[i];
            ImGui::PushID((int)i);
            ImGui::Separator();
            if (ImGui::BeginCombo("Quando", typeNames[std::max(0, std::min(3, t.type))])) {
                for (int k = 0; k < 4; k++) if (ImGui::Selectable(typeNames[k], t.type == k)) t.type = k;
                ImGui::EndCombo();
            }
            clipCombo("Tocar", g, t.clip, false);
            ImGui::SliderFloat("Atraso", &t.delay, 0.0f, 30.0f, "%.1f s");
            ImGui::SliderFloat("Veloc.", &t.speed, 0.1f, 3.0f, "%.2f");
            ImGui::Checkbox("Em loop", &t.loop);
            ImGui::SameLine();
            if (ImGui::Button("Remover")) del = (int)i;
            ImGui::PopID();
        }
        if (del >= 0) o.triggers.erase(o.triggers.begin() + del);
        if (ImGui::Button("+ Novo evento")) {
            AnimTrigger t;
            t.clip = g->anims[0].name;
            o.triggers.push_back(t);
        }
    }

    bool audioCombo(const char* label, std::string& cur) {
        bool changed = false;
        if (ImGui::BeginCombo(label, cur.empty() ? "(nenhum)" : cur.c_str())) {
            if (ImGui::Selectable("(nenhum)", cur.empty())) { cur.clear(); changed = true; }
            for (auto& f : audioFiles) if (ImGui::Selectable(f.c_str(), f == cur)) { cur = f; changed = true; }
            ImGui::EndCombo();
        }
        return changed;
    }

    void soundUI(Object& o) {
        ImGui::TextDisabled("Sons ligados a eventos. Arquivos: audio/ (wav ogg mp3 flac)");
        static const char* trg[4] = {"Ao iniciar", "Ao colidir", "Após tempo", "Ao tocar"};
        int del = -1;
        for (size_t i = 0; i < o.sounds.size(); i++) {
            SoundSlot& s = o.sounds[i];
            ImGui::PushID(2000 + (int)i);
            ImGui::Separator();
            audioCombo("Som", s.clip);
            if (ImGui::BeginCombo("Quando", trg[std::max(0, std::min(3, s.trigger))])) {
                for (int k = 0; k < 4; k++) if (ImGui::Selectable(trg[k], s.trigger == k)) s.trigger = k;
                ImGui::EndCombo();
            }
            if (ImGui::BeginCombo("Canal", kBusLabel[std::max(0, std::min(3, s.bus))])) {
                for (int k = 0; k < 4; k++) if (ImGui::Selectable(kBusLabel[k], s.bus == k)) s.bus = k;
                ImGui::EndCombo();
            }
            ImGui::SliderFloat("Volume", &s.volume, 0.0f, 2.0f, "%.2f");
            ImGui::SliderFloat("Tom", &s.pitch, 0.25f, 3.0f, "%.2f");
            ImGui::SliderFloat("Var. tom", &s.pitchVar, 0.0f, 0.5f, "%.2f");
            ImGui::SliderFloat("Var. vol.", &s.volVar, 0.0f, 0.5f, "%.2f");
            ImGui::SliderFloat("Atraso", &s.delay, 0.0f, 30.0f, "%.1f s");
            ImGui::SliderFloat("Fade in", &s.fadeIn, 0.0f, 10.0f, "%.1f s");
            ImGui::Checkbox("Loop", &s.loop);
            ImGui::SameLine();
            ImGui::Checkbox("3D (segue o objeto)", &s.spatial);
            if (s.spatial) {
                ImGui::SliderFloat("Dist. min", &s.minDist, 0.1f, 20.0f, "%.1f");
                ImGui::SliderFloat("Dist. máx", &s.maxDist, 1.0f, 200.0f, "%.0f");
            }
            if (ImGui::Button("Testar")) previewSound(s.clip, s.volume, kBusKey[std::max(0, std::min(3, s.bus))]);
            ImGui::SameLine();
            if (ImGui::Button("Parar")) { audio.stop(previewId); previewId = 0; }
            ImGui::SameLine();
            if (ImGui::Button("Remover")) del = (int)i;
            ImGui::PopID();
        }
        if (del >= 0) o.sounds.erase(o.sounds.begin() + del);
        if (ImGui::Button("+ Novo som")) o.sounds.push_back(SoundSlot());
    }

    void sceneTab(float ph) {
        ImGui::Text("Objetos (%d)", (int)scene.objects.size());
        ImGui::BeginChild("##lista", ImVec2(0, std::max(ph * 0.24f, 90.0f)), true);
        for (int i = 0; i < (int)scene.objects.size(); i++) {
            const Object& o = scene.objects[i];
            std::string label = (o.shape == SHAPE_LIGHT ? "(luz) " : (o.shape == SHAPE_MODEL ? "(modelo) " : "")) + o.name;
            ImGui::PushID(i);
            if (ImGui::Selectable(label.c_str(), selected == i)) selected = i;
            ImGui::PopID();
        }
        ImGui::EndChild();
        if (!hasSel()) { ImGui::TextDisabled("Toque em um objeto para editar."); return; }
        Object& o = scene.objects[selected];
        ImGui::Separator();
        ImGui::Text("Tipo: %s", kShapeName[o.shape]);
        ImGui::PushItemWidth(-ImGui::GetFontSize() * 3.4f);
        inputStr("Nome", o.name);
        if (ImGui::CollapsingHeader("Transformar", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat3("Pos", &o.pos.x, 0.05f);
            if (o.shape != SHAPE_LIGHT) {
                ImGui::DragFloat3("Rot", &o.rot.x, 1.0f);
                ImGui::DragFloat3("Esc", &o.scl.x, 0.02f, 0.01f, 100.0f);
            }
        }
        if (o.shape == SHAPE_LIGHT) {
            if (ImGui::CollapsingHeader("Luz", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::ColorEdit3("Cor", o.color, ImGuiColorEditFlags_NoInputs);
                ImGui::SliderFloat("Intens.", &o.intensity, 0.0f, 100.0f, "%.1f");
                ImGui::SliderFloat("Alcance", &o.range, 1.0f, 60.0f, "%.1f");
                ImGui::Checkbox("Ligada", &o.visible);
            }
            ImGui::PopItemWidth();
            return;
        }
        bool gl = o.shape == SHAPE_MODEL && Renderer::isGltfName(o.model);
        if (ImGui::CollapsingHeader("Material")) {
            if (gl) ImGui::Checkbox("Usar material do arquivo", &o.fileMaterial);
            ImGui::ColorEdit3("Cor", o.color, ImGuiColorEditFlags_NoInputs);
            if (!gl || !o.fileMaterial) {
                ImGui::SliderFloat("Metal", &o.metallic, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("Rugosid.", &o.roughness, 0.0f, 1.0f, "%.2f");
            }
            ImGui::ColorEdit3("Emissão", o.emissive, ImGuiColorEditFlags_NoInputs);
            ImGui::SliderFloat("Brilho", &o.emissiveStrength, 0.0f, 10.0f, "%.1f");
            if (ImGui::BeginCombo("Textura", o.texture.empty() ? "(nenhuma)" : o.texture.c_str())) {
                if (ImGui::Selectable("(nenhuma)", o.texture.empty())) o.texture.clear();
                for (auto& f : textureFiles) if (ImGui::Selectable(f.c_str(), f == o.texture)) o.texture = f;
                ImGui::EndCombo();
            }
            if (!o.texture.empty()) ImGui::DragFloat2("Repetir", o.tile, 0.05f, 0.01f, 100.0f);
        }
        if (o.shape == SHAPE_MODEL && ImGui::CollapsingHeader("Modelo")) {
            if (ImGui::BeginCombo("Arquivo", o.model.empty() ? "(nenhum)" : o.model.c_str())) {
                for (auto& f : modelFiles) if (ImGui::Selectable(f.c_str(), f == o.model)) o.model = f;
                ImGui::EndCombo();
            }
        }
        if (ImGui::CollapsingHeader("Física")) {
            ImGui::RadioButton("Nenhuma", &o.physics, PHYS_NONE); ImGui::SameLine();
            ImGui::RadioButton("Estática", &o.physics, PHYS_STATIC); ImGui::SameLine();
            ImGui::RadioButton("Dinâmica", &o.physics, PHYS_DYNAMIC);
            if (o.physics != PHYS_NONE) {
                ImGui::SliderFloat("Quique", &o.bounce, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("Atrito", &o.friction, 0.0f, 1.0f, "%.2f");
            }
            if (o.physics == PHYS_DYNAMIC) {
                ImGui::SliderFloat("Massa", &o.mass, 0.1f, 100.0f, "%.1f");
                ImGui::Checkbox("Afetado pela gravidade", &o.useGravity);
            }
            ImGui::TextDisabled("Colisor: caixa sem rotação (esfera para Esfera)");
        }
        if (gl && ImGui::CollapsingHeader("Animação")) animUI(o);
        if (ImGui::CollapsingHeader("Áudio")) soundUI(o);
        if (ImGui::CollapsingHeader("Comportamento")) {
            ImGui::Text("No Play:");
            ImGui::RadioButton("Nenhum", &o.behavior, BHV_NONE); ImGui::SameLine();
            ImGui::RadioButton("Girar", &o.behavior, BHV_SPIN); ImGui::SameLine();
            ImGui::RadioButton("Flutuar", &o.behavior, BHV_BOB);
            ImGui::Checkbox("Projeta sombra", &o.castShadow);
            ImGui::SameLine();
            ImGui::Checkbox("Visível", &o.visible);
        }
        ImGui::PopItemWidth();
    }

    void applyPreset(int k) {
        Env& e = scene.env;
        Env d;
        e = d;
        if (k == 0) return;  // dia
        if (k == 1) {  // entardecer
            e.sunYaw = 200; e.sunPitch = -9; setC3(e.sunColor, 1.0f, 0.55f, 0.25f); e.sunIntensity = 2.5f; e.ambient = 0.6f;
            setC3(e.skyTop, 0.25f, 0.30f, 0.55f); setC3(e.skyHorizon, 0.95f, 0.60f, 0.35f); setC3(e.ground, 0.2f, 0.15f, 0.12f);
            e.fog = 0.01f;
        } else if (k == 2) {  // noite
            e.sunYaw = 120; e.sunPitch = -35; setC3(e.sunColor, 0.5f, 0.6f, 1.0f); e.sunIntensity = 0.35f; e.ambient = 0.35f;
            setC3(e.skyTop, 0.02f, 0.03f, 0.08f); setC3(e.skyHorizon, 0.08f, 0.10f, 0.18f); setC3(e.ground, 0.03f, 0.03f, 0.04f);
            e.fog = 0.012f; e.exposure = 1.4f;
        } else if (k == 3) {  // nublado
            e.sunYaw = 60; e.sunPitch = -60; setC3(e.sunColor, 0.85f, 0.88f, 0.92f); e.sunIntensity = 1.2f; e.ambient = 1.2f;
            setC3(e.skyTop, 0.55f, 0.58f, 0.62f); setC3(e.skyHorizon, 0.72f, 0.74f, 0.77f); setC3(e.ground, 0.3f, 0.3f, 0.3f);
            e.fog = 0.012f;
        }
    }

    void envTab() {
        Env& e = scene.env;
        ImGui::PushItemWidth(-ImGui::GetFontSize() * 4.5f);
        if (ImGui::CollapsingHeader("Predefinições", ImGuiTreeNodeFlags_DefaultOpen)) {
            bool first = true;
            if (wrapBtn("Dia", first)) applyPreset(0);
            if (wrapBtn("Entardecer", first)) applyPreset(1);
            if (wrapBtn("Noite", first)) applyPreset(2);
            if (wrapBtn("Nublado", first)) applyPreset(3);
        }
        if (ImGui::CollapsingHeader("Sol e luz", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Azimute", &e.sunYaw, 0.0f, 360.0f, "%.0f");
            float elev = -e.sunPitch;
            if (ImGui::SliderFloat("Altura", &elev, -10.0f, 90.0f, "%.0f")) e.sunPitch = -elev;
            ImGui::ColorEdit3("Cor do sol", e.sunColor, ImGuiColorEditFlags_NoInputs);
            ImGui::SliderFloat("Intens. sol", &e.sunIntensity, 0.0f, 10.0f, "%.2f");
            ImGui::SliderFloat("Ambiente", &e.ambient, 0.0f, 3.0f, "%.2f");
        }
        if (ImGui::CollapsingHeader("Céu")) {
            ImGui::ColorEdit3("Topo", e.skyTop, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit3("Horizonte", e.skyHorizon, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit3("Chão (refl.)", e.ground, ImGuiColorEditFlags_NoInputs);
            ImGui::SliderFloat("Neblina", &e.fog, 0.0f, 0.1f, "%.3f");
        }
        if (ImGui::CollapsingHeader("Sombras")) {
            bool sh = e.shadows != 0;
            if (ImGui::Checkbox("Ativar sombras", &sh)) e.shadows = sh ? 1 : 0;
            ImGui::Text("Qualidade");
            bool f2 = true;
            const int sizes[4] = {512, 1024, 2048, 4096};
            for (int i = 0; i < 4; i++) {
                std::string l = std::to_string(sizes[i]);
                if (wrapBtn(l.c_str(), f2, e.shadowSize == sizes[i])) e.shadowSize = sizes[i];
            }
            ImGui::SliderFloat("Suavidade", &e.shadowSoft, 0.5f, 4.0f, "%.1f");
            ImGui::SliderFloat("Alcance", &e.shadowRange, 5.0f, 80.0f, "%.0f");
            ImGui::TextDisabled("Só o sol projeta sombra; luzes de ponto não.");
        }
        if (ImGui::CollapsingHeader("Música de fundo")) {
            audioCombo("Arquivo", e.music);
            ImGui::SliderFloat("Volume", &e.musicVolume, 0.0f, 1.5f, "%.2f");
            ImGui::SliderFloat("Fade in", &e.musicFade, 0.0f, 10.0f, "%.1f s");
            ImGui::Checkbox("Em loop", &e.musicLoop);
            if (ImGui::Button("Testar")) previewSound(e.music, e.musicVolume, "music");
            ImGui::SameLine();
            if (ImGui::Button("Parar")) { audio.stop(previewId); previewId = 0; }
            ImGui::TextDisabled("Toca ao dar Play. Arquivos em audio/");
        }
        if (ImGui::CollapsingHeader("Imagem")) {
            ImGui::SliderFloat("Exposição", &e.exposure, 0.2f, 3.0f, "%.2f");
            bool tm = e.tonemap != 0;
            if (ImGui::Checkbox("Tonemap (cinema)", &tm)) e.tonemap = tm ? 1 : 0;
        }
        ImGui::PopItemWidth();
    }

    void scriptsTab(float ph) {
        ImGui::TextDisabled("Pasta: %s/scripts", projDir.c_str());
        bool first = true;
        for (auto& f : scriptFiles) if (wrapBtn(f.c_str(), first, f == curScript)) openScript(f);
        if (wrapBtn("+ Novo", first)) openDialog(DLG_NEW_SCRIPT, "", "script");
        ImGui::Separator();
        first = true;
        if (wrapBtn("Salvar", first)) { saveScript(); scriptMsg = "Salvo"; }
        if (wrapBtn("Recarregar", first)) { scriptDirty = false; std::string c = curScript; curScript.clear(); refreshScripts(); openScript(c); scriptMsg = "Recarregado do arquivo"; }
        if (wrapBtn("Verificar", first)) {
            std::string err, nm = curScript.empty() ? "script" : curScript;
            bool ok;
            if (fsu::ext(nm) == "lua") ok = LuaVM::check(scriptText, nm, err);
            else { hx::Interp t; ok = t.check(scriptText, nm, err); }
            scriptMsg = ok ? "OK: sem erros de sintaxe" : err;
        }
        if (wrapBtn("Teclado", first)) kbToggle = true;
        bool canDel = !curScript.empty() && curScript != "main.lua" && curScript != "main.hx";
        if (!canDel) ImGui::BeginDisabled();
        if (wrapBtn("Excluir", first)) openDialog(DLG_DELETE_SCRIPT, curScript, "");
        if (!canDel) ImGui::EndDisabled();
        if (!scriptMsg.empty()) ImGui::TextWrapped("%s", scriptMsg.c_str());
        if (curScript.empty()) { ImGui::TextDisabled("Nenhum script aberto."); }
        else {
            ImGui::Text("%s%s", curScript.c_str(), scriptDirty ? " *" : "");
            if (fontMono) ImGui::PushFont(fontMono);
            if (ImGui::InputTextMultiline("##code", (char*)scriptText.c_str(), scriptText.capacity() + 1,
                                          ImVec2(-1, std::max(ph * 0.5f, 160.0f)),
                                          ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackResize, resizeCb, &scriptText))
                scriptDirty = true;
            if (fontMono) ImGui::PopFont();
        }
        if (ImGui::CollapsingHeader("Referência da API")) ImGui::TextWrapped("%s", kApiDoc);
    }

    void consoleTab() {
        if (ImGui::Button("Limpar")) console.clear();
        ImGui::SameLine();
        ImGui::TextDisabled("Mensagens de trace()/erros aparecem ao dar Play");
        ImGui::Separator();
        for (auto& c : console) {
            if (c.error) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", c.text.c_str());
            else ImGui::TextWrapped("%s", c.text.c_str());
        }
    }

    void assetsTab() {
        if (frameCount % 120 == 0) refreshAssets();
        if (ImGui::Button("Atualizar lista")) { refreshAssets(); rd.reloadAssets(); }
        if (ImGui::CollapsingHeader("Modelos", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextDisabled("%s/models  (.obj .glb .gltf)", projDir.c_str());
            for (size_t i = 0; i < modelFiles.size(); i++) {
                ImGui::PushID((int)i);
                ImGui::Text("%s", modelFiles[i].c_str());
                ImGui::SameLine();
                if (ImGui::Button("Adicionar")) addModel(modelFiles[i]);
                ImGui::PopID();
            }
            if (modelFiles.empty()) ImGui::TextDisabled("Nenhum modelo.");
        }
        if (ImGui::CollapsingHeader("Áudio")) {
            ImGui::TextDisabled("%s/audio  (.wav .mp3 .ogg .flac)", projDir.c_str());
            for (size_t i = 0; i < audioFiles.size(); i++) {
                ImGui::PushID(3000 + (int)i);
                ImGui::Text("%s", audioFiles[i].c_str());
                ImGui::SameLine();
                if (ImGui::Button("Ouvir")) previewSound(audioFiles[i], 1.0f, "sfx");
                ImGui::PopID();
            }
            if (audioFiles.empty()) ImGui::TextDisabled("Nenhum áudio.");
            if (ImGui::Button("Parar")) { audio.stop(previewId); previewId = 0; }
            ImGui::Separator();
            ImGui::Text("Mixer");
            ImGui::PushItemWidth(-ImGui::GetFontSize() * 4.0f);
            if (ImGui::SliderFloat("Geral", &mixMaster, 0.0f, 1.5f, "%.2f")) audio.setMasterVolume(mixMaster);
            for (int i = 0; i < 4; i++)
                if (ImGui::SliderFloat(kBusLabel[i], &mixBus[i], 0.0f, 1.5f, "%.2f")) audio.setBusVolume(kBusKey[i], mixBus[i]);
            ImGui::PopItemWidth();
        }
        if (ImGui::CollapsingHeader("Texturas", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextDisabled("%s/textures  (.png .jpg)", projDir.c_str());
            for (auto& f : textureFiles) ImGui::BulletText("%s", f.c_str());
            if (textureFiles.empty()) ImGui::TextDisabled("Nenhuma textura.");
            ImGui::TextDisabled("Aplique no inspetor: Cena > Material > Textura.");
        }
    }

    void modelEditorTab() {
        ImGui::TextWrapped("Editor nativo .h3dm: ossos, pecas primitivas e clipes com keyframes/easing.");
        ImGui::SetNextItemWidth(-1);
        char fileBuf[160] = {};
        std::snprintf(fileBuf, sizeof(fileBuf), "%s", modelEditorFile.c_str());
        if (ImGui::InputText("Arquivo .h3dm", fileBuf, sizeof(fileBuf))) modelEditorFile = fileBuf;
        if (ImGui::Button("Novo")) {
            modelEditor = H3dModel::makeDefault();
            modelEditorBone = 0; modelEditorClip = 0;
        }
        ImGui::SameLine();
        if (ImGui::Button("Salvar")) {
            fsu::mkdirs(projDir + "/models");
            std::string name = modelEditorFile;
            if (name.empty()) name = "modelo.h3dm";
            if (name.find(".h3dm") == std::string::npos) name += ".h3dm";
            if (modelEditor.save(projDir + "/models/" + name)) status = "Modelo salvo: models/" + name;
            else status = "Falha ao salvar modelo .h3dm";
        }
        ImGui::SameLine();
        if (ImGui::Button("Carregar")) {
            std::string name = modelEditorFile;
            if (name.find(".h3dm") == std::string::npos) name += ".h3dm";
            if (modelEditor.load(projDir + "/models/" + name)) {
                modelEditorBone = std::max(0, std::min(modelEditorBone, (int)modelEditor.bones.size()-1));
                status = "Modelo carregado: " + name;
            } else status = "Nao foi possivel carregar models/" + name;
        }
        if (!status.empty()) ImGui::TextWrapped("%s", status.c_str());
        ImGui::Separator();
        if (ImGui::BeginTabBar("##modeltabs")) {
            if (ImGui::BeginTabItem("Pecas")) {
                const char* shapes[] = {"Cubo", "Esfera", "Cilindro"};
                ImGui::Combo("Primitiva", &modelEditorShape, shapes, 3);
                if (ImGui::Button("Adicionar peca")) {
                    HPart part; part.name = "peca_" + std::to_string(modelEditor.parts.size()+1);
                    part.shape = modelEditorShape; part.bone = std::max(0, std::min(modelEditorBone, (int)modelEditor.bones.size()-1));
                    part.pos = Vec3(0, 0.5f, 0); modelEditor.parts.push_back(part);
                }
                for (size_t i=0; i<modelEditor.parts.size(); ++i) {
                    HPart& part = modelEditor.parts[i];
                    ImGui::PushID((int)i);
                    if (ImGui::CollapsingHeader(part.name.c_str())) {
                        char nm[96] = {}; std::snprintf(nm, sizeof(nm), "%s", part.name.c_str());
                        if (ImGui::InputText("Nome", nm, sizeof(nm))) part.name = nm;
                        ImGui::Combo("Forma", &part.shape, shapes, 3);
                        ImGui::SliderInt("Osso", &part.bone, 0, std::max(0,(int)modelEditor.bones.size()-1));
                        ImGui::DragFloat3("Posicao", &part.pos.x, 0.02f);
                        ImGui::DragFloat3("Rotacao", &part.rot.x, 1.0f);
                        ImGui::DragFloat3("Escala", &part.scl.x, 0.02f, 0.01f, 100.0f);
                        ImGui::ColorEdit3("Cor", part.color);
                        ImGui::SliderFloat("Metal", &part.metal, 0, 1);
                        ImGui::SliderFloat("Rugosidade", &part.rough, 0, 1);
                        if (ImGui::Button("Remover peca")) { modelEditor.parts.erase(modelEditor.parts.begin()+i); ImGui::PopID(); break; }
                    }
                    ImGui::PopID();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Ossos")) {
                if (ImGui::Button("Adicionar osso filho")) {
                    HBone b; b.name = "osso_" + std::to_string(modelEditor.bones.size()+1);
                    b.parent = std::max(0, std::min(modelEditorBone, (int)modelEditor.bones.size()-1));
                    modelEditor.bones.push_back(b); modelEditorBone = (int)modelEditor.bones.size()-1;
                }
                for (size_t i=0; i<modelEditor.bones.size(); ++i) {
                    HBone& b = modelEditor.bones[i];
                    ImGui::PushID(1000+(int)i);
                    if (ImGui::Selectable(b.name.c_str(), modelEditorBone==(int)i)) modelEditorBone=(int)i;
                    if (modelEditorBone==(int)i) {
                        char nm[96] = {}; std::snprintf(nm, sizeof(nm), "%s", b.name.c_str());
                        if (ImGui::InputText("Nome do osso", nm, sizeof(nm))) b.name=nm;
                        ImGui::SliderInt("Pai (-1 = raiz)", &b.parent, -1, std::max(-1,(int)modelEditor.bones.size()-1));
                        if (b.parent==(int)i) b.parent=-1;
                        ImGui::DragFloat3("Posicao##bone", &b.pos.x, 0.02f);
                        ImGui::DragFloat3("Rotacao##bone", &b.rot.x, 1.0f);
                        ImGui::DragFloat3("Escala##bone", &b.scl.x, 0.02f, 0.01f, 100.0f);
                    }
                    ImGui::PopID();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Animacao")) {
                if (ImGui::Button("Novo clipe")) {
                    HClip c; c.name = "animacao_" + std::to_string(modelEditor.clips.size()+1);
                    modelEditor.clips.push_back(c); modelEditorClip=(int)modelEditor.clips.size()-1;
                }
                for (size_t i=0; i<modelEditor.clips.size(); ++i) {
                    HClip& c=modelEditor.clips[i]; ImGui::PushID(2000+(int)i);
                    if (ImGui::Selectable(c.name.c_str(), modelEditorClip==(int)i)) modelEditorClip=(int)i;
                    ImGui::PopID();
                }
                if (!modelEditor.clips.empty()) {
                    modelEditorClip=std::max(0,std::min(modelEditorClip,(int)modelEditor.clips.size()-1));
                    HClip& c=modelEditor.clips[modelEditorClip];
                    char nm[96]={}; std::snprintf(nm,sizeof(nm),"%s",c.name.c_str());
                    if (ImGui::InputText("Nome do clipe",nm,sizeof(nm))) c.name=nm;
                    ImGui::DragFloat("Duracao (s)",&c.duration,0.05f,0.01f,600.0f);
                    ImGui::Checkbox("Loop",&c.loop);
                    static float keyTime=0;
                    ImGui::SliderFloat("Tempo do keyframe",&keyTime,0,std::max(0.01f,c.duration));
                    if (ImGui::Button("Criar keyframe para osso selecionado")) {
                        int bi=std::max(0,std::min(modelEditorBone,(int)modelEditor.bones.size()-1));
                        Vec3 kp,kr,ks; modelEditor.sampleBone(&c,bi,keyTime,kp,kr,ks);
                        modelEditor.setKey(c,bi,keyTime,kp,kr,ks);
                    }
                    if (ImGui::Button("Converter para glTF interno (validar)")) {
                        Gltf converted; std::string err;
                        status = modelEditor.toGltf(converted,err) ? "Conversao .h3dm -> glTF interno concluida" : "Conversao falhou: "+err;
                    }
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }

    void playUI() {
        ImGui::SetNextWindowPos(ImVec2(10, 10));
        ImGui::Begin("##play", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse);
        trackRect();
        if (ImGui::Button("Parar")) wantStop = true;
        ImGui::End();

        ImDrawList* dl = ImGui::GetForegroundDrawList();
        float R = stickRadius();
        for (auto& p : ptr) {
            if (!p.active || p.ui || p.sx >= W * 0.4f) continue;
            dl->AddCircle(ImVec2(p.sx, p.sy), R, IM_COL32(255, 255, 255, 120), 32, 3.0f);
            dl->AddCircleFilled(ImVec2(p.sx + stickX * R, p.sy + stickY * R), R * 0.35f, IM_COL32(255, 255, 255, 150));
        }
        // console no jogo (ultimas linhas)
        size_t n = console.size();
        size_t from = n > 6 ? n - 6 : 0;
        float y = H - 60.0f * dp;
        for (size_t i = from; i < n; i++) {
            dl->AddText(ImVec2(10.0f * dp, y), console[i].error ? IM_COL32(255, 110, 110, 255) : IM_COL32(255, 255, 255, 220), console[i].text.c_str());
            y += 16.0f * dp;
        }
    }

    // ================= render =================
    void render() {
        if (screen == SCR_MENU) {
            glViewport(0, 0, W, H);
            glClearColor(0.05f, 0.06f, 0.09f, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            return;
        }
        View v;
        camBasis(v.eye, v.fwd, v.right, v.up);
        v.vp = viewProj();
        v.tanY = std::tan(30.0f * kDeg2Rad);
        v.aspect = (float)W / (float)(H > 0 ? H : 1);
        v.W = W; v.H = H;
        v.focus = playing ? v.eye + v.fwd * 8.0f : target;
        rd.renderScene(playing ? play : scene, playing ? playEnv : scene.env, v, playing ? -1 : selected, !playing, !playing);
    }
};
