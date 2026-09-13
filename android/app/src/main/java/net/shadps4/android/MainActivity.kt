// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package net.shadps4.android

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/**
 * Deliberately renders before touching native code.
 *
 * libshadps4.so is the whole emulator, so loading it runs every static initialiser in it. If one
 * of those aborts, the process dies -- and doing that from onCreate() before setContent() meant
 * the app died with no window at all, which looks identical to the app not launching. Now the UI
 * is up first and the native work happens afterwards, off the main thread, so a failure is
 * visible rather than silent.
 */
class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme {
                Scaffold { padding ->
                    ReportScreen(Modifier.padding(padding))
                }
            }
        }
    }
}

@Composable
private fun ReportScreen(modifier: Modifier = Modifier) {
    val context = LocalContext.current
    var status by remember { mutableStateOf("Loading native library...") }
    var report by remember { mutableStateOf<String?>(null) }

    LaunchedEffect(Unit) {
        val text = StringBuilder(deviceSection())

        // The probe first and separately: it links none of the emulator, so it answers even when
        // the emulator library does not load.
        val probe = withContext(Dispatchers.IO) { NativeBridge.loadProbe() }
        probe.fold(
            onSuccess = {
                val measured = withContext(Dispatchers.IO) {
                    runCatching { NativeBridge.deviceReport() }.getOrElse { "deviceReport() threw: " + it }
                }
                text.appendLine(measured)
            },
            onFailure = { text.appendLine("libshadps4_probe.so failed to load: $it") },
        )

        text.appendLine("----------------------------------------")
        status = "Loading the emulator library..."

        val emulator = withContext(Dispatchers.IO) { NativeBridge.loadEmulator() }
        emulator.fold(
            onSuccess = {
                val info = withContext(Dispatchers.IO) {
                    runCatching { NativeBridge.emulatorInfo(context.filesDir.absolutePath) }
                        .getOrElse { "emulatorInfo() threw: " + it }
                }
                text.appendLine("libshadps4.so loaded OK")
                text.appendLine()
                text.append(info)
            },
            onFailure = { error ->
                text.appendLine("libshadps4.so FAILED TO LOAD")
                text.appendLine()
                text.appendLine(error.toString())
                text.appendLine()
                text.appendLine("If the app instead disappears at this point, a static")
                text.appendLine("initialiser aborted and killed the process. Capture it with:")
                text.appendLine("  adb logcat -d | grep -iE 'shadps4|DEBUG|libc'")
            },
        )

        report = text.toString()
        status = "done"
    }

    Column(
        modifier = modifier
            .fillMaxSize()
            .padding(16.dp)
            .verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Text("shadPS4 device report", style = MaterialTheme.typography.titleLarge)

        val text = report
        if (text == null) {
            Text(status)
        } else {
            Text(text, fontFamily = FontFamily.Monospace, style = MaterialTheme.typography.bodySmall)
            CopyButton(text)
        }
    }
}

/** Gathered in Kotlin so it is still reported when the native library cannot be loaded. */
private fun deviceSection(): String = buildString {
    appendLine("Device:  ${Build.MANUFACTURER} ${Build.MODEL}")
    // SOC_MANUFACTURER and SOC_MODEL only exist from API 31; reading them below that throws.
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        appendLine("SoC:     ${Build.SOC_MANUFACTURER} ${Build.SOC_MODEL}")
    }
    appendLine("Android: ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})")
    appendLine("ABIs:    ${Build.SUPPORTED_ABIS.joinToString()}")
    appendLine()
}

@Composable
private fun CopyButton(report: String) {
    val context = LocalContext.current
    val clipboard = remember {
        context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
    }
    Button(onClick = { clipboard.setPrimaryClip(ClipData.newPlainText("shadPS4 report", report)) }) {
        Text("Copy report")
    }
}
