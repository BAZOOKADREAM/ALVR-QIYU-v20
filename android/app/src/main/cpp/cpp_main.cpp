// ALVR client for QIYU 3 / QIYU Dream (Snapdragon XR platform).
// Ported from the verified ALVR v19.1.1 QIYU fork to the ALVR v20.x client_core C API.
// Rendering and tracking are driven by the Qiyu Native SDK (VrApi-compatible API).

// #include "VrApi.h"
#include "VrApi_Helpers.h"
#include "VrApi_Input.h"
#include "alvr_client_core.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <vector>
#include <time.h>
#include "QiyuApi.h"
#include "Hand_Trajectory_Prediction/Hand_Trajectory_Prediction.h"
#include "Jerk_Estimation/Jerk_Estimation.h"
#include "QYRenderTarget.h"

 /**
  * Foveation
  * https://www.khronos.org/registry/OpenGL/extensions/QCOM/QCOM_framebuffer_foveated.txt
  * https://www.khronos.org/registry/OpenGL/extensions/QCOM/QCOM_texture_foveated.txt
  * https://www.khronos.org/registry/OpenGL/extensions/QCOM/QCOM_texture_foveated2.txt
  * https://www.khronos.org/registry/OpenGL/extensions/QCOM/QCOM_texture_foveated_subsampled_layout.txt
  */
#ifndef GL_EXT_framebuffer_foveated

#define GL_FOVEATION_ENABLE_BIT_QCOM                    0x0001
#define GL_FOVEATION_SCALED_BIN_METHOD_BIT_QCOM         0x0002
#define GL_FOVEATION_SUBSAMPLED_LAYOUT_METHOD_BIT_QCOM  0x0004
//#define GL_TEXTURE_PREVIOUS_SOURCE_TEXTURE_QCOM         0x8BE8  //
//#define GL_TEXTURE_FOVEATED_FRAME_OFFSET_QCOM           0x8BE9  //
#define GL_TEXTURE_FOVEATED_FEATURE_BITS_QCOM           0x8BFB
#define GL_TEXTURE_FOVEATED_MIN_PIXEL_DENSITY_QCOM      0x8BFC
//#define GL_TEXTURE_FOVEATED_FEATURE_QUERY_QCOM          0x8BFD
//#define GL_TEXTURE_FOVEATED_NUM_FOCAL_POINTS_QUERY_QCOM 0x8BFE
//#define GL_FRAMEBUFFER_INCOMPLETE_FOVEATION_QCOM        0x8BFF
//#define GL_MAX_SHADER_SUBSAMPLED_IMAGE_UNITS_QCOM       0x8FA1

#ifdef GL_GLEXT_PROTOTYPES
//GL_APICALL void GL_APIENTRY glFramebufferFoveationConfigQCOM(GLuint fbo, GLuint numLayers, GLuint focalPointsPerLayer, GLuint requiredFeatures, GLuint* gotFeatures);
//GL_APICALL void GL_APIENTRY glFramebufferFoveationParametersQCOM(GLuint fbo, GLuint layer, GLuint focalPoint, GLfloat focalX, GLfloat focalY, GLfloat gainX, GLfloat gainY, GLfloat foveaArea);
GL_APICALL void GL_APIENTRY glTextureFoveationParametersQCOM(GLuint texure, GLuint layer, GLuint focalPoint, GLfloat focalX, GLfloat focalY, GLfloat gainX, GLfloat gainY, GLfloat foveaArea);
#endif

#define GL_APIENTRYP GL_APIENTRY*
typedef void (GL_APIENTRYP PFNGLTEXTUREFOVEATIONPARAMETERSEXT)(GLuint texture, GLuint layer, GLuint focalPoint, GLfloat focalX, GLfloat focalY, GLfloat gainX, GLfloat gainY, GLfloat foveaArea);
PFNGLTEXTUREFOVEATIONPARAMETERSEXT glTextureFoveationParametersQCOM = NULL;
//
//typedef void (GL_APIENTRYP PFNGLFRAMEBUFFERFOVEATIONCONFIGEXT)(GLuint fbo, GLuint numLayers, GLuint focalPointsPerLayer, GLuint requiredFeatures, GLuint* gotFeatures);
//PFNGLFRAMEBUFFERFOVEATIONCONFIGEXT glFramebufferFoveationConfigQCOM = NULL;
//
//typedef void (GL_APIENTRYP PFNGLFRAMEBUFFERFOVEATIONPARAMETERSEXT)(GLuint fbo, GLuint layer, GLuint focalPoint, GLfloat focalX, GLfloat focalY, GLfloat gainX, GLfloat gainY, GLfloat foveaArea);
//PFNGLFRAMEBUFFERFOVEATIONPARAMETERSEXT glFramebufferFoveationParametersQCOM = NULL;
//bool glTextureFoveationFrameOffsetQCOM = false;

#endif//GL_EXT_framebuffer_foveated

#define NUM_EYE_BUFFERS_     3

void log(AlvrLogLevel level, const char *format, ...) {
    va_list args;
    va_start(args, format);

    char buf[1024];
    int count = vsnprintf(buf, sizeof(buf), format, args);
    if (count > (int) sizeof(buf))
        count = (int) sizeof(buf);
    if (count > 0 && buf[count - 1] == '\n')
        buf[count - 1] = '\0';

    alvr_log(level, buf);

    va_end(args);
}

#define error(...) log(ALVR_LOG_LEVEL_ERROR, __VA_ARGS__)
#define info(...) log(ALVR_LOG_LEVEL_INFO, __VA_ARGS__)

namespace QY_GL_EXT
{
	bool InitFunction_Foveation()
	{
		glTextureFoveationParametersQCOM == NULL;
		glTextureFoveationParametersQCOM = (PFNGLTEXTUREFOVEATIONPARAMETERSEXT)eglGetProcAddress("glTextureFoveationParametersQCOM");
		if (glTextureFoveationParametersQCOM == NULL)
		{
			error("@@QY_GL_EXT::InitFunction_Foveation, glTextureFoveationParametersQCOM is not supported, fail to get proc address!");
			return false;
		}
		return true;
	}
	bool IsSupport_Foveation()
	{
		return glTextureFoveationParametersQCOM != NULL;
	}
}

uint64_t HEAD_ID = alvr_path_string_to_id("/user/head");
uint64_t LEFT_HAND_ID = alvr_path_string_to_id("/user/hand/left");
uint64_t RIGHT_HAND_ID = alvr_path_string_to_id("/user/hand/right");
uint64_t LEFT_CONTROLLER_HAPTICS_ID = alvr_path_string_to_id("/user/hand/left/output/haptic");
uint64_t RIGHT_CONTROLLER_HAPTICS_ID = alvr_path_string_to_id("/user/hand/right/output/haptic");

