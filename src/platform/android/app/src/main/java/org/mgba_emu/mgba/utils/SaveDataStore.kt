
package org.mgba_emu.mgba.utils

import org.mgba_emu.mgba.mGBAApplication
import org.mgba_emu.mgba.utils.FileUtils.safeReadBytes
import org.mgba_emu.mgba.utils.FileUtils.writeBytesAtomically
import java.io.File

object SaveDataStore {
    private val saveDir: File by lazy {
        File(mGBAApplication.context.getExternalFilesDir(null), "saves").apply { mkdirs() }
    }

    private fun fileFor(fileName: String): File {
        return File(saveDir, "$fileName.sav")
    }

    fun load(fileName: String): ByteArray {
        return fileFor(fileName).safeReadBytes()
    }

    fun save(fileName: String, saveBytes: ByteArray): Boolean {
        return fileFor(fileName).writeBytesAtomically(saveBytes)
    }
}