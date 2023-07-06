package org.jzvd.jzplayer

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.view.Surface
import android.view.View
import android.widget.TextView
import org.jzvd.jzplayer.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    var surface: Surface? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // Example of a call to a native method
        binding.sampleText.text = stringFromJNI()
    }

    fun onclick_startbtn(view: View) {
        surface = Surface(binding.textureView.surfaceTexture)
        Thread {
            start(surface)
        }.start()
    }

    /**
     * A native method that is implemented by the 'jzplayer' native library,
     * which is packaged with this application.
     */
    external fun stringFromJNI(): String
    external fun start(surface: Surface?)

    companion object {
        // Used to load the 'jzplayer' library on application startup.
        init {
            System.loadLibrary("jzplayer")
        }
    }

}