#include "volctl.h"

#include <atomic>
#include <thread>
#include <chrono>

#include "cmath"
#include "atlbase.h"
#include "combaseapi.h"
#include "endpointvolume.h"
#include "mmdeviceapi.h"
#include "functiondiscoverykeys.h" // PKEY_Device_FriendlyName

const IID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IAudioEndpointVolume = __uuidof(IAudioEndpointVolume);
const IID IID_IUnknown = __uuidof(IUnknown);

JavaVM *jvm;
jobject globalRefVolObj = NULL;
jobject globalRefVolDevObj = NULL;
std::atomic<bool> volumeChangeThreadIsRunning(false);
std::atomic<bool> volumeDeviceThreadIsRunning(false);

// Mutex causes ACCESS_VIOLATION in msvcp140.dll
std::atomic<bool> volumeChangeLock(false);
std::atomic<bool> volumeDeviceLock(false);

CComPtr<IMMDeviceEnumerator> getDeviceEnumerator() {
    CComPtr<IMMDeviceEnumerator> enumerator;
    enumerator.CoCreateInstance(
            CLSID_MMDeviceEnumerator, nullptr,
            CLSCTX_ALL);

    return enumerator;
}

CComPtr<IMMDevice> getDefaultAudioDevice() {
    auto enumerator = getDeviceEnumerator();

    CComPtr<IMMDevice> device;
    enumerator->GetDefaultAudioEndpoint(
            eRender,
            eMultimedia,
            &device);

    return device;
}

CComPtr<IAudioEndpointVolume> getEndpointVolume() {
    auto device = getDefaultAudioDevice();
    if (device == NULL) {
        return NULL;
    }

    CComPtr<IAudioEndpointVolume> volume;
    device->Activate(
            IID_IAudioEndpointVolume,
            CLSCTX_ALL,
            nullptr,
            (void **) &volume
    );

    return volume;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    jvm = vm;
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved) {
	JNIEnv *env;
	jvm->AttachCurrentThread((void **)&env, nullptr);

    if (globalRefVolObj != NULL) {
        env->DeleteGlobalRef(globalRefVolObj);
        globalRefVolObj = NULL;
    }

    if (globalRefVolDevObj != NULL) {
        env->DeleteGlobalRef(globalRefVolDevObj);
        globalRefVolDevObj = NULL;
    }

	jvm->DetachCurrentThread();
}

JNIEXPORT jint JNICALL Java_net_bjoernpetersen_volctl_VolumeControl_getVolumeNative
        (JNIEnv *, jobject) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    float result = -1;

    auto volume = getEndpointVolume();
    if (volume != NULL) {
        volume->GetMasterVolumeLevelScalar(&result);
    }

    CoUninitialize();

    return (result >= 0) ? lround(result * 100.0) : -1;
}

JNIEXPORT void JNICALL Java_net_bjoernpetersen_volctl_VolumeControl_setVolumeNative
        (JNIEnv *, jobject, jint value) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    auto volume = getEndpointVolume();
    if (volume != NULL) {
        float floatValue = value / 100.0f;
        volume->SetMasterVolumeLevelScalar(floatValue, nullptr);
    }

    CoUninitialize();
}

JNIEXPORT jboolean JNICALL Java_net_bjoernpetersen_volctl_VolumeControl_getMuteNative
        (JNIEnv *, jobject) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    BOOL isMuted = FALSE;

    auto volume = getEndpointVolume();
    if (volume != NULL) {
        volume->GetMute(&isMuted);
    }

    CoUninitialize();

    return isMuted ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_net_bjoernpetersen_volctl_VolumeControl_setMuteNative
        (JNIEnv *, jobject, jboolean mute) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    auto volume = getEndpointVolume();
    if (volume != NULL) {
        volume->SetMute(mute == JNI_TRUE, nullptr);
    }

    CoUninitialize();
}

