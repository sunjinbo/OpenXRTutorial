// Copyright 2023, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

// OpenXR Tutorial for Khronos Group

#include <DebugOutput.h>
// XR_DOCS_TAG_BEGIN_include_GraphicsAPI_D3D11
#include <GraphicsAPI_D3D11.h>
// XR_DOCS_TAG_END_include_GraphicsAPI_D3D11
// XR_DOCS_TAG_BEGIN_include_GraphicsAPI_D3D12
#include <GraphicsAPI_D3D12.h>
// XR_DOCS_TAG_END_include_GraphicsAPI_D3D12
// XR_DOCS_TAG_BEGIN_include_GraphicsAPI_OpenGL
#include <GraphicsAPI_OpenGL.h>
// XR_DOCS_TAG_END_include_GraphicsAPI_OpenGL
// XR_DOCS_TAG_BEGIN_include_GraphicsAPI_OpenGL_ES
#include <GraphicsAPI_OpenGL_ES.h>
// XR_DOCS_TAG_END_include_GraphicsAPI_OpenGL_ES
// XR_DOCS_TAG_BEGIN_include_GraphicsAPI_Vulkan
#include <GraphicsAPI_Vulkan.h>
// XR_DOCS_TAG_END_include_GraphicsAPI_Vulkan
// XR_DOCS_TAG_BEGIN_include_OpenXRDebugUtils
#include <OpenXRDebugUtils.h>
// XR_DOCS_TAG_END_include_OpenXRDebugUtils

#include <OpenXRHelper.h>

#define XR_DOCS_CHAPTER_VERSION XR_DOCS_CHAPTER_1_4

class OpenXRTutorial {
public:
    OpenXRTutorial(GraphicsAPI_Type apiType) : m_apiType(apiType) {
        if (!CheckGraphicsAPI_TypeIsValidForPlatform(m_apiType)) {
            std::cout << "ERROR: The provided Graphics API is not valid for this platform." << std::endl;
            DEBUG_BREAK;
        }
    }
    ~OpenXRTutorial() = default;

