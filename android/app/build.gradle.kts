plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

android {
    namespace = "net.shadps4.android"
    compileSdk = 35

    defaultConfig {
        applicationId = "net.shadps4.android"
        minSdk = 28
        targetSdk = 35
        versionCode = 1
        versionName = "0.1-devicereport"
        ndk { abiFilters += "arm64-v8a" }
    }

    // libshadps4.so is built separately by the top level CMake, then dropped into jniLibs. Driving
    // it through externalNativeBuild instead would mean Gradle owning a build that also has to run
    // on desktop, and would rebuild FFmpeg and FEXCore per variant.
    sourceSets["main"].jniLibs.srcDirs("src/main/jniLibs")

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    // The native library is already stripped; leave it alone.
    packaging {
        jniLibs.keepDebugSymbols += "**/libshadps4.so"
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }
    buildFeatures { compose = true }
}

dependencies {
    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.activity:activity-compose:1.9.3")
    implementation(platform("androidx.compose:compose-bom:2024.10.01"))
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.ui:ui")
}
