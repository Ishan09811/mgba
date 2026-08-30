package org.mgba_emu.mgba.preferences

import android.content.Context
import android.util.AttributeSet
import android.view.LayoutInflater
import android.widget.TextView
import androidx.constraintlayout.widget.ConstraintLayout
import org.mgba_emu.mgba.R
import org.mgba_emu.mgba.utils.FileUtils

class ViewInternalDirectoryPreference @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : ConstraintLayout(context, attrs, defStyleAttr) {

    init {
        LayoutInflater.from(context).inflate(R.layout.nav_preference, this, true)

        val titleView = findViewById<TextView>(R.id.title)
        val subtitleView = findViewById<TextView>(R.id.subtitle)

        context.obtainStyledAttributes(attrs, R.styleable.SwitchPreference, 0, 0).apply {
            titleView.text = getString(R.styleable.SwitchPreference_title)
            val subtitleText = getString(R.styleable.SwitchPreference_subtitle)
            if (subtitleText.isNullOrEmpty()) {
                subtitleView.visibility = GONE
            } else {
                subtitleView.text = subtitleText
            }
            recycle()
        }

        isClickable = true
        isFocusable = true

        setOnClickListener {
            FileUtils.launchInternalDir(context)
        }
    }
}