    void Run() {
        CreateInstance();
        CreateDebugMessenger();

        GetInstanceProperties();
        GetSystemID();

        CreateSession();

        while (m_applicationRunning) {
            PollSystemEvents();
            PollEvents();
            if (m_sessionRunning) {
                // Draw Frame
            }
        }

        DestroySession();

        DestroyDebugMessenger();
        DestroyInstance();
    }

private:
    void CreateInstance() {
        XrApplicationInfo AI;
        strncpy(AI.applicationName, "OpenXR Tutorial", XR_MAX_APPLICATION_NAME_SIZE);
        AI.applicationVersion = 1;
        strncpy(AI.engineName, "OpenXR Engine", XR_MAX_ENGINE_NAME_SIZE);
        AI.engineVersion = 1;
        AI.apiVersion = XR_CURRENT_API_VERSION;

        m_instanceExtensions.push_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
        // Ensure m_apiType is already defined when we call this line.
        m_instanceExtensions.push_back(GetGraphicsAPIInstanceExtensionString(m_apiType));

        // Get all the API Layers from the OpenXR runtime.
        uint32_t apiLayerCount = 0;
        std::vector<XrApiLayerProperties> apiLayerProperties;
        OPENXR_CHECK(xrEnumerateApiLayerProperties(0, &apiLayerCount, nullptr), "Failed to enumerate ApiLayerProperties.");
        apiLayerProperties.resize(apiLayerCount, {XR_TYPE_API_LAYER_PROPERTIES});  // two-call idiom
        OPENXR_CHECK(xrEnumerateApiLayerProperties(apiLayerCount, &apiLayerCount, apiLayerProperties.data()), "Failed to enumerate ApiLayerProperties.");

        // Check the requested API layers against the ones from the OpenXR. If found add it to the Active API Layers.
        for (auto &requestLayer : m_apiLayers) {
            for (auto &layerProperty : apiLayerProperties) {
                // strcmp returns 0 if the strings match.
                if (strcmp(requestLayer.c_str(), layerProperty.layerName) != 0) {
                    continue;
                } else {
                    m_activeAPILayers.push_back(requestLayer.c_str());
                    break;
                }
            }
        }

        // Get all the Instance Extensions from the OpenXR instance.
        uint32_t extensionCount = 0;
        std::vector<XrExtensionProperties> extensionProperties;
        OPENXR_CHECK(xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr), "Failed to enumerate InstanceExtensionProperties.");
        extensionProperties.resize(extensionCount, {XR_TYPE_EXTENSION_PROPERTIES}); // two-call idiom
        OPENXR_CHECK(xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount, extensionProperties.data()), "Failed to enumerate InstanceExtensionProperties.");

        // Check the requested Instance Extensions against the ones from the OpenXR runtime.
        // If an extension is found add it to Active Instance Extensions.
        // Log error if the Instance Extension is not found.
        for (auto &requestedInstanceExtension : m_instanceExtensions) {
            bool found = false;
            for (auto &extensionProperty : extensionProperties) {
                // strcmp returns 0 if the strings match.
                if (strcmp(requestedInstanceExtension.c_str(), extensionProperty.extensionName) != 0) {
                    continue;
                } else {
                    m_activeInstanceExtensions.push_back(requestedInstanceExtension.c_str());
                    found = true;
                    break;
                }
            }
            if (!found) {
                XR_TUT_LOG_ERROR("Failed to find OpenXR instance extension: " << requestedInstanceExtension);
            }
        }

        XrInstanceCreateInfo instanceCI{XR_TYPE_INSTANCE_CREATE_INFO};
        instanceCI.createFlags = 0;
        instanceCI.applicationInfo = AI;
        instanceCI.enabledApiLayerCount = static_cast<uint32_t>(m_activeAPILayers.size());
        instanceCI.enabledApiLayerNames = m_activeAPILayers.data();
        instanceCI.enabledExtensionCount = static_cast<uint32_t>(m_activeInstanceExtensions.size());
        instanceCI.enabledExtensionNames = m_activeInstanceExtensions.data();
        OPENXR_CHECK(xrCreateInstance(&instanceCI, &m_xrInstance), "Failed to create Instance.");
    }

    void DestroyInstance() {
        OPENXR_CHECK(xrDestroyInstance(m_xrInstance), "Failed to destroy Instance.");
    }

    void CreateDebugMessenger() {
        // Check that "XR_EXT_debug_utils" is in the active Instance Extensions before creating an XrDebugUtilsMessengerEXT.
        if (IsStringInVector(m_activeInstanceExtensions, XR_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
            m_debugUtilsMessenger = CreateOpenXRDebugUtilsMessenger(m_xrInstance);  // From OpenXRDebugUtils.h.
        }
    }

    void DestroyDebugMessenger() {
        // Check that "XR_EXT_debug_utils" is in the active Instance Extensions before destroying the XrDebugUtilsMessengerEXT.
        if (m_debugUtilsMessenger != XR_NULL_HANDLE) {
            DestroyOpenXRDebugUtilsMessenger(m_xrInstance, m_debugUtilsMessenger);  // From OpenXRDebugUtils.h.
        }
    }

    void GetInstanceProperties() {
        XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES};
        OPENXR_CHECK(xrGetInstanceProperties(m_xrInstance, &instanceProperties), "Failed to get InstanceProperties.");

        XR_TUT_LOG("OpenXR Runtime: " << instanceProperties.runtimeName << " - "
                                      << XR_VERSION_MAJOR(instanceProperties.runtimeVersion) << "."
                                      << XR_VERSION_MINOR(instanceProperties.runtimeVersion) << "."
                                      << XR_VERSION_PATCH(instanceProperties.runtimeVersion));
    }

    void GetSystemID() {
        // Get the XrSystemId from the instance and the supplied XrFormFactor.
        XrSystemGetInfo systemGI{XR_TYPE_SYSTEM_GET_INFO};
        systemGI.formFactor = m_formFactor;
        OPENXR_CHECK(xrGetSystem(m_xrInstance, &systemGI, &m_systemID), "Failed to get SystemID.");

        // Get the System's properties for some general information about the hardware and the vendor.
        OPENXR_CHECK(xrGetSystemProperties(m_xrInstance, m_systemID, &m_systemProperties), "Failed to get SystemProperties.");
    }

    void CreateSession() {
        XrSessionCreateInfo sessionCI{XR_TYPE_SESSION_CREATE_INFO};

        m_graphicsAPI = std::make_unique<GraphicsAPI_Vulkan>(m_xrInstance, m_systemID);
        sessionCI.next = m_graphicsAPI->GetGraphicsBinding();
        sessionCI.createFlags = 0;
        sessionCI.systemId = m_systemID;

        OPENXR_CHECK(xrCreateSession(m_xrInstance, &sessionCI, &m_session), "Failed to create Session.");
    }

    void DestroySession() {
        OPENXR_CHECK(xrDestroySession(m_session), "Failed to destroy Session.");
    }

    void PollEvents() {
        // Poll OpenXR for a new event.
        XrEventDataBuffer eventData{XR_TYPE_EVENT_DATA_BUFFER};
        auto XrPollEvents = [&]() -> bool {
            eventData = {XR_TYPE_EVENT_DATA_BUFFER};
            return xrPollEvent(m_xrInstance, &eventData) == XR_SUCCESS;
        };

        while (XrPollEvents()) {
            switch (eventData.type) {
                // Log the number of lost events from the runtime.
                case XR_TYPE_EVENT_DATA_EVENTS_LOST: {
                    XrEventDataEventsLost *eventsLost = reinterpret_cast<XrEventDataEventsLost *>(&eventData);
                    XR_TUT_LOG("OPENXR: Events Lost: " << eventsLost->lostEventCount);
                    break;
                }

                // Log that an instance loss is pending and shutdown the application.
                case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
                    XrEventDataInstanceLossPending *instanceLossPending = reinterpret_cast<XrEventDataInstanceLossPending *>(&eventData);
                    XR_TUT_LOG("OPENXR: Instance Loss Pending at: " << instanceLossPending->lossTime);
                    m_sessionRunning = false;
                    m_applicationRunning = false;
                    break;
                }

                // Log that the interaction profile has changed.
                case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED: {
                    XrEventDataInteractionProfileChanged *interactionProfileChanged = reinterpret_cast<XrEventDataInteractionProfileChanged *>(&eventData);
                    XR_TUT_LOG("OPENXR: Interaction Profile changed for Session: " << interactionProfileChanged->session);
                    if (interactionProfileChanged->session != m_session) {
                        XR_TUT_LOG("XrEventDataInteractionProfileChanged for unknown Session");
                        break;
                    }
                    break;
                }

                // Log that there's a reference space change pending.
                case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: {
                    XrEventDataReferenceSpaceChangePending *referenceSpaceChangePending = reinterpret_cast<XrEventDataReferenceSpaceChangePending *>(&eventData);
                    XR_TUT_LOG("OPENXR: Reference Space Change pending for Session: " << referenceSpaceChangePending->session);
                    if (referenceSpaceChangePending->session != m_session) {
                        XR_TUT_LOG("XrEventDataReferenceSpaceChangePending for unknown Session");
                        break;
                    }
                    break;
                }

                // Session State changes:
                case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                    XrEventDataSessionStateChanged *sessionStateChanged = reinterpret_cast<XrEventDataSessionStateChanged *>(&eventData);
                    if (sessionStateChanged->session != m_session) {
                        XR_TUT_LOG("XrEventDataSessionStateChanged for unknown Session");
                        break;
                    }

                    if (sessionStateChanged->state == XR_SESSION_STATE_READY) {
                        // SessionState is ready. Begin the XrSession using the XrViewConfigurationType.
                        XrSessionBeginInfo sessionBeginInfo{XR_TYPE_SESSION_BEGIN_INFO};
                        sessionBeginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                        OPENXR_CHECK(xrBeginSession(m_session, &sessionBeginInfo), "Failed to begin Session.");
                        m_sessionRunning = true;
                    }
                    if (sessionStateChanged->state == XR_SESSION_STATE_STOPPING) {
                        // SessionState is stopping. End the XrSession.
                        OPENXR_CHECK(xrEndSession(m_session), "Failed to end Session.");
                        m_sessionRunning = false;
                    }
                    if (sessionStateChanged->state == XR_SESSION_STATE_EXITING) {
                        // SessionState is exiting. Exit the application.
                        m_sessionRunning = false;
                        m_applicationRunning = false;
                    }
                    if (sessionStateChanged->state == XR_SESSION_STATE_LOSS_PENDING) {
                        // SessionState is loss pending. Exit the application.
                        // It's possible to try a reestablish an XrInstance and XrSession, but we will simply exit here.
                        m_sessionRunning = false;
                        m_applicationRunning = false;
                    }
                    // Store state for reference across the application.
                    m_sessionState = sessionStateChanged->state;
                    break;
                }
                default: {
                    break;
                }
            }
        }
    }