JNIEXPORT void JNICALL Java_net_bjoernpetersen_volctl_VolumeControl_toggleMuteNative
        (JNIEnv *, jobject, jboolean showSystemPanel) {
    if (showSystemPanel) {
        SendMessage(GetForegroundWindow(), WM_APPCOMMAND, 0, APPCOMMAND_VOLUME_MUTE * 0x10000);
    } else {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        auto volume = getEndpointVolume();
        if (volume != NULL) {
            BOOL isMuted;
            HRESULT hr = volume->GetMute(&isMuted);
            if (SUCCEEDED(hr)) {
                volume->SetMute(!isMuted, nullptr);
            }
        }

        CoUninitialize();
    }
}

JNIEXPORT void JNICALL Java_net_bjoernpetersen_volctl_VolumeControl_volumeUpNative
        (JNIEnv *, jobject, jboolean showSystemPanel) {
    if (showSystemPanel) {
        SendMessage(GetForegroundWindow(), WM_APPCOMMAND, 0, APPCOMMAND_VOLUME_UP * 0x10000);
    } else {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        auto volume = getEndpointVolume();
        if (volume != NULL) {
            volume->VolumeStepUp(nullptr);
        }

        CoUninitialize();
    }
}

JNIEXPORT void JNICALL Java_net_bjoernpetersen_volctl_VolumeControl_volumeDownNative
        (JNIEnv *, jobject, jboolean showSystemPanel) {
    if (showSystemPanel) {
        SendMessage(GetForegroundWindow(), WM_APPCOMMAND, 0, APPCOMMAND_VOLUME_DOWN * 0x10000);
    } else {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        auto volume = getEndpointVolume();
        if (volume != NULL) {
            volume->VolumeStepDown(nullptr);
        }

        CoUninitialize();
    }
}

jstring getDefaultAudioDeviceName(JNIEnv *env) {
    auto device = getDefaultAudioDevice();
    if (device == NULL) {
        return NULL;
    }

    HRESULT hr = S_OK;
    CComPtr<IPropertyStore> propertyStore;
    hr = device->OpenPropertyStore(STGM_READ, &propertyStore);
    if (FAILED(hr)) {
        return NULL;
    }

    PROPVARIANT friendlyName;
    PropVariantInit(&friendlyName);
    hr = propertyStore->GetValue(PKEY_Device_FriendlyName, &friendlyName);
    if (FAILED(hr)) {
        return NULL;
    }

    jstring result = env->NewString((jchar*)friendlyName.pwszVal,
        static_cast<jsize>(wcslen(friendlyName.pwszVal)));
    PropVariantClear(&friendlyName);

    return result;
}

JNIEXPORT jstring JNICALL Java_net_bjoernpetersen_volctl_VolumeControl_getDefaultDeviceNameNative
        (JNIEnv *env, jobject) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    jstring result = getDefaultAudioDeviceName(env);
    CoUninitialize();

    return result;
}

