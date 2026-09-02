/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */


package org.mgba_emu.mgba.utils

import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.provider.DocumentsContract
import android.util.Log
import org.mgba_emu.mgba.providers.AppDataDocumentProvider
import java.io.File
import java.io.IOException

object FileUtils {
    const val LOG_TAG = "FileUtils"

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

    fun launchInternalDir(ctx: Context): Boolean {
        if (!ctx.launchBrowseIntent(Intent.ACTION_VIEW)) {
            if (!ctx.launchBrowseIntent()) {
                if (!ctx.launchBrowseIntent(Intent.ACTION_OPEN_DOCUMENT_TREE)) {
                    return false
                }
            }
        }
        return true
    }

    private fun Context.launchBrowseIntent(
        action: String = "android.provider.action.BROWSE"
    ): Boolean {
        return try {
            val intent = Intent(action).apply {
                addCategory(Intent.CATEGORY_DEFAULT)
                data = DocumentsContract.buildRootUri(
                    AppDataDocumentProvider.AUTHORITY, AppDataDocumentProvider.ROOT_ID
                )
                addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION or Intent.FLAG_GRANT_PREFIX_URI_PERMISSION or Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION)
            }
            startActivity(intent)
            true
        } catch (_: ActivityNotFoundException) {
            Log.e(LOG_TAG, "No activity found to handle $action intent")
            false
        }
    }
}