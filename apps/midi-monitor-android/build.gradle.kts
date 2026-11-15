// Top-level build file. Plugins are declared here with `apply false`: that pins one
// version for the whole project without applying the plugin to the root itself.
// Each module then opts in to the plugins it needs.
plugins {
    alias(libs.plugins.android.application) apply false
    alias(libs.plugins.kotlin.jvm) apply false
    alias(libs.plugins.kotlin.compose) apply false
}