uint64_t MENU_CLICK_ID = alvr_path_string_to_id("/user/hand/left/input/menu/click");
uint64_t A_CLICK_ID = alvr_path_string_to_id("/user/hand/right/input/a/click");
uint64_t A_TOUCH_ID = alvr_path_string_to_id("/user/hand/right/input/a/touch");
uint64_t B_CLICK_ID = alvr_path_string_to_id("/user/hand/right/input/b/click");
uint64_t B_TOUCH_ID = alvr_path_string_to_id("/user/hand/right/input/b/touch");
uint64_t X_CLICK_ID = alvr_path_string_to_id("/user/hand/left/input/x/click");
uint64_t X_TOUCH_ID = alvr_path_string_to_id("/user/hand/left/input/x/touch");
uint64_t Y_CLICK_ID = alvr_path_string_to_id("/user/hand/left/input/y/click");
uint64_t Y_TOUCH_ID = alvr_path_string_to_id("/user/hand/left/input/y/touch");
uint64_t LEFT_SQUEEZE_CLICK_ID = alvr_path_string_to_id("/user/hand/left/input/squeeze/click");
uint64_t LEFT_SQUEEZE_VALUE_ID = alvr_path_string_to_id("/user/hand/left/input/squeeze/value");
uint64_t LEFT_TRIGGER_CLICK_ID = alvr_path_string_to_id("/user/hand/left/input/trigger/click");
uint64_t LEFT_TRIGGER_VALUE_ID = alvr_path_string_to_id("/user/hand/left/input/trigger/value");
uint64_t LEFT_TRIGGER_TOUCH_ID = alvr_path_string_to_id("/user/hand/left/input/trigger/touch");
uint64_t LEFT_THUMBSTICK_X_ID = alvr_path_string_to_id("/user/hand/left/input/thumbstick/x");
uint64_t LEFT_THUMBSTICK_Y_ID = alvr_path_string_to_id("/user/hand/left/input/thumbstick/y");
uint64_t LEFT_THUMBSTICK_CLICK_ID = alvr_path_string_to_id(
        "/user/hand/left/input/thumbstick/click");
uint64_t LEFT_THUMBSTICK_TOUCH_ID = alvr_path_string_to_id(
        "/user/hand/left/input/thumbstick/touch");
uint64_t LEFT_THUMBREST_TOUCH_ID = alvr_path_string_to_id(
        "/user/hand/left/input/thumbrest/touch");
uint64_t RIGHT_SQUEEZE_CLICK_ID = alvr_path_string_to_id("/user/hand/right/input/squeeze/click");
uint64_t RIGHT_SQUEEZE_VALUE_ID = alvr_path_string_to_id("/user/hand/right/input/squeeze/value");
uint64_t RIGHT_TRIGGER_CLICK_ID = alvr_path_string_to_id("/user/hand/right/input/trigger/click");
uint64_t RIGHT_TRIGGER_VALUE_ID = alvr_path_string_to_id("/user/hand/right/input/trigger/value");
uint64_t RIGHT_TRIGGER_TOUCH_ID = alvr_path_string_to_id("/user/hand/right/input/trigger/touch");
uint64_t RIGHT_THUMBSTICK_X_ID = alvr_path_string_to_id("/user/hand/right/input/thumbstick/x");
uint64_t RIGHT_THUMBSTICK_Y_ID = alvr_path_string_to_id("/user/hand/right/input/thumbstick/y");
uint64_t RIGHT_THUMBSTICK_CLICK_ID = alvr_path_string_to_id(
        "/user/hand/right/input/thumbstick/click");
uint64_t RIGHT_THUMBSTICK_TOUCH_ID = alvr_path_string_to_id(
        "/user/hand/right/input/thumbstick/touch");
uint64_t RIGHT_THUMBREST_TOUCH_ID = alvr_path_string_to_id(
        "/user/hand/right/input/thumbrest/touch");

const int MAXIMUM_TRACKING_FRAMES = 360;
// minimum change for a scalar button to be registered as a new value
const float BUTTON_EPS = 0.001;
const float IPD_EPS = 0.001; // minimum change of IPD to be registered as a new value

const GLenum SWAPCHAIN_FORMAT = GL_RGBA8;

static float g_fTrackingOffset = 0.f;//FIXME!

static Jerk_Estimation leftHandJerkEstimation[3];
static Jerk_Estimation rightHandJerkEstimation[3];

struct Render_EGL {
    EGLDisplay Display;
    EGLConfig Config;
    EGLSurface TinySurface;
    EGLSurface MainSurface;
    EGLContext Context;
};

struct QYEyeBuffer {
    QYRenderTarget eyeTarget[NUM_EYE_BUFFERS_];
    int index;
};

class NativeContext {
public:
    JavaVM *vm;
    jobject context;

    Render_EGL egl;

    ANativeWindow *window = nullptr;
    ovrMobile *ovrContext{};

    bool running = false;
    bool streaming = false;
    std::thread eventsThread;

    uint32_t recommendedViewWidth = 1;
    uint32_t recommendedViewHeight = 1;
    uint32_t streamViewWidth = 1;
    uint32_t streamViewHeight = 1;
    float refreshRate = 72.f;
    bool enableFoveatedEncoding = false;
    bool enableHdr = false;

    uint64_t ovrFrameIndex = 0;
    uint64_t lastFrameTimeUs = 0;

    std::deque<std::pair<uint64_t, qiyu_HeadPoseState>> trackingFrameMap;
    std::mutex trackingFrameMutex;

    QYEyeBuffer lobbyBuffers[2] = {};
    QYEyeBuffer streamBuffers[2] = {};

    uint8_t hmdBattery = 0;
    bool hmdPlugged = false;
    uint8_t lastLeftControllerBattery = 0;
    uint8_t lastRightControllerBattery = 0;

    float lastIpd = 0.f;
    AlvrFov lastFov = {};

    std::map<uint64_t, AlvrButtonValue> previousButtonsState;

    struct HapticsState {
        uint64_t startUs;
        uint64_t endUs;
        float amplitude;
        float frequency;
        bool fresh;
        bool buffered;
    };
    HapticsState hapticsState[2]{};
};

NativeContext CTX = {};

static const char *EglErrorString(const EGLint err) {
    switch (err) {
        case EGL_SUCCESS:
            return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED:
            return "EGL_NOT_INITIALIZED";
        case EGL_BAD_ACCESS:
            return "EGL_BAD_ACCESS";
        case EGL_BAD_ALLOC:
            return "EGL_BAD_ALLOC";
        case EGL_BAD_ATTRIBUTE:
            return "EGL_BAD_ATTRIBUTE";
        case EGL_BAD_CONTEXT:
            return "EGL_BAD_CONTEXT";
        case EGL_BAD_CONFIG:
            return "EGL_BAD_CONFIG";
        case EGL_BAD_CURRENT_SURFACE:
            return "EGL_BAD_CURRENT_SURFACE";
        case EGL_BAD_DISPLAY:
            return "EGL_BAD_DISPLAY";
        case EGL_BAD_SURFACE:
            return "EGL_BAD_SURFACE";
        case EGL_BAD_MATCH:
            return "EGL_BAD_MATCH";
        case EGL_BAD_PARAMETER:
            return "EGL_BAD_PARAMETER";
        case EGL_BAD_NATIVE_PIXMAP:
            return "EGL_BAD_NATIVE_PIXMAP";
        case EGL_BAD_NATIVE_WINDOW:
            return "EGL_BAD_NATIVE_WINDOW";
        case EGL_CONTEXT_LOST:
            return "EGL_CONTEXT_LOST";
        default:
            return "unknown";
    }
}

