/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */


package org.mgba_emu.mgba.viewmodel

import android.content.Context
import android.net.Uri
import android.provider.DocumentsContract
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import org.mgba_emu.mgba.core.Core
import org.mgba_emu.mgba.mGBAApplication
import org.mgba_emu.mgba.model.GameModel
import org.mgba_emu.mgba.utils.GameCacheManager
import org.mgba_emu.mgba.utils.IconMetadataHelper.getIconUrl
import org.mgba_emu.mgba.utils.SearchLocationHelper

class GamesViewModel : ViewModel() {
    private val _gameList = MutableStateFlow<List<GameModel>>(emptyList())
    val gameList: StateFlow<List<GameModel>> = _gameList

    private val _isLoading = MutableStateFlow(false)
    val isLoading: StateFlow<Boolean> = _isLoading

    private val coreMutex = Mutex()

    private val supportedExtensions = setOf("gb", "gba", "gbc", "sgb") // TODO: zip?

    init {
        loadRoms()
    }

    fun loadRoms() {
        if (_isLoading.value) return
        if (!Core.init()) return
        viewModelScope.launch(Dispatchers.IO) {
            _isLoading.value = true

            val context = mGBAApplication.context
            val folderUris = SearchLocationHelper.getGameFolders()

            val searchJobs = folderUris.map { folderUri ->
                async { searchRoms(context, folderUri) }
            }

            val allFoundFiles = searchJobs.awaitAll().flatten()

            val gameModels = allFoundFiles.mapNotNull { (fileUri, fileName) ->
                coreMutex.withLock {
                    GameCacheManager.getGame(fileUri, fileName) ?: if (Core.validateRom(fileUri)) {
                        val platform = Core.getPlatform()
                        val title = Core.gameTitle()

                         val game = GameModel(
                            uri = fileUri,
                            fileName = fileName,
                            code = Core.gameCode(),
                            iconUrl = getIconUrl(title, platform),
                            platform = platform,
                            title = title,
                            version = Core.gameVersion
                        )

                        GameCacheManager.saveGame(game)
                        game
                    } else {
                        null
                    }
                }
            }

            _gameList.value = gameModels
            _isLoading.value = false
        }
    }

    private suspend fun searchRoms(context: Context, treeUri: Uri): List<Pair<Uri, String>> =
        withContext(Dispatchers.IO) {
            val result = mutableListOf<Pair<Uri, String>>()
            val resolver = context.contentResolver

            fun traverse(directoryUri: Uri) {
                try {
                    val childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(
                        treeUri,
                        DocumentsContract.getDocumentId(directoryUri)
                    )

                    val projection = arrayOf(
                        DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                        DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                        DocumentsContract.Document.COLUMN_MIME_TYPE
                    )

                    resolver.query(childrenUri, projection, null, null, null)?.use { cursor ->
                        val idIndex = cursor.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_DOCUMENT_ID)
                        val nameIndex = cursor.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_DISPLAY_NAME)
                        val mimeIndex = cursor.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_MIME_TYPE)

                        while (cursor.moveToNext()) {
                            val docId = cursor.getString(idIndex)
                            val name = cursor.getString(nameIndex) ?: continue
                            val mime = cursor.getString(mimeIndex)
                            val childUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, docId)

                            if (mime == DocumentsContract.Document.MIME_TYPE_DIR) {
                                traverse(childUri) // Recurse into subdirectories
                            } else {
                                val extension = name.substringAfterLast('.', "").lowercase()
                                if (supportedExtensions.contains(extension)) {
                                    result.add(Pair(childUri, name))
                                }
                            }
                        }
                    }
                } catch (e: Exception) {
                    e.printStackTrace()
                }
            }

            try {
                val rootDocUri = DocumentsContract.buildDocumentUriUsingTree(
                    treeUri,
                    DocumentsContract.getTreeDocumentId(treeUri)
                )
                traverse(rootDocUri)
            } catch (e: Exception) {
                e.printStackTrace()
            }

            return@withContext result
        }
}
