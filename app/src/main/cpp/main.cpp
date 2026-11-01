// h3d - engine 3D para Android (NativeActivity + OpenGL ES 3.0 + Dear ImGui)
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <jni.h>
#include <android/log.h>
#include <android/configuration.h>
#include <android/asset_manager.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>
#include <chrono>
#include <string>
#include <cstring>

#include "imgui.h"
#include "backends/imgui_impl_android.h"
#include "backends/imgui_impl_opengl3.h"
#include "app.h"

#ifndef EGL_OPENGL_ES3_BIT
#define EGL_OPENGL_ES3_BIT 0x0040
#endif

struct Ctx {
    android_app* app = nullptr;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    bool ready = false;
    bool kb = false;
    bool kbWant = false;
    App game;
};

static bool initGL(Ctx* c) {
    c->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(c->display, nullptr, nullptr);
    const EGLint attribsMS[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_SAMPLE_BUFFERS, 1, EGL_SAMPLES, 4,
        EGL_NONE};
    const EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_NONE};
    EGLConfig config;
    EGLint n = 0;
    eglChooseConfig(c->display, attribsMS, &config, 1, &n);
    if (n < 1) eglChooseConfig(c->display, attribs, &config, 1, &n);
    if (n < 1) { LOGI("sem config EGL ES3"); return false; }
    EGLint fmt;
    eglGetConfigAttrib(c->display, config, EGL_NATIVE_VISUAL_ID, &fmt);
    ANativeWindow_setBuffersGeometry(c->app->window, 0, 0, fmt);
    c->surface = eglCreateWindowSurface(c->display, config, c->app->window, nullptr);
    const EGLint ctxAttr[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    c->context = eglCreateContext(c->display, config, EGL_NO_CONTEXT, ctxAttr);
    if (eglMakeCurrent(c->display, c->surface, c->surface, c->context) == EGL_FALSE) return false;
    eglSwapInterval(c->display, 1);

    ImGui_ImplAndroid_Init(c->app->window);
    ImGui_ImplOpenGL3_Init("#version 300 es");
    if (!c->game.rd.init()) return false;
    c->ready = true;
    return true;
}

static void termGL(Ctx* c) {
    if (c->ready) {
        c->game.rd.shutdown();
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplAndroid_Shutdown();
    }
    c->ready = false;
    if (c->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(c->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (c->context != EGL_NO_CONTEXT) eglDestroyContext(c->display, c->context);
        if (c->surface != EGL_NO_SURFACE) eglDestroySurface(c->display, c->surface);
        eglTerminate(c->display);
    }
    c->display = EGL_NO_DISPLAY;
    c->context = EGL_NO_CONTEXT;
    c->surface = EGL_NO_SURFACE;
    c->kb = false;
    c->kbWant = false;
    c->game.cancel();
}

static void onCmd(android_app* app, int32_t cmd) {
    Ctx* c = (Ctx*)app->userData;
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (app->window) initGL(c);
            c->game.audio.setPaused(false);
            break;
        case APP_CMD_TERM_WINDOW:
            c->game.audio.setPaused(true);
            termGL(c);
            break;
        default:
            break;
    }
}

// Converte um evento de tecla em caractere Unicode (via android.view.KeyEvent)
static int unicodeFromKey(android_app* app, int32_t action, int32_t keyCode, int32_t meta) {
    JavaVM* vm = app->activity->vm;
    JNIEnv* env = nullptr;
    bool attached = false;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return 0;
        attached = true;
    }
    int u = 0;
    jclass cls = env->FindClass("android/view/KeyEvent");
    if (cls) {
        jmethodID ctor = env->GetMethodID(cls, "<init>", "(II)V");
        jmethodID getU = env->GetMethodID(cls, "getUnicodeChar", "(I)I");
        if (ctor && getU) {
            jobject ev = env->NewObject(cls, ctor, (jint)action, (jint)keyCode);
            if (ev) {
                u = (int)env->CallIntMethod(ev, getU, (jint)meta);
                env->DeleteLocalRef(ev);
            }
        }
        env->DeleteLocalRef(cls);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (attached) vm->DetachCurrentThread();
    return u;
}