void eglInit() {
    EGLint major, minor;

    CTX.egl.Display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(CTX.egl.Display, &major, &minor);

    // Do NOT use eglChooseConfig, because the Android EGL code pushes in multisample
    // flags in eglChooseConfig if the user has selected the "force 4x MSAA" option in
    // settings, and that is completely wasted for our warp target.
    const int MAX_CONFIGS = 1024;
    EGLConfig configs[MAX_CONFIGS];
    EGLint numConfigs = 0;
    if (eglGetConfigs(CTX.egl.Display, configs, MAX_CONFIGS, &numConfigs) == EGL_FALSE) {
        error("        eglGetConfigs() failed: %s", EglErrorString(eglGetError()));
        return;
    }
    const EGLint configAttribs[] = {EGL_RED_SIZE,
                                    8,
                                    EGL_GREEN_SIZE,
                                    8,
                                    EGL_BLUE_SIZE,
                                    8,
                                    EGL_ALPHA_SIZE,
                                    8, // need alpha for the multi-pass timewarp compositor
                                    EGL_DEPTH_SIZE,
                                    0,
                                    EGL_STENCIL_SIZE,
                                    0,
                                    EGL_SAMPLES,
                                    0,
                                    EGL_NONE};
    CTX.egl.Config = 0;
    for (int i = 0; i < numConfigs; i++) {
        EGLint value = 0;

        eglGetConfigAttrib(CTX.egl.Display, configs[i], EGL_RENDERABLE_TYPE, &value);
        if ((value & EGL_OPENGL_ES3_BIT_KHR) != EGL_OPENGL_ES3_BIT_KHR) {
            continue;
        }

        // The pbuffer config also needs to be compatible with normal window rendering
        // so it can share textures with the window context.
        eglGetConfigAttrib(CTX.egl.Display, configs[i], EGL_SURFACE_TYPE, &value);
        if ((value & (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) != (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) {
            continue;
        }

        int j = 0;
        for (; configAttribs[j] != EGL_NONE; j += 2) {
            eglGetConfigAttrib(CTX.egl.Display, configs[i], configAttribs[j], &value);
            if (value != configAttribs[j + 1]) {
                break;
            }
        }
        if (configAttribs[j] == EGL_NONE) {
            CTX.egl.Config = configs[i];
            break;
        }
    }
    if (CTX.egl.Config == 0) {
        error("        eglChooseConfig() failed: %s", EglErrorString(eglGetError()));
        return;
    }
    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    CTX.egl.Context = eglCreateContext(CTX.egl.Display, CTX.egl.Config, EGL_NO_CONTEXT,
                                       contextAttribs);
    if (CTX.egl.Context == EGL_NO_CONTEXT) {
        error("        eglCreateContext() failed: %s", EglErrorString(eglGetError()));
        return;
    }
    const EGLint surfaceAttribs[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
    CTX.egl.TinySurface = eglCreatePbufferSurface(CTX.egl.Display, CTX.egl.Config, surfaceAttribs);
    if (CTX.egl.TinySurface == EGL_NO_SURFACE) {
        error("        eglCreatePbufferSurface() failed: %s", EglErrorString(eglGetError()));
        eglDestroyContext(CTX.egl.Display, CTX.egl.Context);
        CTX.egl.Context = EGL_NO_CONTEXT;
        return;
    }
    if (eglMakeCurrent(CTX.egl.Display, CTX.egl.TinySurface, CTX.egl.TinySurface,
                       CTX.egl.Context) == EGL_FALSE) {
        error("        eglMakeCurrent() failed: %s", EglErrorString(eglGetError()));
        eglDestroySurface(CTX.egl.Display, CTX.egl.TinySurface);
        eglDestroyContext(CTX.egl.Display, CTX.egl.Context);
        CTX.egl.Context = EGL_NO_CONTEXT;
        return;
    }
}

void eglDestroy() {
    if (CTX.egl.Display != 0) {
        error("        eglMakeCurrent( Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT )");
        if (eglMakeCurrent(CTX.egl.Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) ==
            EGL_FALSE) {
            error("        eglMakeCurrent() failed: %s", EglErrorString(eglGetError()));
        }
    }
    if (CTX.egl.Context != EGL_NO_CONTEXT) {
        error("        eglDestroyContext( Display, Context )");
        if (eglDestroyContext(CTX.egl.Display, CTX.egl.Context) == EGL_FALSE) {
            error("        eglDestroyContext() failed: %s", EglErrorString(eglGetError()));
        }
        CTX.egl.Context = EGL_NO_CONTEXT;
    }
    if (CTX.egl.TinySurface != EGL_NO_SURFACE) {
        error("        eglDestroySurface( Display, TinySurface )");
        if (eglDestroySurface(CTX.egl.Display, CTX.egl.TinySurface) == EGL_FALSE) {
            error("        eglDestroySurface() failed: %s", EglErrorString(eglGetError()));
        }
        CTX.egl.TinySurface = EGL_NO_SURFACE;
    }
    if (CTX.egl.Display != 0) {
        error("        eglTerminate( Display )");
        if (eglTerminate(CTX.egl.Display) == EGL_FALSE) {
            error("        eglTerminate() failed: %s", EglErrorString(eglGetError()));
        }
        CTX.egl.Display = 0;
    }
}

inline uint64_t getTimestampUs() {
    timeval tv;
    gettimeofday(&tv, nullptr);

    uint64_t Current = (uint64_t) tv.tv_sec * 1000 * 1000 + tv.tv_usec;
    return Current;
}

inline uint64_t getTimestampNs() {
    timespec ts;
    clock_gettime(CLOCK_BOOTTIME, &ts);

    return (uint64_t) ts.tv_sec * 1e9 + ts.tv_nsec;
}

ovrJava getOvrJava(bool initThread = false) {
    JNIEnv *env;
    if (initThread) {
        JavaVMAttachArgs args = {JNI_VERSION_1_6};
        CTX.vm->AttachCurrentThread(&env, &args);
    } else {
        CTX.vm->GetEnv((void **) &env, JNI_VERSION_1_6);
    }

    ovrJava java{};
    java.Vm = CTX.vm;
    java.Env = env;
    java.ActivityObject = CTX.context;

    return java;
}

void updateBinary(uint64_t path, uint32_t flag) {
    auto value = flag != 0;
    auto *stateRef = &CTX.previousButtonsState[path];
    if (stateRef->tag != ALVR_BUTTON_VALUE_BINARY || stateRef->binary != value) {
        stateRef->tag = ALVR_BUTTON_VALUE_BINARY;
        stateRef->binary = value;

        alvr_send_button(path, *stateRef);
    }
}

void updateScalar(uint64_t path, float value) {
    auto *stateRef = &CTX.previousButtonsState[path];
    if (stateRef->tag != ALVR_BUTTON_VALUE_SCALAR ||
        std::fabs(stateRef->scalar - value) > BUTTON_EPS) {
        stateRef->tag = ALVR_BUTTON_VALUE_SCALAR;
        stateRef->scalar = value;

        alvr_send_button(path, *stateRef);
    }
}

void updateButtons() {
    if(!qiyu_IsControllerInit()) {
        return;
    }

    qiyu_ControllerData left;
    qiyu_ControllerData right;

    qiyu_GetControllerData(&left, &right);

    if (left.isConnect) {
        updateBinary(MENU_CLICK_ID, left.button & BT_Home_Menu);
        updateBinary(X_CLICK_ID, left.button & BT_A_X);
        updateBinary(X_TOUCH_ID, left.buttonTouch & BT_A_X);
        updateBinary(Y_CLICK_ID, left.button & BT_B_Y);
        updateBinary(Y_TOUCH_ID, left.buttonTouch & BT_B_Y);
        updateBinary(LEFT_SQUEEZE_CLICK_ID, left.button & BT_Grip);
        updateScalar(LEFT_SQUEEZE_VALUE_ID, left.gripForce);
        updateBinary(LEFT_TRIGGER_CLICK_ID, left.button & BT_Trigger);
        updateScalar(LEFT_TRIGGER_VALUE_ID, left.triggerForce);
        updateBinary(LEFT_TRIGGER_TOUCH_ID, left.buttonTouch & BT_Trigger);
        updateScalar(LEFT_THUMBSTICK_X_ID, left.joyStickPos.x);
        updateScalar(LEFT_THUMBSTICK_Y_ID, left.joyStickPos.y);
        updateBinary(LEFT_THUMBSTICK_CLICK_ID, left.button & BT_JoyStick);
        updateBinary(LEFT_THUMBSTICK_TOUCH_ID, left.buttonTouch & BT_JoyStick);
        updateBinary(LEFT_THUMBREST_TOUCH_ID, left.buttonTouch & BT_None);
    }

    if (right.isConnect) {
        updateBinary(A_CLICK_ID, right.button & BT_A_X);
        updateBinary(A_TOUCH_ID, right.buttonTouch & BT_A_X);
        updateBinary(B_CLICK_ID, right.button & BT_B_Y);
        updateBinary(B_TOUCH_ID, right.buttonTouch & BT_B_Y);
        updateBinary(RIGHT_SQUEEZE_CLICK_ID, right.button & BT_Grip);
        updateScalar(RIGHT_SQUEEZE_VALUE_ID, right.gripForce);
        updateBinary(RIGHT_TRIGGER_CLICK_ID, right.button & BT_Trigger);
        updateScalar(RIGHT_TRIGGER_VALUE_ID, right.triggerForce);
        updateBinary(RIGHT_TRIGGER_TOUCH_ID, right.buttonTouch & BT_Trigger);
        updateScalar(RIGHT_THUMBSTICK_X_ID, right.joyStickPos.x);
        updateScalar(RIGHT_THUMBSTICK_Y_ID, right.joyStickPos.y);
        updateBinary(RIGHT_THUMBSTICK_CLICK_ID, right.button & BT_JoyStick);
        updateBinary(RIGHT_THUMBSTICK_TOUCH_ID, right.buttonTouch & BT_JoyStick);
        updateBinary(RIGHT_THUMBREST_TOUCH_ID, right.buttonTouch & BT_None);
    }
}

// return fov in OpenXR convention
AlvrFov getFov(qiyu_DeviceInfo* di, int eye) {
    // ovrTracking2 tracking = vrapi_GetPredictedTracking2(CTX.ovrContext, 0.0);

    qiyu_ViewFrustum* pFrust;

    if (eye == 0) {
        pFrust = &di->frustumLeftEye;
    } else {
        pFrust = &di->frustumRightEye;
    }

    AlvrFov fov;

    fov.left = (float) atan(pFrust->left / pFrust->near);
    fov.right = (float) atan(pFrust->right / pFrust->near);
    fov.up = (float) atan(pFrust->top / pFrust->near);
    fov.down = (float) atan(pFrust->bottom / pFrust->near);

    return fov;
}

static inline float getInterpupillaryDistance(qiyu_DeviceInfo* di) {
    qiyu_Vector3 delta;
    delta.x = di->frustumRightEye.position.x - di->frustumLeftEye.position.x;
    delta.y = di->frustumRightEye.position.y - di->frustumLeftEye.position.y;
    delta.z = di->frustumRightEye.position.z - di->frustumLeftEye.position.z;
    return sqrtf(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
}

// View params are expressed in head-local space, as if the head is at the origin. QIYU uses a
// left-handed coordinate system while ALVR expects a right-handed one, so mirror the eye poses
// using the same transform as the head pose.
std::array<AlvrViewParams, 2> getViewParams(qiyu_DeviceInfo* di) {
    std::array<AlvrViewParams, 2> viewParams = {};

    for (int eye = 0; eye < 2; eye++) {
        qiyu_ViewFrustum* pFrust =
            eye == 0 ? &di->frustumLeftEye : &di->frustumRightEye;

        viewParams[eye].pose.orientation = AlvrQuat{
            pFrust->rotation.x,
            pFrust->rotation.y,
            pFrust->rotation.z,
            -pFrust->rotation.w,
        };
        viewParams[eye].pose.position[0] = -pFrust->position.x;
        viewParams[eye].pose.position[1] = -pFrust->position.y;
        viewParams[eye].pose.position[2] = -pFrust->position.z;
        viewParams[eye].fov = getFov(di, eye);
    }

    return viewParams;
}

void getPlayspaceArea(float *width, float *height) {
    qiyu_Vector3 bboxScale;
    // Theoretically pose (the 2nd parameter) could be nullptr, since we already have that, but
    // then this function gives us 0-size bounding box, so it has to be provided.
    bboxScale = qiyu_GetBoundaryDimensions();
    *width = 2.0f * bboxScale.x;
    *height = 2.0f * bboxScale.z;
}

uint8_t getControllerBattery(int index) {
    if(!qiyu_IsControllerInit()) {
        return 0;
    }

    qiyu_ControllerData left;
    qiyu_ControllerData right;

    qiyu_GetControllerData(&left, &right);

    if (index == 0) {
        return (uint8_t) left.batteryLevel;
    } else {
        return (uint8_t) right.batteryLevel;
    }
}

auto& finishHapticsBuffer = qiyu_StopControllerVibration;

void updateHapticsState() {
    qiyu_ControllerMask curCaps;

    if(!qiyu_IsControllerInit()) {
        return;
    }

    for (uint32_t deviceIndex = 0;
         deviceIndex < CI_COUNT;
         deviceIndex++) {

        curCaps = deviceIndex ? qiyu_ControllerMask::CM_Right : qiyu_ControllerMask::CM_Left;

        int curHandIndex =
                deviceIndex ? 0 : 1;
        auto &s = CTX.hapticsState[curHandIndex];

        uint64_t currentUs = getTimestampUs();

        if (s.fresh) {
            s.startUs = s.startUs + currentUs;
            s.endUs = s.startUs + s.endUs;
            s.fresh = false;
        }

        if (s.startUs <= 0) {
            // No requested haptics for this hand.
            if (s.buffered) {
                finishHapticsBuffer(curCaps);
                s.buffered = false;
            }
            continue;
        }

        if (currentUs >= s.endUs) {
            // No more haptics is needed.
            s.startUs = 0;
            if (s.buffered) {
                finishHapticsBuffer(curCaps);
                s.buffered = false;
            }
            continue;
        }

        qiyu_StartControllerVibration(
            curCaps, s.amplitude, (float) (s.endUs - currentUs) / 1e6);
        s.buffered = true;
    }
}

// low frequency events.
// This thread gets created after the creation of ovrContext and before its destruction
void eventsThread() {
    auto java = getOvrJava(true);

    jclass cls = java.Env->GetObjectClass(java.ActivityObject);
    jmethodID onStreamStartMethod = java.Env->GetMethodID(cls, "onStreamStart", "()V");
    jmethodID onStreamStopMethod = java.Env->GetMethodID(cls, "onStreamStop", "()V");

    auto deadline = std::chrono::steady_clock::now();
    auto motionVec = std::vector<AlvrDeviceMotion>();

    while (CTX.running) {
        if (CTX.streaming) {
            motionVec.clear();

            AlvrDeviceMotion headMotion = {};
            uint64_t predictionOffsetNs = alvr_get_prediction_offset_ns();
            uint64_t targetTimestampNs = getTimestampNs() + predictionOffsetNs;
            auto headTracking =
                    qiyu_PredictHeadPose((float) predictionOffsetNs / 1e6);
            headMotion.device_id = HEAD_ID;
            headMotion.pose.orientation.x = headTracking.pose.rotation.x;
            headMotion.pose.orientation.y = headTracking.pose.rotation.y;
            headMotion.pose.orientation.z = headTracking.pose.rotation.z;
            headMotion.pose.orientation.w = -headTracking.pose.rotation.w;
            headMotion.pose.position[0] = -headTracking.pose.position.x;
            headMotion.pose.position[1] = -headTracking.pose.position.y - g_fTrackingOffset;
            headMotion.pose.position[2] = -headTracking.pose.position.z;
            // Note: do not copy velocities. Avoid reprojection in SteamVR
            motionVec.push_back(headMotion);

            {
                std::lock_guard<std::mutex> lock(CTX.trackingFrameMutex);
                // Insert from the front: it will be searched first
                CTX.trackingFrameMap.push_front({targetTimestampNs, headTracking});
                if (CTX.trackingFrameMap.size() > MAXIMUM_TRACKING_FRAMES) {
                    CTX.trackingFrameMap.pop_back();
                }
            }

            updateButtons();

            double controllerDisplayTimeS =
                    (double) predictionOffsetNs / 1e9 * 1.0f;

            if(qiyu_IsControllerInit()) {
                qiyu_ControllerData left;
                qiyu_ControllerData right;

                qiyu_GetControllerData(&left, &right);

                // From left-handed to right-handed
                left.velocity.z = -left.velocity.z;
                right.velocity.z = -right.velocity.z;
                left.acceleration.z = -left.acceleration.z;
                right.acceleration.z = -right.acceleration.z;
                left.angVelocity.z = -left.angVelocity.z;
                right.angVelocity.z = -right.angVelocity.z;
                left.angAcceleration.z = -left.angAcceleration.z;
                right.angAcceleration.z = -right.angAcceleration.z;

                if (left.isConnect) {
                    handPositionf leftHand;
                    float predictedPosition[3];
                    for (int i = 0; i < 3; i++) {
                        leftHand.Position = *(&left.position.x + i);
                        leftHand.LinearVelocity = *(&left.velocity.x + i);
                        leftHand.LinearAcceleration = *(&left.acceleration.x + i);
                        leftHandJerkEstimation[i].rtU.acceleration = leftHand.LinearAcceleration;
                        leftHandJerkEstimation[i].rtU.tor = 1.0 / CTX.refreshRate / 3;
                        leftHandJerkEstimation[i].step();
                        leftHand.LinearJerk = leftHandJerkEstimation[i].rtY.jerk;
                        leftHand.LinearSnap = 0.f;
                        leftHand.LinearCrackle = 0.f;
                        predictedPosition[i] = handTrajectoryPrediction(leftHand, controllerDisplayTimeS);
                    }

                    AlvrDeviceMotion motion = {};
                    motion.device_id = LEFT_HAND_ID;
                    memcpy(&motion.pose.orientation, &left.rotation, 4 * 4);
                    memcpy(motion.pose.position, predictedPosition, 4 * 3);
                    memcpy(motion.linear_velocity, &left.velocity, 4 * 3);
                    memcpy(motion.angular_velocity, &left.angVelocity, 4 * 3);
                    motion.pose.position[1] -= g_fTrackingOffset;

                    motionVec.push_back(motion);
                }

                if (right.isConnect) {
                    handPositionf rightHand;
                    float predictedPosition[3];
                    for (int i = 0; i < 3; i++) {
                        rightHand.Position = *(&right.position.x + i);
                        rightHand.LinearVelocity = *(&right.velocity.x + i);
                        rightHand.LinearAcceleration = *(&right.acceleration.x + i);
                        rightHandJerkEstimation[i].rtU.acceleration = rightHand.LinearAcceleration;
                        rightHandJerkEstimation[i].rtU.tor = 1.0 / CTX.refreshRate / 3;
                        rightHandJerkEstimation[i].step();
                        rightHand.LinearJerk = rightHandJerkEstimation[i].rtY.jerk;
                        rightHand.LinearSnap = 0.f;
                        rightHand.LinearCrackle = 0.f;
                        predictedPosition[i] = handTrajectoryPrediction(rightHand, controllerDisplayTimeS);
                    }

                    AlvrDeviceMotion motion = {};
                    motion.device_id = RIGHT_HAND_ID;
                    memcpy(&motion.pose.orientation, &right.rotation, 4 * 4);
                    memcpy(motion.pose.position, predictedPosition, 4 * 3);
                    memcpy(motion.linear_velocity, &right.velocity, 4 * 3);
                    memcpy(motion.angular_velocity, &right.angVelocity, 4 * 3);
                    motion.pose.position[1] -= g_fTrackingOffset;

                    motionVec.push_back(motion);
                }
            }

            // QIYU does not provide a hand skeleton or eye gaze APIs, pass null.
            alvr_send_tracking(targetTimestampNs, motionVec.data(), motionVec.size(), nullptr,
                               nullptr);
        }

        qiyu_DeviceInfo di = qiyu_GetDeviceInfo();
        auto viewParams = getViewParams(&di);
        float newIpd = getInterpupillaryDistance(&di);

        if (std::fabs(newIpd - CTX.lastIpd) > IPD_EPS ||
            std::fabs(viewParams[0].fov.left - CTX.lastFov.left) > IPD_EPS) {
            alvr_send_view_params(viewParams.data());
            CTX.lastIpd = newIpd;
            CTX.lastFov = viewParams[0].fov;
        }

        uint8_t leftBattery = getControllerBattery(0);
        if (leftBattery != CTX.lastLeftControllerBattery) {
            alvr_send_battery(LEFT_HAND_ID, (float) leftBattery / 100.f, false);
            CTX.lastLeftControllerBattery = leftBattery;
        }
        uint8_t rightBattery = getControllerBattery(1);
        if (rightBattery != CTX.lastRightControllerBattery) {
            alvr_send_battery(RIGHT_HAND_ID, (float) rightBattery / 100.f, false);
            CTX.lastRightControllerBattery = rightBattery;
        }

        AlvrEvent event;
        while (alvr_poll_event(&event)) {
            if (event.tag == ALVR_EVENT_HAPTICS) {
                auto haptics = event.HAPTICS;
                int curHandIndex = (haptics.device_id == RIGHT_CONTROLLER_HAPTICS_ID ? 0 : 1);
                auto &s = CTX.hapticsState[curHandIndex];
                s.startUs = 0;
                s.endUs = (uint64_t) (haptics.duration_s * 1000'000);
                s.amplitude = (haptics.amplitude > 0.2) ? haptics.amplitude : 0.2;
                s.frequency = haptics.frequency;
                s.fresh = true;
                s.buffered = false;
            } else if (event.tag == ALVR_EVENT_STREAMING_STARTED) {
                CTX.streamViewWidth = event.STREAMING_STARTED.view_width;
                CTX.streamViewHeight = event.STREAMING_STARTED.view_height;
                if (event.STREAMING_STARTED.refresh_rate_hint > 0.f) {
                    CTX.refreshRate = event.STREAMING_STARTED.refresh_rate_hint;
                }
                CTX.enableFoveatedEncoding = event.STREAMING_STARTED.enable_foveated_encoding;
                CTX.enableHdr = event.STREAMING_STARTED.enable_hdr;
                java.Env->CallVoidMethod(java.ActivityObject, onStreamStartMethod);
            } else if (event.tag == ALVR_EVENT_STREAMING_STOPPED) {
                java.Env->CallVoidMethod(java.ActivityObject, onStreamStopMethod);
            } else if (event.tag == ALVR_EVENT_DECODER_CONFIG) {
                alvr_create_decoder_auto(event.DECODER_CONFIG.codec);
            } else if (event.tag == ALVR_EVENT_HUD_MESSAGE_UPDATED) {
                auto messageLength = alvr_hud_message(nullptr);
                if (messageLength > 0) {
                    auto messageBuffer = std::vector<char>(messageLength);
                    alvr_hud_message(messageBuffer.data());
                    alvr_update_hud_message_opengl(messageBuffer.data());
                }
            }
        }

        deadline += std::chrono::nanoseconds((uint64_t) (1e9 / CTX.refreshRate / 3));
        std::this_thread::sleep_until(deadline);
    }
}

static void CreateLayout_(float centerX, float centerY, float radiusX, float radiusY, qiyu_RenderLayer_ScreenPosUV* pLayout)//FIXME! //TODO!
{
	// This is always in screen space so we want Z = 0 and W = 1
	float lowerLeftPos[4] = { centerX - radiusX, centerY - radiusY, 0.0f, 1.0f };
	float lowerRightPos[4] = { centerX + radiusX, centerY - radiusY, 0.0f, 1.0f };
	float upperLeftPos[4] = { centerX - radiusX, centerY + radiusY, 0.0f, 1.0f };
	float upperRightPos[4] = { centerX + radiusX, centerY + radiusY, 0.0f, 1.0f };
	float lowerUVs[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
	float upperUVs[4] = { 0.0f, 1.0f, 1.0f, 1.0f };
	memcpy(pLayout->LowerLeftPos, lowerLeftPos, sizeof(lowerLeftPos));
	memcpy(pLayout->LowerRightPos, lowerRightPos, sizeof(lowerRightPos));
	memcpy(pLayout->UpperLeftPos, upperLeftPos, sizeof(upperLeftPos));
	memcpy(pLayout->UpperRightPos, upperRightPos, sizeof(upperRightPos));
	memcpy(pLayout->LowerUVs, lowerUVs, sizeof(lowerUVs));
	memcpy(pLayout->UpperUVs, upperUVs, sizeof(upperUVs));
}

extern "C" JNIEXPORT void JNICALL
Java_alvr_client_VRActivity_initializeNative(JNIEnv *env, jobject context) {
    env->GetJavaVM(&CTX.vm);
    CTX.context = env->NewGlobalRef(context);

    auto java = getOvrJava(true);

    eglInit();

    memset(CTX.hapticsState, 0, sizeof(CTX.hapticsState));
    QY_GL_EXT::InitFunction_Foveation();
    qiyu_Init(java.ActivityObject,
			  java.Vm,
			  qiyu_GraphicsApi::GA_OpenGLES,
			  qiyu_TrackingOriginMode::TM_Ground,
			  false);

    qiyu_DeviceInfo deviceInfo = qiyu_GetDeviceInfo();
    CTX.recommendedViewWidth =
            deviceInfo.iEyeTargetWidth;
    CTX.recommendedViewHeight =
            deviceInfo.iEyeTargetHeight;

    int refreshRatesCount =
            2;
    auto refreshRatesBuffer = std::vector<float>(refreshRatesCount);
    refreshRatesBuffer[0] = 72.f;
    refreshRatesBuffer[1] = 90.f;

    alvr_initialize_logging();
    alvr_initialize_android_context((void *) CTX.vm, (void *) CTX.context);

    AlvrClientCapabilities capabilities = {};
    capabilities.default_view_width = CTX.recommendedViewWidth;
    capabilities.default_view_height = CTX.recommendedViewHeight;
    capabilities.refresh_rates = refreshRatesBuffer.data();
    capabilities.refresh_rates_count = refreshRatesCount;
    capabilities.foveated_encoding = false;
    capabilities.encoder_high_profile = true;
    capabilities.encoder_10_bits = true;
    capabilities.encoder_av1 = false;
    capabilities.prefer_10bit = false;
    capabilities.prefer_full_range = false;
    capabilities.preferred_encoding_gamma = 2.2f;
    capabilities.prefer_hdr = false;
    alvr_initialize(capabilities);

    // Ask for microphone permission, mirroring the behavior of the stock ALVR client.
    alvr_try_get_permission("android.permission.RECORD_AUDIO");

    alvr_initialize_opengl();
    qiyu_PostSetEyeBufferSize(CTX.recommendedViewWidth, CTX.recommendedViewHeight);
}

extern "C" JNIEXPORT void JNICALL
Java_alvr_client_VRActivity_destroyNative(JNIEnv *_env, jobject _context) {
    qiyu_Release();

    alvr_destroy();
    alvr_destroy_opengl();

    eglDestroy();

    auto java = getOvrJava();
    java.Env->DeleteGlobalRef(CTX.context);
}

extern "C" JNIEXPORT void JNICALL Java_alvr_client_VRActivity_onResumeNative(
        JNIEnv *_env, jobject _context, jobject surface) {
    auto java = getOvrJava();

    CTX.window = ANativeWindow_fromSurface(java.Env, surface);

    info("Entering VR mode.");

    if (!qiyu_StartVR(CTX.window, PL_System, PL_System)) {
        error("Invalid ANativeWindow");
    }

    qiyu_SetTrackingOriginMode(qiyu_TrackingOriginMode::TM_Ground);

    std::vector<uint32_t> textureHandlesBuffer[2];
    for (int eye = 0; eye < 2; eye++) {
        for (int index = 0; index < NUM_EYE_BUFFERS_; index++) {
            CTX.lobbyBuffers[eye].eyeTarget[index].Init(
                false, GL_FOVEATION_ENABLE_BIT_QCOM | GL_FOVEATION_SCALED_BIN_METHOD_BIT_QCOM, false,
                CTX.recommendedViewWidth, CTX.recommendedViewHeight, 1, GL_RGBA8, false, false);
            auto handle = CTX.lobbyBuffers[eye].eyeTarget[index].GetColorAttachment();
            textureHandlesBuffer[eye].push_back(handle);
        }
        CTX.lobbyBuffers[eye].index = 0;
    }
    const uint32_t *textureHandles[2] = {&textureHandlesBuffer[0][0], &textureHandlesBuffer[1][0]};
    qiyu_PostSetEyeBufferSize(CTX.recommendedViewWidth, CTX.recommendedViewHeight);

    CTX.running = true;
    CTX.eventsThread = std::thread(eventsThread);

    alvr_resume_opengl(CTX.recommendedViewWidth, CTX.recommendedViewHeight, textureHandles,
                       textureHandlesBuffer[0].size());
    alvr_resume();
}

extern "C" JNIEXPORT void JNICALL
Java_alvr_client_VRActivity_onStreamStartNative(JNIEnv *_env, jobject _context) {
    auto java = getOvrJava();

    std::vector<uint32_t> textureHandlesBuffer[2];
    for (int eye = 0; eye < 2; eye++) {
        for (int index = 0; index < NUM_EYE_BUFFERS_; index++) {
            CTX.streamBuffers[eye].eyeTarget[index].Init(
                false, GL_FOVEATION_ENABLE_BIT_QCOM | GL_FOVEATION_SCALED_BIN_METHOD_BIT_QCOM, false,
                CTX.streamViewWidth, CTX.streamViewHeight, 1, GL_RGBA8, false, false);
            auto handle = CTX.streamBuffers[eye].eyeTarget[index].GetColorAttachment();
            textureHandlesBuffer[eye].push_back(handle);
        }

        CTX.streamBuffers[eye].index = 0;
    }
    const uint32_t *textureHandles[2] = {&textureHandlesBuffer[0][0], &textureHandlesBuffer[1][0]};
    qiyu_PostSetEyeBufferSize(CTX.streamViewWidth, CTX.streamViewHeight);

    // Hardware (display-side) foveation follows the negotiated foveated encoding state.
    // Render-target level foveation is disabled (see QYRenderTarget::Init calls above).
    if (CTX.enableFoveatedEncoding) {
        qiyu_SetFoveation(FL_High);
    } else {
        qiyu_SetFoveation(FL_None);
    }

    qiyu_DeviceInfo di = qiyu_GetDeviceInfo();
    auto viewParams = getViewParams(&di);
    alvr_send_view_params(viewParams.data());

    alvr_send_battery(HEAD_ID, CTX.hmdBattery, CTX.hmdPlugged);
    alvr_send_battery(LEFT_HAND_ID, getControllerBattery(0) / 100.f, false);
    alvr_send_battery(RIGHT_HAND_ID, getControllerBattery(1) / 100.f, false);

    float areaWidth, areaHeight;
    getPlayspaceArea(&areaWidth, &areaHeight);
    alvr_send_playspace(areaWidth, areaHeight);

    AlvrStreamConfig streamConfig = {};
    streamConfig.view_resolution_width = CTX.streamViewWidth;
    streamConfig.view_resolution_height = CTX.streamViewHeight;
    streamConfig.swapchain_textures = textureHandles;
    streamConfig.swapchain_length = textureHandlesBuffer[0].size();
    streamConfig.enable_foveation = false;
    streamConfig.enable_upscaling = false;

    alvr_start_stream_opengl(streamConfig);

    CTX.streaming = true;
}

extern "C" JNIEXPORT void JNICALL
Java_alvr_client_VRActivity_onStreamStopNative(JNIEnv *_env, jobject _context) {
    CTX.streaming = false;

    alvr_destroy_decoder();

    for (int eye = 0; eye < 2; eye++) {
        for (int index = 0; index < NUM_EYE_BUFFERS_; index++) {
            CTX.streamBuffers[eye].eyeTarget[index].Release();
        }
    }
}

extern "C" JNIEXPORT void JNICALL
Java_alvr_client_VRActivity_onPauseNative(JNIEnv *_env, jobject _context) {
    Java_alvr_client_VRActivity_onStreamStopNative(_env, _context);

    alvr_pause();
    alvr_pause_opengl();

    if (CTX.running) {
        CTX.running = false;
        CTX.eventsThread.join();
    }
    for (int eye = 0; eye < 2; eye++) {
        for (int index = 0; index < NUM_EYE_BUFFERS_; index++) {
            CTX.lobbyBuffers[eye].eyeTarget[index].Release();
        }
    }

    qiyu_EndVR();

    CTX.ovrContext = nullptr;

    if (CTX.window != nullptr) {
        ANativeWindow_release(CTX.window);
    }
    CTX.window = nullptr;
}

extern "C" JNIEXPORT void JNICALL
Java_alvr_client_VRActivity_renderNative(JNIEnv *_env, jobject _context) {
    qiyu_HeadPoseState tracking;
    qiyu_FrameParam frameParam;
    memset(&frameParam, 0, sizeof(frameParam));

    uint64_t currentUs = getTimestampUs();
    float tickSecond = (currentUs - CTX.lastFrameTimeUs) / 1000000.0;
    qiyu_Update(tickSecond);
    CTX.lastFrameTimeUs = currentUs;

    if (CTX.streaming) {
        void *streamHardwareBuffer = nullptr;
        uint64_t timestampNs = 0;
        if (!alvr_get_frame(&timestampNs, &streamHardwareBuffer)) {
            return;
        }
        // Fetch the view parameters negotiated with the server. The pose is unused by the C API
        // stream renderer (only FoV and reprojection rotation are consumed), the FoV may have
        // been adjusted by the server for canting.
        AlvrViewParams viewParams[2] = {};
        alvr_report_compositor_start(timestampNs, viewParams);
        if (viewParams[0].fov.left == 0.f && viewParams[0].fov.right == 0.f &&
            viewParams[0].fov.up == 0.f && viewParams[0].fov.down == 0.f) {
            qiyu_DeviceInfo di = qiyu_GetDeviceInfo();
            viewParams[0].fov = getFov(&di, 0);
            viewParams[1].fov = getFov(&di, 1);
        }

        updateHapticsState();

        {
            std::lock_guard<std::mutex> lock(CTX.trackingFrameMutex);

            // Take the frame with equal timestamp, or the next closest one.
            for (auto &pair: CTX.trackingFrameMap) {
                if (pair.first <= timestampNs) {
                    tracking = pair.second;
                    break;
                }
            }
        }

        AlvrStreamViewParams streamViewParams[2] = {};
        for (int eye = 0; eye < 2; eye++) {
            streamViewParams[eye].swapchain_index = CTX.streamBuffers[eye].index;
            streamViewParams[eye].reprojection_rotation = AlvrQuat{0.f, 0.f, 0.f, 1.f};
            streamViewParams[eye].fov = viewParams[eye].fov;
        }

        qiyu_StartEye(false, EYE_Left, TT_Texture);
        qiyu_StartEye(false, EYE_Right, TT_Texture);
        alvr_render_stream_opengl(streamHardwareBuffer, streamViewParams);
        qiyu_EndEye(false, EYE_Left, TT_Texture);
        qiyu_EndEye(false, EYE_Right, TT_Texture);

        float vsyncQueueMs = qiyu_PredictDisplayTime();
        alvr_report_submit(timestampNs, vsyncQueueMs * 1e6);

        for (int eye = 0; eye < 2; eye++) {
            frameParam.renderLayers[eye].imageHandle = CTX.streamBuffers[eye].eyeTarget[CTX.streamBuffers[eye].index].GetColorAttachment();
            frameParam.renderLayers[eye].imageType = TT_Texture;
            CreateLayout_(0.0f, 0.0f, 1.0f, 1.0f, &frameParam.renderLayers[eye].imageCoords);//FIXME! //TODO!
            frameParam.renderLayers[eye].eyeMask = eye ? RL_EyeMask_Right : RL_EyeMask_Left;
            CTX.streamBuffers[eye].index = (CTX.streamBuffers[eye].index + 1) % NUM_EYE_BUFFERS_;
        }
    } else {
        qiyu_DeviceInfo di = qiyu_GetDeviceInfo();
        float fPredictedTimeMs = qiyu_PredictDisplayTime();
	    tracking = qiyu_PredictHeadPose(fPredictedTimeMs);

        qiyu_Quaternion leftEyeRot;// glm quat is (w)(xyz), BUT here is xyzw
        leftEyeRot.x = di.frustumLeftEye.rotation.x;
        leftEyeRot.y = di.frustumLeftEye.rotation.y;
        leftEyeRot.z = di.frustumLeftEye.rotation.z;
        leftEyeRot.w = di.frustumLeftEye.rotation.w;
        qiyu_Quaternion rightEyeRot;// glm quat is (w)(xyz), BUT here is xyzw
        rightEyeRot.x = di.frustumRightEye.rotation.x;
        rightEyeRot.y = di.frustumRightEye.rotation.y;
        rightEyeRot.z = di.frustumRightEye.rotation.z;
        rightEyeRot.w = di.frustumRightEye.rotation.w;
        qiyu_Matrix4 outEyeMatrix[2];
        qiyu_GetViewMatrix(outEyeMatrix[0], outEyeMatrix[1], g_fTrackingOffset, tracking, leftEyeRot, rightEyeRot);

        AlvrLobbyViewParams lobbyViewParams[2] = {};
        for (int eye = 0; eye < 2; eye++) {
            auto q = tracking.pose.rotation;
            auto v = ovrMatrix4f_Inverse((ovrMatrix4f*) &outEyeMatrix[eye]);

            lobbyViewParams[eye].swapchain_index = CTX.lobbyBuffers[eye].index;
            lobbyViewParams[eye].pose.orientation = AlvrQuat{q.x, q.y, q.z, -q.w};
            lobbyViewParams[eye].pose.position[0] = -v.M[0][3];
            lobbyViewParams[eye].pose.position[1] = -v.M[1][3] - g_fTrackingOffset;
            lobbyViewParams[eye].pose.position[2] = -v.M[2][3];
            lobbyViewParams[eye].fov = getFov(&di, eye);
        }

        qiyu_StartEye(false, EYE_Left, TT_Texture);
        qiyu_StartEye(false, EYE_Right, TT_Texture);
        alvr_render_lobby_opengl(lobbyViewParams, true);
        qiyu_EndEye(false, EYE_Left, TT_Texture);
        qiyu_EndEye(false, EYE_Right, TT_Texture);

        for (int eye = 0; eye < 2; eye++) {
            frameParam.renderLayers[eye].imageHandle = CTX.lobbyBuffers[eye].eyeTarget[CTX.lobbyBuffers[eye].index].GetColorAttachment();
            frameParam.renderLayers[eye].imageType = TT_Texture;
            CreateLayout_(0.0f, 0.0f, 1.0f, 1.0f, &frameParam.renderLayers[eye].imageCoords);//FIXME! //TODO!
            frameParam.renderLayers[eye].eyeMask = eye ? RL_EyeMask_Right : RL_EyeMask_Left;
            CTX.lobbyBuffers[eye].index = (CTX.lobbyBuffers[eye].index + 1) % NUM_EYE_BUFFERS_;
        }
    }

    frameParam.minVsyncs = 1;
    frameParam.headPoseState = tracking;

    qiyu_SubmitFrame(frameParam);

    CTX.ovrFrameIndex++;
}

extern "C" JNIEXPORT void JNICALL Java_alvr_client_VRActivity_onBatteryChangedNative(
        JNIEnv *_env, jobject _context, jint battery, jboolean plugged) {
    alvr_send_battery(HEAD_ID, (float) battery / 100.f, (bool) plugged);
    CTX.hmdBattery = battery;
    CTX.hmdPlugged = plugged;
}