#if defined(__ANDROID__)
    // XR_DOCS_TAG_BEGIN_Android_System_Functionality1
public:
    // Stored pointer to the android_app structure from android_main().
    static android_app *androidApp;

    // Custom data structure that is used by PollSystemEvents().
    // Modified from https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/d6b6d7a10bdcf8d4fe806b4f415fde3dd5726878/src/tests/hello_xr/main.cpp#L133C1-L189C2
    struct AndroidAppState {
        ANativeWindow *nativeWindow = nullptr;
        bool resumed = false;
    };
    static AndroidAppState androidAppState;

    // Processes the next command from the Android OS. It updates AndroidAppState.
    static void AndroidAppHandleCmd(struct android_app *app, int32_t cmd) {
        AndroidAppState *appState = (AndroidAppState *)app->userData;

        switch (cmd) {
        // There is no APP_CMD_CREATE. The ANativeActivity creates the application thread from onCreate().
        // The application thread then calls android_main().
        case APP_CMD_START: {
            break;
        }
        case APP_CMD_RESUME: {
            appState->resumed = true;
            break;
        }
        case APP_CMD_PAUSE: {
            appState->resumed = false;
            break;
        }
        case APP_CMD_STOP: {
            break;
        }
        case APP_CMD_DESTROY: {
            appState->nativeWindow = nullptr;
            break;
        }
        case APP_CMD_INIT_WINDOW: {
            appState->nativeWindow = app->window;
            break;
        }
        case APP_CMD_TERM_WINDOW: {
            appState->nativeWindow = nullptr;
            break;
        }
        }
    }
    // XR_DOCS_TAG_END_Android_System_Functionality1

