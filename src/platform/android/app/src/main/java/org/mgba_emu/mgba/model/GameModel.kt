/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */


package org.mgba_emu.mgba.model

import android.net.Uri
import android.os.Parcelable
import kotlinx.parcelize.Parcelize
import kotlinx.serialization.Serializable
import org.mgba_emu.mgba.core.Platform
import org.mgba_emu.mgba.utils.UriSerializer

@Parcelize
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
) : Parcelable {
    companion object {
        val launchId = "game"
    }
}