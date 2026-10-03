#!/bin/bash
# Troca a família azul do programa pelas cores da logo MercadoPro (verde #17963A / azul-marinho #142D45).
# Cores de status (vermelho, âmbar, verde de sucesso) ficam como estão.
cd "$(dirname "$0")/.."
ARQ=$(grep -rlEi "#[0-9a-f]{6}|rgba?\(" src --include=*.cpp --include=*.h --include=*.ui)
S='s/x/x/'
# hex (sem diferenciar maiúscula/minúscula)
for par in \
  "2B84BF:17963A" "1F6A9C:107A2E" "174F75:107A2E" "8CC4EC:8FD9A3" "D3E8F8:D4F0DA" "EAF3FB:EAF6EC" \
  "E3F0FA:E3F4E7" "DCE9F5:DDF1E1" "EAF0F7:EAF3EC" "0B2540:142D45" "1E3A5F:142D45" "16324F:142D45" \
  "1E40AF:107A2E" "F3F6FA:F2F7F4" "E9EEF5:E8F0EA" "0E6B24:0E6B24"; do
  de=${par%%:*}; para=${par##*:}
  sed -i -E "s/#$de\b/#$para/Ig" $ARQ
done
# rgb(...) com ou sem espaços: "r *, *g *, *b"
rgbtroca() { # r g b -> r2 g2 b2 (aceita rgb( e rgba( )
  sed -i -E "s/rgb(a?)\( *$1 *, *$2 *, *$3 *([,)])/rgb\1($4, $5, $6\7/Ig" $ARQ
}
sed -i -E 's/rgb(a?)\( *43 *, *132 *, *191 *([,)])/rgb\1(23, 150, 58\2/Ig' $ARQ
sed -i -E 's/rgb(a?)\( *33 *, *105 *, *149 *([,)])/rgb\1(16, 122, 46\2/Ig' $ARQ
sed -i -E 's/rgb(a?)\( *13 *, *92 *, *161 *([,)])/rgb\1(16, 122, 46\2/Ig' $ARQ
sed -i -E 's/rgb(a?)\( *0?3 *, *75 *, *119 *([,)])/rgb\1(11, 92, 34\2/Ig' $ARQ
sed -i -E 's/rgb(a?)\( *153 *, *193 *, *241 *([,)])/rgb\1(143, 217, 163\2/Ig' $ARQ
sed -i -E 's/rgb(a?)\( *195 *, *215 *, *235 *([,)])/rgb\1(197, 222, 208\2/Ig' $ARQ
sed -i -E 's/rgb(a?)\( *220 *, *235 *, *250 *([,)])/rgb\1(221, 241, 225\2/Ig' $ARQ
sed -i -E 's/rgb(a?)\( *30 *, *64 *, *175 *([,)])/rgb\1(16, 122, 46\2/Ig' $ARQ
sed -i -E 's/rgb(a?)\( *24 *, *42 *, *66 *([,)])/rgb\1(20, 45, 69\2/Ig' $ARQ
sed -i -E 's/rgb(a?)\( *30 *, *50 *, *80 *([,)])/rgb\1(20, 45, 69\2/Ig' $ARQ
sed -i -E 's/rgb(a?)\( *38 *, *62 *, *92 *([,)])/rgb\1(29, 56, 74\2/Ig' $ARQ
sed -i -E 's/rgb(a?)\( *40 *, *60 *, *80 *([,)])/rgb\1(29, 56, 74\2/Ig' $ARQ
echo "arquivos analisados: $(echo "$ARQ" | wc -l)"
