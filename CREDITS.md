# Credits and acknowledgements

This iOS port is an adaptation of [Carlox33's Android port](https://github.com/Carlox33/The-Simpsons-Hit-and-Run-Android). Credit for that foundation belongs to Carlox33, including the mobile engine adaptations, integrated touch controls, touch icon design and controller work inherited by this project. The iOS work builds on those contributions; it does not claim sole authorship of the port.

The Android project's original acknowledgements credit [ZenoArrows](https://github.com/ZenoArrows/The-Simpsons-Hit-and-Run) for the Nintendo Switch and PS Vita ports on which it was based. The upstream README and multilingual documentation are preserved in [docs/android-history](docs/android-history/README.md).

Thanks to the original *The Simpsons: Hit & Run* development team at Radical Entertainment, and to the community contributors to the engine and platform ports.

## Third-party components

- [SDL2](https://github.com/libsdl-org/SDL): platform, input and application support; [license](libs/SDL2/LICENSE.txt).
- [FFmpeg](https://ffmpeg.org/): movie playback; [license information](libs/ffmpeg/LICENSE.md).
- [OpenAL Soft](https://github.com/kcat/openal-soft): audio; license files are in [libs/openal-soft](libs/openal-soft).
- [MGL](https://github.com/openglonmetal/MGL): OpenGL-on-Metal translation, with glslang, SPIRV-Tools and SPIRV-Cross; local integration and dependency details are in [mgl-ios/README](mgl-ios/README).
- Other bundled components retain their copyright and license notices in [libs](libs).

All original game content and trademarks remain the property of their respective owners. Game files must be supplied by the player.
