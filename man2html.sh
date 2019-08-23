#!/bin/sh

hostname="http://termsend.bofc.pl"
out="$(pwd)/www"
root="$(pwd)"
ftmp="/tmp/termsend-man2html"


m="termsend.1"
n="1"
man2html -r -H "${hostname}" "${m}" > "${ftmp}"

# get only body part of the file
body_only="$(sed -n '/<BODY>/,/<\/BODY>/p' "${ftmp}")"
echo "$body_only" > "${ftmp}"

# remove leftover <body> and <h1>man</h1> tags from beginning
tail -n+3 "${ftmp}" > tmp; mv tmp "${ftmp}"

# construct top page heading with page info, remove superflous info
name="$(basename ${m})"
name="${name%.*}"
version_info="$(head -n1 ${ftmp} | cut -f3 -d: | cut -f1 -d\<)"
tail -n+2 "${ftmp}" > tmp; mv tmp "${ftmp}"
sed -i "1s/^/<p class=\"info left\">${name}(${n})<\/p><p class=\"info center\">bofc manual pages<\/p><p class=\"info right\">${name}(${n})<\/p>\n<br><P> /" "${ftmp}"

# remove uneeded links to non-existing index
sed -i 's/<A HREF="\.\.\/index.html">Return to Main Contents<\/A><HR>//' "${ftmp}"
sed -i 's/<A HREF="#index">Index<\/A>//g' "${ftmp}"

# extract table of content and put it in the beginning of file
## cache first two lines (page info) and remove them from file
tmp="$(head -n2 ${ftmp})"
tail -n+3 "${ftmp}" > tmp; mv tmp "${ftmp}"

## get table of content from file
toc="$(sed -n '/<DL>/,/<\/DL>/p' "${ftmp}")"

toc="$(echo "${toc}" | sed 's/<DL>/<UL class="man-toc">/')"
toc="$(echo "${toc}" | sed 's/<\/DL>/<\/UL>/')"
toc="$(echo "${toc}" | sed 's/<DT>/<LI>/')"
toc="$(echo "${toc}" | sed 's/<DD>/<\/LI>/')"

## put table of content and first two lines into file and append hr
{ echo -e "${tmp}\n${toc}\n<HR>"; cat "${ftmp}"; } > tmp; mv tmp "${ftmp}"

## remove table of content and some uneeded info from bottom of file
sed -i '/^<A NAME="index">&nbsp;<\/A><H2>Index<\/H2>$/,$d' "${ftmp}"
head -n-3 "${ftmp}" > tmp; mv tmp "${ftmp}"

# change deprecated name in <a> into id
sed -i 's/A NAME="/A ID="/g' "${ftmp}"

# generate page info at bottom of page
echo "</DL><p class=\"info left\"><a href=\"http://en.bofc.pl\">bofc.pl</a></p><p class=\"info center\">${version_info}</p><p class=\"info right\">${name}(${n})</p>" >> "${ftmp}"

# convert all h2 into h1 headings
sed -i 's/H2>/H1>/g' "${ftmp}"

# remove obsolete COMPACT from dl
sed -i 's/DL COMPACT/DL/g' "${ftmp}"

# move generated file into output directory for further processing
cp "${ftmp}" "${out}/${m}.html"
