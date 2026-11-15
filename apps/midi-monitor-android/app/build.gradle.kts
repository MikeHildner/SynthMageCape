plugins {
    // Builds an installable Android app (an APK). Also compiles the Kotlin sources.
    alias(libs.plugins.android.application)
    // Kotlin compiler plugin that turns @Composable functions into Compose UI code.
    alias(libs.plugins.kotlin.compose)
}

android {
    // The Kotlin/Java package for generated code such as the R class.
    namespace = "com.example.midimonitor"

    // The SDK version the code is compiled against: which APIs the compiler can see.
    compileSdk = 36

    defaultConfig {
        // The app's unique identity on a phone and in a store.
        applicationId = "com.example.midimonitor"

        // Oldest Android version the app installs on. 33 is Android 13.
        //  - Android 12 (31) introduced the BLUETOOTH_SCAN and BLUETOOTH_CONNECT
        //    permissions, so there is no location permission to deal with.
        //  - Android 13 (33) replaced the Bluetooth GATT calls that passed data
        //    through a shared mutable field with ones that pass it as a parameter.
        //    Starting at 33 means using only the new calls.
        minSdk = 33

        // The Android version the app has been tested against; the system switches
        // off compatibility behaviours up to this level.
        targetSdk = 36

        versionCode = 1
        versionName = "0.1.0"
    }

    buildFeatures {
        compose = true
    }
}

dependencies {
    // The packet parser and MIDI message types.
    implementation(project(":midi"))

    // ComponentActivity and setContent { }, the bridge from an Activity into Compose.
    implementation(libs.androidx.activity.compose)

    // viewModel(): gives a composable a ViewModel that survives screen rotation.
    implementation(libs.androidx.lifecycle.viewmodel.compose)
    // collectAsStateWithLifecycle(): turns a Flow into Compose state, and stops
    // collecting while the app is in the background.
    implementation(libs.androidx.lifecycle.runtime.compose)

    // platform(...) imports the BOM, which supplies the versions for the two lines below it.
    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.compose.ui)
    implementation(libs.androidx.compose.material3)
}
