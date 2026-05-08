#!/bin/bash -x

for FONTSIZE in 4 6 7 10; do
    fontconvert /usr/share/fonts/TTF/IosevkaNerdFontMono-Regular.ttf    $FONTSIZE 32 256 > iosevka_regular_${FONTSIZE}pt.h
    fontconvert /usr/share/fonts/TTF/IosevkaNerdFontMono-Bold.ttf        $FONTSIZE 32 256 > iosevka_bold_${FONTSIZE}pt.h
    fontconvert /usr/share/fonts/TTF/IosevkaNerdFontMono-Italic.ttf      $FONTSIZE 32 256 > iosevka_italic_${FONTSIZE}pt.h
    fontconvert /usr/share/fonts/TTF/IosevkaNerdFontMono-BoldItalic.ttf  $FONTSIZE 32 256 > iosevka_bolditalic_${FONTSIZE}pt.h
done
