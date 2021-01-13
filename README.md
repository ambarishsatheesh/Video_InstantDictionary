# Video_InstantDictionary

A program that allows video playback with subtitles and features live English dictionary lookup of words in the subtitles (from a subtitle file, e.g. .srt file). I made this for my dad so he could learn the meaning of difficult English words in the movies and TV shows he watched. 

![screenshot1](https://github.com/ambarishsatheesh/Video_InstantDictionary/blob/master/images/screenshot1.png)


_**Debug mode (CONFIG += debug) is required even when compiling a release version, otherwise subtitles aren't drawn. This is a bug and I have not looked into the cause yet.**_

## Executable/Feature Requisites and Issues

### Linux
The source code should compile as is on Linux (tested on Linut Mint 20 and Ubuntu 20.04.1 LTS) and the executable should have access to all features.

### Windows:
The source code should compile as is on Windows (tested with Windows 10 64-bit using MinGW 8.1.0 64-bit). However, you might find that some features do not work/ crash the application:
* QMediaPlayer can fail to play some/all video formats (e.g. .mp4, .mkv). To fix this, you need to install the [K-Lite Codec Pack](https://codecguide.com/download_kl.htm). 
You can find the version I used (15.7.0 Basic) in the requisites folder in the source.

* To access live definitions, you need the OpenSSL 1.1.1d binaries

  Install the toolkit via Qt's online installer/ Maintenance Tool, or install Win32 OpenSSL from another source. I have provided the 64-bit binaries in the requisites folder in the source.
  
  After this, compile via qmake and do either one of the following:
  * Add C:\Qt\Tools\OpenSSL\Win_x64\bin (or \Win_x86 for 32-bit) to your PATH environment variable
  * Place libssl-1_1-x64.dll and libcrypto-1_1-x64.dll (or \*x86.dlls) where the compiled executable is located.
