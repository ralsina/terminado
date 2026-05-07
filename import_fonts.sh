#!/bin/bash -x

FONTSIZE=7

fontconvert /usr/share/fonts/TTF/IosevkaNerdFontMono-Regular.ttf $FONTSIZE 32 256 > iosekva.h
fontconvert /usr/share/fonts/TTF/IosevkaNerdFontMono-Bold.ttf $FONTSIZE 32 256 > iosekva_bold.h
fontconvert /usr/share/fonts/TTF/IosevkaNerdFontMono-Italic.ttf $FONTSIZE 32 256 > iosekva_italic.h
fontconvert /usr/share/fonts/TTF/IosevkaNerdFontMono-BoldItalic.ttf $FONTSIZE 32 256 > iosekva_bolditalic.h
