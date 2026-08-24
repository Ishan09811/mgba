plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.serialization)
}

android {
    namespace = "org.mgba_emu.mgba"
    compileSdk {
        version = release(37) {
            minorApiLevel = 1
        }
    }

    defaultConfig {
        applicationId = "org.mgba_emu.mgba"
        minSdk = 26
        targetSdk = 37
        versionCode = 1
        versionName = "1.0"

        ndk {
            abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86_64")
        }

        externalNativeBuild {
            cmake {
                cppFlags += "-std=c++17"
                arguments(
                    "-DANDROID_STL=c++_shared"
                )
            }
        }

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildTypes {
        release {
            optimization {
                enable = false
            }
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    buildFeatures {
        viewBinding = true
        prefab = true
    }
}

kotlin {
    compilerOptions {
        jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17)
    }
}

tasks.register<Copy>("copyNoIntroDat") {
    description = "copying nointro.dat file to assets"
    val sourceFile = layout.projectDirectory.file("../../../../res/nointro.dat")
    val destinationDir = layout.projectDirectory.dir("src/main/assets")

    from(sourceFile)
    into(destinationDir)
}

tasks.named("preBuild") {
    dependsOn("copyNoIntroDat")
}

dependencies {
    implementation(libs.androidx.appcompat)
    implementation(libs.androidx.core.ktx)
    implementation(libs.material)
    implementation(libs.androidx.navigation.fragment.ktx)
    implementation(libs.androidx.navigation.ui.ktx)
    implementation(libs.androidx.documentfile)
    implementation(libs.coil)
    implementation(libs.coil.network.okhttp)
    implementation(libs.oboe)
    implementation(libs.kotlinx.serialization.json)
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    androidTestImplementation(libs.androidx.junit)
}