// Mostra/esconde o teclado virtual via InputMethodManager (mais confiavel que o NDK em tela cheia)
static void jniKeyboard(android_app* app, bool show) {
    JavaVM* vm = app->activity->vm;
    JNIEnv* env = nullptr;
    bool attached = false;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
        attached = true;
    }
    bool done = false;
    jobject act = app->activity->clazz;
    jclass actCls = env->GetObjectClass(act);
    jmethodID getSS = env->GetMethodID(actCls, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    if (getSS) {
        jstring name = env->NewStringUTF("input_method");
        jobject imm = env->CallObjectMethod(act, getSS, name);
        env->DeleteLocalRef(name);
        if (imm && !env->ExceptionCheck()) {
            jclass immCls = env->GetObjectClass(imm);
            if (show) {
                jmethodID tog = env->GetMethodID(immCls, "toggleSoftInput", "(II)V");
                if (tog) { env->CallVoidMethod(imm, tog, (jint)2, (jint)0); done = !env->ExceptionCheck(); }
            } else {
                jmethodID getWin = env->GetMethodID(actCls, "getWindow", "()Landroid/view/Window;");
                jobject win = getWin ? env->CallObjectMethod(act, getWin) : nullptr;
                if (win) {
                    jclass winCls = env->GetObjectClass(win);
                    jmethodID getDecor = env->GetMethodID(winCls, "getDecorView", "()Landroid/view/View;");
                    jobject decor = getDecor ? env->CallObjectMethod(win, getDecor) : nullptr;
                    if (decor) {
                        jclass viewCls = env->GetObjectClass(decor);
                        jmethodID getTok = env->GetMethodID(viewCls, "getWindowToken", "()Landroid/os/IBinder;");
                        jobject tok = getTok ? env->CallObjectMethod(decor, getTok) : nullptr;
                        jmethodID hide = env->GetMethodID(immCls, "hideSoftInputFromWindow", "(Landroid/os/IBinder;I)Z");
                        if (tok && hide) { env->CallBooleanMethod(imm, hide, tok, (jint)0); done = !env->ExceptionCheck(); }
                    }
                }
            }
        }
    }
    if (env->ExceptionCheck()) { env->ExceptionClear(); done = false; }
    if (attached) vm->DetachCurrentThread();
    if (!done) {   // plano B: API do NDK
        if (show) ANativeActivity_showSoftInput(app->activity, ANATIVEACTIVITY_SHOW_SOFT_INPUT_FORCED);
        else ANativeActivity_hideSoftInput(app->activity, 0);
    }
}

static int32_t onInput(android_app* app, AInputEvent* e) {
    Ctx* c = (Ctx*)app->userData;
    if (!c->ready) return 0;
    int type = AInputEvent_getType(e);
    if (type == AINPUT_EVENT_TYPE_KEY) {
        int32_t code = AKeyEvent_getKeyCode(e);
        if (code == AKEYCODE_BACK || code == AKEYCODE_VOLUME_UP || code == AKEYCODE_VOLUME_DOWN) return 0;
        ImGui_ImplAndroid_HandleInputEvent(e);
        if (AKeyEvent_getAction(e) == AKEY_EVENT_ACTION_DOWN) {
            int u = unicodeFromKey(app, AKEY_EVENT_ACTION_DOWN, code, AKeyEvent_getMetaState(e));
            if (u >= 32 && u != 127) ImGui::GetIO().AddInputCharacter((unsigned int)u);
            else if (code == AKEYCODE_ENTER) { /* tratado pelo ImGui como tecla */ }
        }
        return 1;
    }
    if (type != AINPUT_EVENT_TYPE_MOTION) return 0;
    ImGui_ImplAndroid_HandleInputEvent(e);

    int32_t action = AMotionEvent_getAction(e);
    int act = action & AMOTION_EVENT_ACTION_MASK;
    int idx = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    switch (act) {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN:
            c->game.pDown(AMotionEvent_getPointerId(e, idx), AMotionEvent_getX(e, idx), AMotionEvent_getY(e, idx));
            break;
        case AMOTION_EVENT_ACTION_MOVE: {
            size_t n = AMotionEvent_getPointerCount(e);
            for (size_t i = 0; i < n; i++)
                c->game.pMove(AMotionEvent_getPointerId(e, i), AMotionEvent_getX(e, i), AMotionEvent_getY(e, i));
            c->game.endMove();
            break;
        }
        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP:
            c->game.pUp(AMotionEvent_getPointerId(e, idx), AMotionEvent_getX(e, idx), AMotionEvent_getY(e, idx));
            break;
        case AMOTION_EVENT_ACTION_CANCEL:
            c->game.cancel();
            break;
        default:
            break;
    }
    return 1;
}