private:
    void PollSystemEvents() {
        // XR_DOCS_TAG_BEGIN_Android_System_Functionality2
        // Checks whether Android has requested that application should by destroyed.
        if (androidApp->destroyRequested != 0) {
            m_applicationRunning = false;
            return;
        }
        while (true) {
            // Poll and process the Android OS system events.
            struct android_poll_source *source = nullptr;
            int events = 0;
            // The timeout depends on whether the application is active.
            const int timeoutMilliseconds = (!androidAppState.resumed && !m_sessionRunning && androidApp->destroyRequested == 0) ? -1 : 0;
            if (ALooper_pollOnce(timeoutMilliseconds, nullptr, &events, (void**)&source) >= 0) {
                if (source != nullptr) {
                    source->process(androidApp, source);
                }
            } else {
                break;
            }
        }
        // XR_DOCS_TAG_END_Android_System_Functionality2
    }
#else
    void PollSystemEvents() {
        return;
    }
#endif

private:
    XrInstance m_xrInstance = {};
    std::vector<const char *> m_activeAPILayers = {};
    std::vector<const char *> m_activeInstanceExtensions = {};
    std::vector<std::string> m_apiLayers = {};
    std::vector<std::string> m_instanceExtensions = {};

    XrDebugUtilsMessengerEXT m_debugUtilsMessenger = {};

    XrFormFactor m_formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId m_systemID = {};
    XrSystemProperties m_systemProperties = {XR_TYPE_SYSTEM_PROPERTIES};

    GraphicsAPI_Type m_apiType = UNKNOWN;

    std::unique_ptr<GraphicsAPI> m_graphicsAPI = nullptr;

    XrSession m_session = XR_NULL_HANDLE;
    XrSessionState m_sessionState = XR_SESSION_STATE_UNKNOWN;
    bool m_applicationRunning = true;
    bool m_sessionRunning = false;
};

