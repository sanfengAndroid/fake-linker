package eu.chainfire.libsuperuser;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

/**
 * <p>
 * Base receiver to extend to catch notifications when overlays should be
 * hidden.
 * </p>
 * <p>
 * Tapjacking protection in SuperSU prevents some dialogs from receiving user
 * input when overlays are present. For security reasons this notification is
 * only sent to apps that have previously been granted root access, so even if
 * your app does not <em>require</em> root, you still need to <em>request</em>
 * it, and the user must grant it.
 * </p>
 * <p>
 * Note that the word overlay as used here should be interpreted as "any view or
 * window possibly obscuring SuperSU dialogs".
 * </p>
 */
public abstract class HideOverlaysReceiver extends BroadcastReceiver {
    public static final String ACTION_HIDE_OVERLAYS = "eu.chainfire.supersu.action.HIDE_OVERLAYS";
    public static final String CATEGORY_HIDE_OVERLAYS = Intent.CATEGORY_INFO;
    public static final String EXTRA_HIDE_OVERLAYS = "eu.chainfire.supersu.extra.HIDE";

    @Override
    public final void onReceive(Context context, Intent intent) {
        if (intent.hasExtra(EXTRA_HIDE_OVERLAYS)) {
            onHideOverlays(context, intent, intent.getBooleanExtra(EXTRA_HIDE_OVERLAYS, false));
        }
    }

    /**
     * Called when overlays <em>should</em> be hidden or <em>may</em> be shown
     * again.
     *
     * @param context App context
     * @param intent  Received intent
     * @param hide    Should overlays be hidden?
     */
    public abstract void onHideOverlays(Context context, Intent intent, boolean hide);
}
