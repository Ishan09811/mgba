/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */


package org.mgba_emu.mgba.fragments

import android.content.Intent
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.fragment.app.viewModels
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.LinearLayoutManager
import kotlinx.coroutines.launch
import org.mgba_emu.mgba.EmulationActivity
import org.mgba_emu.mgba.adapters.GameAdapter
import org.mgba_emu.mgba.databinding.FragmentRecentBinding
import org.mgba_emu.mgba.model.GameModel
import org.mgba_emu.mgba.utils.GameCacheManager
import org.mgba_emu.mgba.utils.applySafePadding
import org.mgba_emu.mgba.viewmodel.GamesViewModel

class RecentFragment : Fragment() {

    private var _binding: FragmentRecentBinding? = null
    private val binding get() = _binding!!

    private lateinit var recentAdapter: GameAdapter
    private val viewModel: GamesViewModel by viewModels()

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        if (_binding == null) _binding = FragmentRecentBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        recentAdapter = GameAdapter { game ->
            launchEmulationActivity(game)
        }

        binding.root.applySafePadding()
        binding.recentRecyclerView.layoutManager = LinearLayoutManager(requireContext())
        binding.recentRecyclerView.adapter = recentAdapter

        viewLifecycleOwner.lifecycleScope.launch {
            viewModel.gameList.collect { gamesList ->
                recentAdapter.submitList(
                    gamesList.filter { it.lastPlayed > 0L }.sortedByDescending { it.lastPlayed }
                )
            }
        }
    }

    override fun onResume() {
        super.onResume()
        viewModel.loadRoms()
    }

    private fun launchEmulationActivity(game: GameModel) {
        game.lastPlayed = System.currentTimeMillis()

        GameCacheManager.saveGame(game)

        val intent = Intent(requireContext(), EmulationActivity::class.java).apply {
            putExtra(GameModel.launchId, game)
        }
        startActivity(intent)
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}