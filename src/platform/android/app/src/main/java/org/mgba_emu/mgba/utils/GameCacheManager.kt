
package org.mgba_emu.mgba.utils

import android.content.Context
import android.net.Uri
import androidx.core.content.edit
import androidx.core.net.toUri
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.serialization.KSerializer
import kotlinx.serialization.descriptors.PrimitiveKind
import kotlinx.serialization.descriptors.PrimitiveSerialDescriptor
import kotlinx.serialization.descriptors.SerialDescriptor
import kotlinx.serialization.encoding.Decoder
import kotlinx.serialization.encoding.Encoder
import kotlinx.serialization.json.Json
import org.mgba_emu.mgba.mGBAApplication
import org.mgba_emu.mgba.model.GameModel
import kotlin.concurrent.thread

object UriSerializer : KSerializer<Uri> {
    override val descriptor: SerialDescriptor = PrimitiveSerialDescriptor("Uri", PrimitiveKind.STRING)

    override fun serialize(encoder: Encoder, value: Uri) {
        encoder.encodeString(value.toString())
    }

    override fun deserialize(decoder: Decoder): Uri {
        return decoder.decodeString().toUri()
    }
}

object GameCacheManager {
    private val prefs = mGBAApplication.context.getSharedPreferences("games_cache", Context.MODE_PRIVATE)

    private val jsonConfig = Json {
        ignoreUnknownKeys = true
        coerceInputValues = true
    }

    fun getGame(uri: Uri, fileName: String): GameModel? {
        val jsonString = prefs.getString(uri.toString(), null) ?: return null

        return try {
            val cachedGame = jsonConfig.decodeFromString<GameModel>(jsonString)
            cachedGame.copy(fileName = fileName)
        } catch (_: Exception) {
            null
        }
    }

    fun saveGame(game: GameModel) {
        CoroutineScope(Dispatchers.IO).launch {
            try {
                val jsonString = jsonConfig.encodeToString(game)
                prefs.edit {
                    putString(game.uri.toString(), jsonString)
                }
            } catch (_: Exception) {}
        }
    }
}