static void setupStyle(float scale) {
    ImGuiStyle& st = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    float u = scale * 0.8f;
    st.WindowRounding = 8.0f * u;
    st.ChildRounding = 6.0f * u;
    st.FrameRounding = 6.0f * u;
    st.PopupRounding = 6.0f * u;
    st.ScrollbarRounding = 6.0f * u;
    st.GrabRounding = 6.0f * u;
    st.TabRounding = 6.0f * u;
    st.WindowPadding = ImVec2(8.0f * u, 6.0f * u);
    st.FramePadding = ImVec2(8.0f * u, 4.0f * u);
    st.ItemSpacing = ImVec2(6.0f * u, 5.0f * u);
    st.ItemInnerSpacing = ImVec2(5.0f * u, 4.0f * u);
    st.ScrollbarSize = 14.0f * u;
    st.GrabMinSize = 14.0f * u;
    st.IndentSpacing = 12.0f * u;
    st.WindowBorderSize = 1.0f;
    st.FrameBorderSize = 0.0f;
    ImVec4* c = st.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.075f, 0.085f, 0.11f, 0.96f);
    c[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.10f, 0.13f, 0.92f);
    c[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.11f, 0.15f, 0.98f);
    c[ImGuiCol_Border] = ImVec4(0.25f, 0.29f, 0.37f, 0.65f);
    c[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.16f, 0.21f, 1.0f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.24f, 0.33f, 1.0f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.30f, 0.43f, 1.0f);
    c[ImGuiCol_Button] = ImVec4(0.16f, 0.20f, 0.28f, 1.0f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.31f, 0.45f, 1.0f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.17f, 0.38f, 0.64f, 1.0f);
    c[ImGuiCol_Header] = ImVec4(0.16f, 0.25f, 0.38f, 1.0f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.20f, 0.34f, 0.52f, 1.0f);
    c[ImGuiCol_Tab] = ImVec4(0.13f, 0.17f, 0.24f, 1.0f);
    c[ImGuiCol_TabHovered] = ImVec4(0.22f, 0.36f, 0.56f, 1.0f);
    c[ImGuiCol_TabActive] = ImVec4(0.18f, 0.32f, 0.52f, 1.0f);
    c[ImGuiCol_CheckMark] = ImVec4(0.40f, 0.68f, 0.98f, 1.0f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.32f, 0.58f, 0.88f, 1.0f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.45f, 0.72f, 1.0f, 1.0f);
}

static ImFont* addFontFromAsset(android_app* state, ImGuiIO& io, const char* path, float px) {
    AAsset* a = AAssetManager_open(state->activity->assetManager, path, AASSET_MODE_BUFFER);
    if (!a) return nullptr;
    ImFont* f = nullptr;
    const void* data = AAsset_getBuffer(a);
    const off_t len = AAsset_getLength(a);
    if (data && len > 0) {
        void* copy = IM_ALLOC((size_t)len);
        if (copy) {
            std::memcpy(copy, data, (size_t)len);
            ImFontConfig fc;
            fc.FontDataOwnedByAtlas = true;
            f = io.Fonts->AddFontFromMemoryTTF(copy, (int)len, px, &fc);
        }
    }
    AAsset_close(a);
    return f;
}

void android_main(android_app* state) {
    Ctx ctx;
    ctx.app = state;
    state->userData = &ctx;
    state->onAppCmd = onCmd;
    state->onInputEvent = onInput;

    float scale = 2.5f;
    if (state->config) {
        int dens = AConfiguration_getDensity(state->config);
        if (dens > 0 && dens < 1000) scale = dens / 160.0f;
    }
    if (scale < 1.0f) scale = 1.0f;
    if (scale > 4.0f) scale = 4.0f;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    setupStyle(scale);

    // UI compacta: Roboto (empacotada nos assets) em tamanho pequeno
    ImFont* sans = addFontFromAsset(state, io, "fonts/Roboto-Regular.ttf", 11.5f * scale);
    if (!sans) {
        ImFontConfig fc;
        fc.SizePixels = 11.0f * scale;
        io.Fonts->AddFontDefault(&fc);
    }
    ImFont* mono = nullptr;
    if (fsu::exists("/system/fonts/DroidSansMono.ttf")) {
        mono = io.Fonts->AddFontFromFileTTF("/system/fonts/DroidSansMono.ttf", 11.0f * scale);
    }
    ctx.game.fontMono = mono;

    // pasta de projetos: visivel em gerenciadores de arquivos (Android/data/<pacote>/files)
    std::string base = state->activity->externalDataPath ? state->activity->externalDataPath : state->activity->internalDataPath;
    ctx.game.init(base, scale);

    auto last = std::chrono::steady_clock::now();
    while (true) {
        int events;
        android_poll_source* source;
        int timeout = ctx.ready ? 0 : -1;
        while (ALooper_pollOnce(timeout, nullptr, &events, (void**)&source) >= 0) {
            if (source) source->process(state, source);
            if (state->destroyRequested) {
                termGL(&ctx);
                ImGui::DestroyContext();
                return;
            }
            timeout = ctx.ready ? 0 : -1;
        }
        if (state->destroyRequested) {
            termGL(&ctx);
            ImGui::DestroyContext();
            return;
        }
        if (!ctx.ready) continue;

        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        if (dt > 0.1f) dt = 0.1f;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplAndroid_NewFrame();
        ImGui::NewFrame();
        ctx.game.update(dt);
        ctx.game.ui();
        ImGui::Render();

        ctx.game.render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        eglSwapBuffers(ctx.display, ctx.surface);

        // teclado virtual: abre quando um campo de texto fica ativo; fecha quando sai;
        // o botao "Teclado" da interface alterna manualmente
        bool want = ImGui::GetIO().WantTextInput;
        if (ctx.game.kbToggle) {
            ctx.game.kbToggle = false;
            ctx.kb = !ctx.kb;
            jniKeyboard(state, ctx.kb);
        } else if (want != ctx.kbWant) {
            ctx.kbWant = want;
            if (want != ctx.kb) { ctx.kb = want; jniKeyboard(state, want); }
        }
    }
}
