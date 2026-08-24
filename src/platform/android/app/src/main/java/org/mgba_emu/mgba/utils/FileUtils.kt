
package org.mgba_emu.mgba.utils

import java.io.File
import java.io.IOException

object FileUtils {
    fun File.writeBytesAtomically(bytes: ByteArray): Boolean {
        if (bytes.isEmpty()) return true
        val tempFile = File(parentFile, "$name.tmp")

        return try {
            tempFile.writeBytes(bytes)
            tempFile.renameTo(this)
        } catch (_: IOException) {
            if (tempFile.exists()) tempFile.delete()
            false
        }
    }

    fun File.safeReadBytes(): ByteArray {
        if (!this.exists()) return ByteArray(0)
        return try {
            this.readBytes()
        } catch (_: IOException) {
            ByteArray(0)
        }
    }
}