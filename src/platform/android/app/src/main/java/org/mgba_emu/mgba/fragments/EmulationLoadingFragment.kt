/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

package org.mgba_emu.mgba.fragments

import android.annotation.SuppressLint
import android.graphics.Bitmap
import android.graphics.RenderEffect
import android.graphics.Shader
import android.os.Build
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import coil3.ImageLoader
import coil3.load
import coil3.request.ImageRequest
import coil3.request.crossfade
import coil3.request.error
import coil3.request.fallback
import coil3.toBitmap
import com.google.android.renderscript.Toolkit
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.mgba_emu.mgba.R
import org.mgba_emu.mgba.databinding.EmulationLoadingBinding
import org.mgba_emu.mgba.model.GameModel

private const val ProgressTag = "EmulationLoadingFragment::Progress"

class EmulationLoadingFragment : Fragment() {
    private val game by lazy { requireArguments().getParcelable<GameModel>(GameModel.launchId) }

    private lateinit var binding : EmulationLoadingBinding

    override fun onCreateView(inflater : LayoutInflater, container : ViewGroup?, savedInstanceState : Bundle?) = EmulationLoadingBinding.inflate(inflater).also { binding = it }.root

    @SuppressLint("SetTextI18n")
    override fun onViewCreated(view : View, savedInstanceState : Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        binding.gameTitle.apply {
            text = game?.title ?: game?.fileName ?: ""
            isSelected = true
        }

        binding.gameVersion.text = game?.platform?.name ?: ""
        binding.gameIcon.load(data = game?.iconUrl ?: "") {
            crossfade(true)
            fallback(R.mipmap.ic_launcher)
            error(R.mipmap.ic_launcher)
        }

        val progress = savedInstanceState?.getString(ProgressTag) ?: ""
        binding.progressBar.isIndeterminate = true
        updateProgress(progress)

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            binding.backgroundImage.load(data = game?.iconUrl ?: "") {
                crossfade(true)
                fallback(R.mipmap.ic_launcher)
                error(R.mipmap.ic_launcher)
            }
            binding.backgroundImage.setRenderEffect(RenderEffect.createBlurEffect(75f, 75f, Shader.TileMode.MIRROR))
        } else {
            CoroutineScope(Dispatchers.IO).launch {
                val loader = ImageLoader(requireContext())
                val request = ImageRequest.Builder(requireContext())
                    .data(game?.iconUrl ?: "")
                    .fallback(R.mipmap.ic_launcher)
                    .error(R.mipmap.ic_launcher)
                    .build()

                val result = loader.execute(request)
                val bitmap = result.image?.toBitmap()?.copy(Bitmap.Config.ARGB_8888, false)
                bitmap?.let {
                    withContext(Dispatchers.Main) {
                        binding.backgroundImage.setImageBitmap(Toolkit.blur(bitmap, 15))
                    }
                }
            }
        }
    }

    override fun onSaveInstanceState(outState : Bundle) {
        super.onSaveInstanceState(outState)
        outState.putString(ProgressTag, binding.progressLabel.text.toString())
    }

    @SuppressLint("SetTextI18n")
    fun updateProgress(progress: String) {
        if (!this::binding.isInitialized) return
        binding.progressLabel.text = progress
    }

    companion object {
        fun newInstance(game : GameModel) = EmulationLoadingFragment().apply {
            arguments = Bundle().apply {
                putParcelable(GameModel.launchId, game)
            }
        }
    }
}