class VolumeCallback : public IAudioEndpointVolumeCallback {
public:
    VolumeCallback(JNIEnv *env) : refCount(1) {
        jclass cls = env->GetObjectClass(globalRefVolObj);
        jmidSubscribed = env->GetMethodID(cls, "onVolumeSubscribed", "()V");
        jmidUnsubscribed = env->GetMethodID(cls, "onVolumeUnsubscribed", "()V");
        jmidMuteChanged = env->GetMethodID(cls, "onMuteChanged", "(Z)V");
        jmidVolumeChanged = env->GetMethodID(cls, "onVolumeChanged", "(I)V");
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) {
        if (riid == IID_IUnknown || riid == __uuidof(IAudioEndpointVolumeCallback)) {
            *ppvObject = static_cast<IAudioEndpointVolumeCallback*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() {
        return InterlockedIncrement(&refCount);
    }

    ULONG STDMETHODCALLTYPE Release() {
        ULONG ulRef = InterlockedDecrement(&refCount);
        if (ulRef == 0) {
            delete this;
        }
        return ulRef;
    }

    HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pNotify) {
        if (pNotify == NULL) {
            return E_INVALIDARG;
        }

        if (!volumeChangeThreadIsRunning.load()) {
            return S_OK;
        }

        JNIEnv *env;
        jvm->AttachCurrentThread((void **)&env, nullptr);

        int mut = pNotify->bMuted ? 1 : 0;
        if (muted != mut) {
            muted = mut;
            env->CallVoidMethod(globalRefVolObj, jmidMuteChanged, pNotify->bMuted);
        }

        if (!pNotify->bMuted) {
            int vol = lround(pNotify->fMasterVolume * 100.0);
            if (volume != vol) {
                volume = vol;
                env->CallVoidMethod(globalRefVolObj, jmidVolumeChanged, volume);
            }
        }

        jvm->DetachCurrentThread();

        return S_OK;
    }

    void onSubscribed() {
        JNIEnv *env;
        jvm->AttachCurrentThread((void **)&env, nullptr);
        env->CallVoidMethod(globalRefVolObj, jmidSubscribed);
        jvm->DetachCurrentThread();
    }

    void onUnsubscribed() {
        JNIEnv *env;
        jvm->AttachCurrentThread((void **)&env, nullptr);
        env->CallVoidMethod(globalRefVolObj, jmidUnsubscribed);
        jvm->DetachCurrentThread();
    }

private:
    int volume = -1;
    int muted = -1;
    long refCount;
    jmethodID jmidSubscribed;
    jmethodID jmidUnsubscribed;
    jmethodID jmidMuteChanged;
    jmethodID jmidVolumeChanged;
};

void volumeChangeThreadFunction(VolumeCallback* callback) {
    while (volumeChangeLock.load()) {
        std::this_thread::yield();
    }
	volumeChangeLock.store(true);
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    auto volume = getEndpointVolume();
    if (volume != NULL) {
        HRESULT hr = volume->RegisterControlChangeNotify(callback);

        if (SUCCEEDED(hr)) {
            callback->onSubscribed();

            while (volumeChangeThreadIsRunning.load()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            volume = getEndpointVolume();
            if (volume != NULL) {
                volume->UnregisterControlChangeNotify(callback);
            }
        }
    }

    volumeChangeThreadIsRunning.store(true);

    if (callback != NULL) {
        callback->onUnsubscribed();
        callback->Release();
        callback = nullptr;
    }

    CoUninitialize();
	volumeChangeLock.store(false);
}

JNIEXPORT void JNICALL Java_net_bjoernpetersen_volctl_VolumeControl_subscribeToVolumeChangesNative
        (JNIEnv *env, jobject obj) {
    volumeChangeThreadIsRunning.store(false);
    while (volumeChangeLock.load()) {
        std::this_thread::yield();
    }
	volumeChangeLock.store(true);
    volumeChangeThreadIsRunning.store(true);
    globalRefVolObj = env->NewGlobalRef(obj);
    VolumeCallback* callback = new VolumeCallback(env);
    std::thread volumeChangeThread(volumeChangeThreadFunction, callback);
    volumeChangeThread.detach();
	volumeChangeLock.store(false);
}

JNIEXPORT void JNICALL Java_net_bjoernpetersen_volctl_VolumeControl_unsubscribeFromVolumeChangesNative
        (JNIEnv *env, jobject obj) {
    volumeChangeThreadIsRunning.store(false);
}

class VolumeDeviceCallback : public IMMNotificationClient {
public:
    VolumeDeviceCallback(JNIEnv *env) : refCount(1), enumerator(NULL), firstDevice(true), lastDeviceId(NULL) {
        jclass cls = env->GetObjectClass(globalRefVolDevObj);
        jmidSubscribed = env->GetMethodID(cls, "onVolumeDeviceSubscribed", "()V");
        jmidUnsubscribed = env->GetMethodID(cls, "onVolumeDeviceUnsubscribed", "()V");
        jmidDeviceAdded = env->GetMethodID(cls, "onDeviceAdded", "(Ljava/lang/String;)V");
        jmidDeviceActive = env->GetMethodID(cls, "onDeviceActive", "(Ljava/lang/String;)V");
        jmidDeviceRemoved = env->GetMethodID(cls, "onDeviceRemoved", "(Ljava/lang/String;)V");
        jmidDeviceDisabled = env->GetMethodID(cls, "onDeviceDisabled", "(Ljava/lang/String;)V");
        jmidDeviceUnplugged = env->GetMethodID(cls, "onDeviceUnplugged", "(Ljava/lang/String;)V");
        jmidDeviceNotPresent = env->GetMethodID(cls, "onDeviceNotPresent", "(Ljava/lang/String;)V");
        jmidDefaultDeviceChanged = env->GetMethodID(cls, "onDefaultDeviceChanged", "(Ljava/lang/String;)V");
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
        if (riid == IID_IUnknown || riid == __uuidof(IMMNotificationClient)) {
            *ppvObject = static_cast<IMMNotificationClient*>(this);
            AddRef();
            return S_OK;
        }
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() {
        return InterlockedIncrement(&refCount);
    }

    ULONG STDMETHODCALLTYPE Release() {
        ULONG ulRef = InterlockedDecrement(&refCount);
        if (ulRef == 0) {
            delete this;
        }
        return ulRef;
    }

    jstring getDeviceName(JNIEnv *env, LPCWSTR pwstrId) {
        if (pwstrId == NULL) {
            return NULL;
        }

        if (enumerator == NULL) {
            enumerator = getDeviceEnumerator();
            if (enumerator == NULL) {
                return NULL;
            }
        }

        HRESULT hr = S_OK;
        CComPtr<IMMDevice> device;
        hr = enumerator->GetDevice(pwstrId, &device);
        if (FAILED(hr)) {
            return NULL;
        }

        CComPtr<IPropertyStore> propertyStore;
        hr = device->OpenPropertyStore(STGM_READ, &propertyStore);
        if (FAILED(hr)) {
            return NULL;
        }

        PROPVARIANT friendlyName;
        PropVariantInit(&friendlyName);
        hr = propertyStore->GetValue(PKEY_Device_FriendlyName, &friendlyName);
        if (FAILED(hr)) {
            return NULL;
        }

        jstring result = env->NewString((jchar*)friendlyName.pwszVal,
            static_cast<jsize>(wcslen(friendlyName.pwszVal)));
        PropVariantClear(&friendlyName);

        return result;
    }

    bool devcmp(LPCWSTR d1, LPCWSTR d2) {
        if (d1 == NULL && d2 == NULL) return true;
        if (d1 == NULL || d2 == NULL) return false;
        if (wcscmp(d1, d2) == 0) return true;
        return false;
    }

    HRESULT callJavaMethod(jmethodID jmid, LPCWSTR pwstrDeviceId) {
        if (volumeDeviceThreadIsRunning.load()) {
            JNIEnv *env;
            jvm->AttachCurrentThread((void **)&env, nullptr);

            jstring deviceName = getDeviceName(env, pwstrDeviceId);
            env->CallVoidMethod(globalRefVolDevObj, jmid, deviceName);

            jvm->DetachCurrentThread();
        }

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId) {
        if (flow == eRender && volumeDeviceThreadIsRunning.load() &&
            (firstDevice || !devcmp(pwstrDeviceId, lastDeviceId))) {

            firstDevice = false;

            if (lastDeviceId != NULL) {
                delete[] lastDeviceId;
                lastDeviceId = NULL;
            }

            if (pwstrDeviceId != NULL) {
                size_t deviceIdLength = wcslen(pwstrDeviceId) + 1;
                lastDeviceId = new wchar_t[deviceIdLength];
                wcscpy_s(lastDeviceId, deviceIdLength, pwstrDeviceId);
            }

            JNIEnv *env;
            jstring deviceName = NULL;
            jvm->AttachCurrentThread((void **)&env, nullptr);

            if (pwstrDeviceId != NULL) {
                deviceName = getDefaultAudioDeviceName(env);
            }

            env->CallVoidMethod(globalRefVolDevObj, jmidDefaultDeviceChanged, deviceName);

            jvm->DetachCurrentThread();
        }

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId) {
        return callJavaMethod(jmidDeviceAdded, pwstrDeviceId);
    }

    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId) {
        return callJavaMethod(jmidDeviceRemoved, pwstrDeviceId);
    }

    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) {
        if (dwNewState == NULL) {
            return S_OK;
        }

        switch (dwNewState) {
            case DEVICE_STATE_ACTIVE:
                return callJavaMethod(jmidDeviceActive, pwstrDeviceId);
            case DEVICE_STATE_DISABLED:
                return callJavaMethod(jmidDeviceDisabled, pwstrDeviceId);
            case DEVICE_STATE_NOTPRESENT:
                return callJavaMethod(jmidDeviceNotPresent, pwstrDeviceId);
            case DEVICE_STATE_UNPLUGGED:
                return callJavaMethod(jmidDeviceUnplugged, pwstrDeviceId);
            default:
                return S_OK;
        }
    }

    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) {
        return S_OK;
    }

