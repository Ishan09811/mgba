
package org.mgba_emu.mgba

import android.app.Application
import android.content.Context
import android.content.Intent
import org.mgba_emu.mgba.core.Core
import org.mgba_emu.mgba.services.LoggerService
import org.mgba_emu.mgba.utils.GlobalConfig

class mGBAApplication : Application() {
    init {
        instance = this
    }

    companion object {
        lateinit var instance : mGBAApplication
            private set

        val context : Context get() = instance.applicationContext
    }

    override fun onCreate() {
        super.onCreate()
        instance = this
        Core.initNoIntroDB(this)
        GlobalConfig.initialize(context)
        startService(Intent(this, LoggerService::class.java))
    }
}
