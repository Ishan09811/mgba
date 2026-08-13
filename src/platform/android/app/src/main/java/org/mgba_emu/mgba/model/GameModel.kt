
package org.mgba_emu.mgba.model

import android.net.Uri
import kotlinx.serialization.Serializable
import org.mgba_emu.mgba.core.Platform
import org.mgba_emu.mgba.utils.UriSerializer

@Serializable
data class GameModel(
    @Serializable(with = UriSerializer::class) var uri: Uri,
    var fileName: String,
    var title: String? = null,
    var version: String? = null,
    var iconUrl: String? = null,
    var code: String? = null,
    var platform: Platform? = null,
    var lastPlayed: Long = 0L
)