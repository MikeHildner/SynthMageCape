// settings.gradle.kts is the first file Gradle reads. It answers two questions:
// where do build plugins and libraries come from, and which modules make up the project.

pluginManagement {
    // Where Gradle looks for build plugins (the Android Gradle plugin, the Compose compiler plugin).
    repositories {
        google {
            content {
                // Only ask Google's repository for the groups it actually hosts.
                includeGroupByRegex("com[.]android.*")
                includeGroupByRegex("com[.]google.*")
                includeGroupByRegex("androidx.*")
            }
        }
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    // Where Gradle looks for the libraries the app itself uses.
    // FAIL_ON_PROJECT_REPOS: modules may not add repositories of their own, so this is the only list.
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "MidiMonitor"

// Two modules, on purpose:
//  :midi  - pure Kotlin, no Android. Parses BLE MIDI packets into MIDI messages.
//           Tested on the PC with plain JVM unit tests; the design ports to the ESP32 firmware.
//  :app   - the Android application (Jetpack Compose UI, Bluetooth).
include(":midi")
include(":app")
