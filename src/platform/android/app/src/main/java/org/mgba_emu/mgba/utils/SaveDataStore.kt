
package org.mgba_emu.mgba.utils

import org.mgba_emu.mgba.mGBAApplication
import org.mgba_emu.mgba.utils.FileUtils.safeReadBytes
import org.mgba_emu.mgba.utils.FileUtils.writeBytesAtomically
import java.io.File

object SaveDataStore {
    private val saveDir: File by lazy {
        File(mGBAApplication.context.getExternalFilesDir(null), "saves").apply { mkdirs() }
    }

    private fun fileFor(gameCode: String): File {
        val safeCode = gameCode.filter { it.isLetterOrDigit() }.ifEmpty { "UNKNOWN" }
        return File(saveDir, "$safeCode.sav")
    }

    fun load(gameCode: String): ByteArray {
        return fileFor(gameCode).safeReadBytes()
    }

    fun save(gameCode: String, saveBytes: ByteArray): Boolean {
        return fileFor(gameCode).writeBytesAtomically(saveBytes)
    }
}