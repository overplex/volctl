#include "volctl.h"

#include "cmath"
#include "atlbase.h"
#include "combaseapi.h"
#include "endpointvolume.h"
#include "mmdeviceapi.h"
#include "functiondiscoverykeys.h" // PKEY_Device_FriendlyName

const IID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IAudioEndpointVolume = __uuidof(IAudioEndpointVolume);

CComPtr<IMMDevice> getImmDevice() {
    CComPtr<IMMDeviceEnumerator> enumerator;
    enumerator.CoCreateInstance(
            CLSID_MMDeviceEnumerator, nullptr,
            CLSCTX_ALL);

    CComPtr<IMMDevice> device;
    enumerator->GetDefaultAudioEndpoint(
            eRender,
            eMultimedia,
            &device);

    return device;
}

CComPtr<IAudioEndpointVolume> getEndpointVolume() {
    auto device = getImmDevice();
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
        (JNIEnv *, jobject) {
    SendMessage(GetForegroundWindow(), WM_APPCOMMAND, 0, APPCOMMAND_VOLUME_MUTE * 0x10000);
}

JNIEXPORT void JNICALL Java_net_bjoernpetersen_volctl_VolumeControl_volumeUpNative
        (JNIEnv *, jobject) {
    SendMessage(GetForegroundWindow(), WM_APPCOMMAND, 0, APPCOMMAND_VOLUME_UP * 0x10000);
}

JNIEXPORT void JNICALL Java_net_bjoernpetersen_volctl_VolumeControl_volumeDownNative
        (JNIEnv *, jobject) {
    SendMessage(GetForegroundWindow(), WM_APPCOMMAND, 0, APPCOMMAND_VOLUME_DOWN * 0x10000);
}

JNIEXPORT jstring JNICALL Java_net_bjoernpetersen_volctl_VolumeControl_getDeviceNameNative
        (JNIEnv *env, jobject) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    auto device = getImmDevice();
    if (device == NULL) {
        CoUninitialize();
        return NULL;
    }

    LPWSTR deviceId;
    device->GetId(&deviceId);

    CComPtr<IPropertyStore> propertyStore;
    device->OpenPropertyStore(STGM_READ, &propertyStore);

    PROPVARIANT friendlyName;
    PropVariantInit(&friendlyName);
    propertyStore->GetValue(PKEY_Device_FriendlyName, &friendlyName);

    jstring result = env->NewString((jchar*)friendlyName.pwszVal,
        static_cast<jsize>(wcslen(friendlyName.pwszVal)));

    PropVariantClear(&friendlyName);
    CoTaskMemFree(deviceId);

    CoUninitialize();

    return result;
}
