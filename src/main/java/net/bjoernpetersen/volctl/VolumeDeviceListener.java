package net.bjoernpetersen.volctl;

import java.util.EventListener;

/**
 * Listener interface for receiving notifications about changes to audio output devices.
 */
public interface VolumeDeviceListener extends EventListener {

    /**
     * Called when the default audio output device has changed.
     *
     * @param name the name of the new default audio output device
     */
    void onDefaultDeviceChanged(String name);

    /**
     * Called when a new audio output device has been added.
     *
     * @param name the name of the newly added audio output device
     */
    void onDeviceAdded(String name);

    /**
     * Called when an audio output device has been removed.
     *
     * @param name the name of the removed audio output device
     */
    void onDeviceRemoved(String name);

    /**
     * Called when an audio output device becomes active.
     * That is, the audio adapter that connects to the endpoint device
     * is present and enabled. In addition, if the endpoint device plugs into a jack on the adapter,
     * then the endpoint device is plugged in.
     *
     * @param name the name of the audio output device that has become active
     */
    void onDeviceActive(String name);

    /**
     * Called when an audio output device becomes disabled.
     * The user has disabled the device in the Windows multimedia control panel.
     *
     * @param name the name of the audio output device that has become disabled
     */
    void onDeviceDisabled(String name);

    /**
     * Called when an audio output device is not present.
     * The audio endpoint device is not present because the audio adapter that connects to the endpoint device
     * has been removed from the system, or the user has disabled the adapter device in Device Manager.
     *
     * @param name the name of the audio output device that is not present
     */
    void onDeviceNotPresent(String name);

    /**
     * Called when an audio output device is unplugged.
     * The audio adapter that contains the jack for the endpoint device
     * is present and enabled, but the endpoint device is not plugged into the jack.
     * Only a device with jack-presence detection can be in this state.
     *
     * @param name the name of the audio output device that has been unplugged
     */
    void onDeviceUnplugged(String name);

    /**
     * Called when the listener has successfully subscribed to volume device changes.
     */
    void onSubscribed();

    /**
     * Called when the listener has successfully unsubscribed from volume device changes.
     */
    void onUnsubscribed();
}