    void onSubscribed() {
        JNIEnv *env;
        jvm->AttachCurrentThread((void **)&env, nullptr);
        env->CallVoidMethod(globalRefVolDevObj, jmidSubscribed);
        jvm->DetachCurrentThread();
    }

    void onUnsubscribed() {
        JNIEnv *env;
        jvm->AttachCurrentThread((void **)&env, nullptr);
        env->CallVoidMethod(globalRefVolDevObj, jmidUnsubscribed);
        jvm->DetachCurrentThread();
    }

private:
    long refCount;
    bool firstDevice;
    wchar_t* lastDeviceId;
    jmethodID jmidSubscribed;
    jmethodID jmidDeviceAdded;
    jmethodID jmidUnsubscribed;
    jmethodID jmidDeviceActive;
    jmethodID jmidDeviceRemoved;
    jmethodID jmidDeviceDisabled;
    jmethodID jmidDeviceUnplugged;
    jmethodID jmidDeviceNotPresent;
    jmethodID jmidDefaultDeviceChanged;
    CComPtr<IMMDeviceEnumerator> enumerator;
};

void volumeDeviceThreadFunction(VolumeDeviceCallback* callback) {
    while (volumeDeviceLock.load()) {
        std::this_thread::yield();
    }
	volumeDeviceLock.store(true);
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    auto enumerator = getDeviceEnumerator();
    HRESULT hr = enumerator->RegisterEndpointNotificationCallback(callback);

    if (SUCCEEDED(hr)) {
        callback->onSubscribed();

        while (volumeDeviceThreadIsRunning.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        enumerator = getDeviceEnumerator();
        if (enumerator != NULL) {
            enumerator->UnregisterEndpointNotificationCallback(callback);
        }
    }

    volumeDeviceThreadIsRunning.store(false);

    if (callback != NULL) {
        callback->onUnsubscribed();
        callback->Release();
        callback = nullptr;
    }

    CoUninitialize();
	volumeDeviceLock.store(false);
}

JNIEXPORT void JNICALL Java_net_bjoernpetersen_volctl_VolumeControl_subscribeToVolumeDeviceChangesNative
        (JNIEnv *env, jobject obj) {
    volumeDeviceThreadIsRunning.store(false);
    while (volumeDeviceLock.load()) {
        std::this_thread::yield();
    }
	volumeDeviceLock.store(true);
    volumeDeviceThreadIsRunning.store(true);
    globalRefVolDevObj = env->NewGlobalRef(obj);
    VolumeDeviceCallback* callback = new VolumeDeviceCallback(env);
    std::thread volumeDeviceThread(volumeDeviceThreadFunction, callback);
    volumeDeviceThread.detach();
	volumeDeviceLock.store(false);
}

JNIEXPORT void JNICALL Java_net_bjoernpetersen_volctl_VolumeControl_unsubscribeFromVolumeDeviceChangesNative
        (JNIEnv *env, jobject obj) {
    volumeDeviceThreadIsRunning.store(false);
}
