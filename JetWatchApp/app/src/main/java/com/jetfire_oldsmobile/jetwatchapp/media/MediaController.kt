package com.jetfire_oldsmobile.jetwatchapp.media

import android.content.ComponentName
import android.content.Context
import android.media.AudioManager
import android.media.MediaMetadata
import android.media.session.MediaSessionManager
import android.media.session.PlaybackState
import com.jetfire_oldsmobile.jetwatchapp.log.AppLog
import com.jetfire_oldsmobile.jetwatchapp.service.WatchNotificationListener

data class MediaState(
    val title: String = "",
    val artist: String = "",
    val playing: Boolean = false,
    val vol: Int = 0,
    val volMax: Int = 0,
)

class MediaController(private val ctx: Context) {

    private val sessions =
        ctx.getSystemService(Context.MEDIA_SESSION_SERVICE) as MediaSessionManager

    private val own = ComponentName(ctx, WatchNotificationListener::class.java)

    private fun current(): android.media.session.MediaController? = try {
        sessions.getActiveSessions(own).firstOrNull()
    } catch (e: SecurityException) {
        AppLog.w("MEDIA", "no notification-listener access")
        null
    } catch (e: Exception) {
        AppLog.e("MEDIA", "getActiveSessions failed", e)
        null
    }

    fun play()  { current()?.transportControls?.play() }
    fun pause() { current()?.transportControls?.pause() }
    fun next()  { current()?.transportControls?.skipToNext() }
    fun prev()  { current()?.transportControls?.skipToPrevious() }

    fun togglePlayPause() {
        val c = current() ?: return
        if (c.playbackState?.state == PlaybackState.STATE_PLAYING) c.transportControls.pause()
        else                                                       c.transportControls.play()
    }

    fun volumeUp() {
        val am = ctx.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        am.adjustStreamVolume(AudioManager.STREAM_MUSIC, AudioManager.ADJUST_RAISE, 0)
    }

    fun volumeDown() {
        val am = ctx.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        am.adjustStreamVolume(AudioManager.STREAM_MUSIC, AudioManager.ADJUST_LOWER, 0)
    }

    fun snapshot(): MediaState {
        val c = current()
        val md = c?.metadata
        val state = c?.playbackState?.state
        val am = ctx.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        return MediaState(
            title   = md?.getString(MediaMetadata.METADATA_KEY_TITLE)  ?: "",
            artist  = md?.getString(MediaMetadata.METADATA_KEY_ARTIST) ?: "",
            playing = state == PlaybackState.STATE_PLAYING,
            vol     = am.getStreamVolume(AudioManager.STREAM_MUSIC),
            volMax  = am.getStreamMaxVolume(AudioManager.STREAM_MUSIC),
        )
    }
}