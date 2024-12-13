package net.bjoernpetersen.volctl;

import java.util.EventListener;

/**
 * Interface for listening to volume changes.
 */
public interface VolumeChangeListener extends EventListener {

    /**
     * Method called when the volume level changes.
     *
     * @param volume The new volume level (from 0 to 100).
     */
    void onVolumeChanged(int volume);

    /**
     * Method called when the mute state changes.
     *
     * @param isMuted true if the sound is muted, false if the sound is unmuted.
     */
    void onMuteChanged(boolean isMuted);
}