void OpenXRTutorial_Main(GraphicsAPI_Type apiType) {
    DebugOutput debugOutput;  // This redirects std::cerr and std::cout to the IDE's output or Android Studio's logcat.
    XR_TUT_LOG("OpenXR Tutorial Chapter 1");

    OpenXRTutorial app(apiType);
    app.Run();
}

#if defined(_WIN32) || (defined(__linux__) && !defined(__ANDROID__))
int main(int argc, char **argv) {
    OpenXRTutorial_Main(XR_TUTORIAL_GRAPHICS_API);
}
/*
// XR_DOCS_TAG_BEGIN_main_Windows_Linux_OPENGL
int main(int argc, char **argv) {
    OpenXRTutorial_Main(OPENGL);
}
// XR_DOCS_TAG_END_main_Windows_Linux_OPENGL
// XR_DOCS_TAG_BEGIN_main_Windows_Linux_VULKAN
int main(int argc, char **argv) {
    OpenXRTutorial_Main(VULKAN);
}
// XR_DOCS_TAG_END_main_Windows_Linux_VULKAN
// XR_DOCS_TAG_BEGIN_main_Windows_Linux_D3D11
int main(int argc, char **argv) {
    OpenXRTutorial_Main(D3D11);
}
// XR_DOCS_TAG_END_main_Windows_Linux_D3D11
// XR_DOCS_TAG_BEGIN_main_Windows_Linux_D3D12
int main(int argc, char **argv) {
    OpenXRTutorial_Main(D3D12);
}
// XR_DOCS_TAG_END_main_Windows_Linux_D3D12
*/
#elif (__ANDROID__)
// XR_DOCS_TAG_BEGIN_android_main___ANDROID__
android_app *OpenXRTutorial::androidApp = nullptr;
OpenXRTutorial::AndroidAppState OpenXRTutorial::androidAppState = {};

void android_main(struct android_app *app) {
    // Allow interaction with JNI and the JVM on this thread.
    // https://developer.android.com/training/articles/perf-jni#threads
    JNIEnv *env;
    app->activity->vm->AttachCurrentThread(&env, nullptr);

    // https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html#XR_KHR_loader_init
    // Load xrInitializeLoaderKHR() function pointer. On Android, the loader must be initialized with variables from android_app *.
    // Without this, there's is no loader and thus our function calls to OpenXR would fail.
    XrInstance m_xrInstance = XR_NULL_HANDLE;  // Dummy XrInstance variable for OPENXR_CHECK macro.
    PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR = nullptr;
    OPENXR_CHECK(xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction *)&xrInitializeLoaderKHR), "Failed to get InstanceProcAddr for xrInitializeLoaderKHR.");
    if (!xrInitializeLoaderKHR) {
        return;
    }

    // Fill out an XrLoaderInitInfoAndroidKHR structure and initialize the loader for Android.
    XrLoaderInitInfoAndroidKHR loaderInitializeInfoAndroid{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
    loaderInitializeInfoAndroid.applicationVM = app->activity->vm;
    loaderInitializeInfoAndroid.applicationContext = app->activity->clazz;
    OPENXR_CHECK(xrInitializeLoaderKHR((XrLoaderInitInfoBaseHeaderKHR *)&loaderInitializeInfoAndroid), "Failed to initialize Loader for Android.");

    // Set userData and Callback for PollSystemEvents().
    app->userData = &OpenXRTutorial::androidAppState;
    app->onAppCmd = OpenXRTutorial::AndroidAppHandleCmd;

    OpenXRTutorial::androidApp = app;
    // XR_DOCS_TAG_END_android_main___ANDROID__
    OpenXRTutorial_Main(XR_TUTORIAL_GRAPHICS_API);
}
/*
// XR_DOCS_TAG_BEGIN_android_main_OPENGL_ES
    OpenXRTutorial_Main(OPENGL_ES);
}
// XR_DOCS_TAG_END_android_main_OPENGL_ES
// XR_DOCS_TAG_BEGIN_android_main_VULKAN
    OpenXRTutorial_Main(VULKAN);
}
// XR_DOCS_TAG_END_android_main_VULKAN
*/
#endif
