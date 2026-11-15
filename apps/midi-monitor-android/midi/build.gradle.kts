// The MIDI module: plain Kotlin with zero Android dependencies.
// Because nothing here can touch Bluetooth or the UI, the parser is forced to be
// "bytes in, messages out", which is exactly the shape the ESP32 firmware needs.
plugins {
    alias(libs.plugins.kotlin.jvm)
}

// Gradle itself runs on Android Studio's JDK 25, but the bytecode is targeted at
// Java 17 so that the Android module can consume it.
java {
    sourceCompatibility = JavaVersion.VERSION_17
    targetCompatibility = JavaVersion.VERSION_17
}

kotlin {
    compilerOptions {
        jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17)
    }
}

dependencies {
    testImplementation(libs.junit)
    testImplementation(libs.kotlin.test.junit)
}

tasks.test {
    useJUnit()
    testLogging {
        events("passed", "failed", "skipped")
        exceptionFormat = org.gradle.api.tasks.testing.logging.TestExceptionFormat.FULL
    }
}
