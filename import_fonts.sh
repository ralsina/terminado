#!/bin/bash -x

FONTSIZE=7

fontconvert /usr/share/fonts/TTF/IosevkaNerdFontMono-Regular.ttf $FONTSIZE 32 256 > iosevka.h
fontconvert /usr/share/fonts/TTF/IosevkaNerdFontMono-Bold.ttf $FONTSIZE 32 256 > iosevka_bold.h
fontconvert /usr/share/fonts/TTF/IosevkaNerdFontMono-Italic.ttf $FONTSIZE 32 256 > iosevka_italic.h
fontconvert /usr/share/fonts/TTF/IosevkaNerdFontMono-BoldItalic.ttf $FONTSIZE 32 256 > iosevka_bolditalic.h
