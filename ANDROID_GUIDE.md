# Android Run Guide (Beginner / Dummy App)

This repo now includes a ready dummy Android Studio project at:

`android-dummy/`

You only need to build the native lib and sync it.

## 0) Requirements

- Android Studio
- Android SDK + NDK installed (SDK Manager)
- Android device (recommended) or emulator

## 1) Build Android native library

From project root:

```bash
make -f makefile.android android-build
```

Output file:

```text
build-android/libmain.so
```

## 2) Sync native lib into dummy app

From project root:

```bash
./android-dummy/sync_native_lib.sh
```

This copies to:

```text
android-dummy/app/src/main/jniLibs/arm64-v8a/libmain.so
```

## 3) Open and run in Android Studio

1. Open Android Studio.
2. Click Open.
3. Select folder `android-dummy/`.
4. Wait for Gradle sync.
5. Select your device/emulator.
6. Press Run.

## 4) Where to add PNG icon files

Put PNG launcher icons in:

- `android-dummy/app/src/main/res/mipmap-mdpi/ic_launcher.png`
- `android-dummy/app/src/main/res/mipmap-hdpi/ic_launcher.png`
- `android-dummy/app/src/main/res/mipmap-xhdpi/ic_launcher.png`
- `android-dummy/app/src/main/res/mipmap-xxhdpi/ic_launcher.png`
- `android-dummy/app/src/main/res/mipmap-xxxhdpi/ic_launcher.png`

Optional round icons:

- `android-dummy/app/src/main/res/mipmap-mdpi/ic_launcher_round.png`
- `android-dummy/app/src/main/res/mipmap-hdpi/ic_launcher_round.png`
- `android-dummy/app/src/main/res/mipmap-xhdpi/ic_launcher_round.png`
- `android-dummy/app/src/main/res/mipmap-xxhdpi/ic_launcher_round.png`
- `android-dummy/app/src/main/res/mipmap-xxxhdpi/ic_launcher_round.png`

After adding custom icons, update app manifest icon references in:

`android-dummy/app/src/main/AndroidManifest.xml`

Change from system icon to mipmap icons:

- `android:icon="@mipmap/ic_launcher"`
- `android:roundIcon="@mipmap/ic_launcher_round"`

## 5) Android in-game controls

- Left pad: movement
- CAST: hold/release cast
- LIFE: lifespan charge
- JUMP: jump
- Top row: PAUSE, CRAFT, LAYER, MENU

## 6) Common issues

### App closes instantly

Do this full refresh sequence first:

1. Rebuild native lib: `make -f makefile.android android-build`
2. Re-sync native lib: `./android-dummy/sync_native_lib.sh`
3. In Android Studio: **Build > Clean Project**
4. Uninstall old app from phone/emulator
5. Run again from Android Studio

- Confirm `android-dummy/app/src/main/jniLibs/arm64-v8a/libmain.so` exists.
- Confirm manifest meta-data uses `android:value="main"`.

If it still crashes, capture logs:

```bash
adb logcat | grep -E "AndroidRuntime|libmain|NativeActivity|FATAL"
```

### Emulator ABI mismatch

For `x86_64` emulator:

```bash
make -f makefile.android android-clean
make -f makefile.android android-build ANDROID_ABI=x86_64
./android-dummy/sync_native_lib.sh x86_64
```

### NDK not found

Set either:

```bash
export ANDROID_NDK_HOME=/path/to/ndk
# or
export ANDROID_NDK_ROOT=/path/to/ndk
```
