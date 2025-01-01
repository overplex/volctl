package net.bjoernpetersen.volctl;

public class VolumeAdapter implements VolumeChangeListener, VolumeDeviceListener {
    protected VolumeAdapter() {}

    /**
     * {@inheritDoc}
     */
    @Override
    public void onVolumeChanged(int volume) {}

    /**
     * {@inheritDoc}
     */
    @Override
    public void onMuteChanged(boolean isMuted) {}

    /**
     * {@inheritDoc}
     */
    @Override
    public void onSubscribed() {}

    /**
     * {@inheritDoc}
     */
    @Override
    public void onUnsubscribed() {}

    /**
     * {@inheritDoc}
     */
    @Override
    public void onDefaultDeviceChanged(String name) {}

    /**
     * {@inheritDoc}
     */
    @Override
    public void onDeviceAdded(String name) {}

    /**
     * {@inheritDoc}
     */
    @Override
    public void onDeviceRemoved(String name) {}

    /**
     * {@inheritDoc}
     */
    @Override
    public void onDeviceActive(String name) {}

    /**
     * {@inheritDoc}
     */
    @Override
    public void onDeviceDisabled(String name) {}

    /**
     * {@inheritDoc}
     */
    @Override
    public void onDeviceNotPresent(String name) {}

    /**
     * {@inheritDoc}
     */
    @Override
    public void onDeviceUnplugged(String name) {}
}
