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
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val report = buildReport()

        setContent {
            MaterialTheme {
                Scaffold { padding ->
                    Column(
                        modifier = Modifier
                            .fillMaxSize()
                            .padding(padding)
                            .padding(16.dp)
                            .verticalScroll(rememberScrollState()),
                        verticalArrangement = Arrangement.spacedBy(12.dp),
                    ) {
                        Text(
                            text = "shadPS4 device report",
                            style = MaterialTheme.typography.titleLarge,
                        )
                        Text(
                            text = report,
                            fontFamily = FontFamily.Monospace,
                            style = MaterialTheme.typography.bodySmall,
                        )
                        CopyButton(report)
                    }
                }
            }
        }
    }

    /**
     * The device section is gathered in Kotlin so that it is still reported when the native
     * library fails to load -- which is itself the most important thing this app can tell us.
     */
    private fun buildReport(): String = buildString {
        appendLine("Device:  ${Build.MANUFACTURER} ${Build.MODEL}")
        // SOC_MANUFACTURER and SOC_MODEL only exist from API 31; reading them below that throws.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            appendLine("SoC:     ${Build.SOC_MANUFACTURER} ${Build.SOC_MODEL}")
        }
        appendLine("Android: ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})")
        appendLine("ABIs:    ${Build.SUPPORTED_ABIS.joinToString()}")
        appendLine()

        NativeBridge.load().fold(
            onSuccess = {
                appendLine("libshadps4.so loaded OK")
                appendLine()
                append(
                    runCatching { NativeBridge.deviceReport(filesDir.absolutePath) }
                        .getOrElse { "deviceReport threw: $it" }
                )
            },
            onFailure = { error ->
                appendLine("libshadps4.so FAILED TO LOAD")
                appendLine(error.toString())
                appendLine()
                appendLine("The native port does not run on this device. The message above says")
                appendLine("why -- a missing dependency, a bad ELF, or a rejected mapping.")
            },
        )
    }
}

@Composable
private fun CopyButton(report: String) {
    val context = androidx.compose.ui.platform.LocalContext.current
    val clipboard = remember {
        context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
    }
    Button(onClick = { clipboard.setPrimaryClip(ClipData.newPlainText("shadPS4 report", report)) }) {
        Text("Copy report")
    }
}
